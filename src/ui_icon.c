/**
 * @file ui_icon.c
 * @brief General-purpose UI Icon System — implementation.
 *
 * Self-contained async texture cache for small PNG icons served by the
 * engine API.  Uses the WASM JS fetch bridge directly (same pattern as
 * dialogue_data.c and the atlas system in object_layers_management.c).
 *
 * See ui_icon.h for full documentation.
 */

#include "ui_icon.h"
#include "config.h"
#include <math.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

/* ── WASM JS bridge (shared with atlas / dialogue systems) ───────────── */

extern void           js_start_fetch_binary(const char* url, int request_id);
extern unsigned char*  js_get_fetch_result(int request_id, int* size);

/* ── Icon texture cache ─────────────────────────────────────────────── */

#define ICON_HASH_SIZE 64

typedef enum {
    ICON_NONE,
    ICON_LOADING,
    ICON_READY,
    ICON_ERROR
} IconState;

typedef struct IconEntry {
    char*             icon_id;   /* filename stem, e.g. "skull" */
    Texture2D         texture;
    IconState         state;
    int               request_id;
    struct IconEntry* next;
} IconEntry;

static IconEntry* s_buckets[ICON_HASH_SIZE];
static int        s_next_req_id = 30000; /* offset to avoid collision with
                                            TextureManager (1+) and
                                            dialogue_data (20000+) */

/* ── Hash helper ─────────────────────────────────────────────────────── */

static unsigned long hash_str(const char* s) {
    unsigned long h = 5381;
    while (*s) h = ((h << 5) + h) + (unsigned char)*s++;
    return h;
}

/* ── Internal lookup / create ────────────────────────────────────────── */

static IconEntry* lookup(const char* icon_id) {
    unsigned long idx = hash_str(icon_id) % ICON_HASH_SIZE;
    IconEntry* e = s_buckets[idx];
    while (e) {
        if (strcmp(e->icon_id, icon_id) == 0) return e;
        e = e->next;
    }
    return NULL;
}

/** Create a new entry and kick off the async HTTP fetch. */
static IconEntry* create_and_fetch(const char* icon_id) {
    IconEntry* e = (IconEntry*)calloc(1, sizeof(IconEntry));
    if (!e) return NULL;

    e->icon_id = strdup(icon_id);
    if (!e->icon_id) { free(e); return NULL; }

    e->state      = ICON_LOADING;
    e->request_id = s_next_req_id++;

    /* Insert into hash table. */
    unsigned long idx = hash_str(icon_id) % ICON_HASH_SIZE;
    e->next        = s_buckets[idx];
    s_buckets[idx] = e;

    /* Start the async fetch:  {API_BASE_URL}/assets/ui-icons/{icon_id}.png */
    char url[512];
    snprintf(url, sizeof(url), "%s/assets/ui-icons/%s.png", API_BASE_URL, icon_id);
    js_start_fetch_binary(url, e->request_id);

    printf("[UI_ICON] Fetch started for '%s' (req %d)\n", icon_id, e->request_id);
    return e;
}

/* ── Public API ─────────────────────────────────────────────────────── */

void ui_icon_init(void) {
    memset(s_buckets, 0, sizeof(s_buckets));
    s_next_req_id = 30000;
}

void ui_icon_poll(void) {
    for (int i = 0; i < ICON_HASH_SIZE; i++) {
        IconEntry* e = s_buckets[i];
        while (e) {
            if (e->state == ICON_LOADING) {
                int size = 0;
                unsigned char* data = js_get_fetch_result(e->request_id, &size);
                if (data && size > 0) {
                    Image img = LoadImageFromMemory(".png", data, size);
                    if (img.data) {
                        e->texture = LoadTextureFromImage(img);
                        UnloadImage(img);
                        e->state = ICON_READY;
                        printf("[UI_ICON] Loaded '%s' (%dx%d)\n",
                               e->icon_id, e->texture.width, e->texture.height);
                    } else {
                        e->state = ICON_ERROR;
                        fprintf(stderr, "[UI_ICON] Image decode failed for '%s'\n",
                                e->icon_id);
                    }
                    free(data);
                } else if (size < 0) {
                    e->state = ICON_ERROR;
                    fprintf(stderr, "[UI_ICON] Fetch error for '%s'\n", e->icon_id);
                }
            }
            e = e->next;
        }
    }
}

void ui_icon_cleanup(void) {
    for (int i = 0; i < ICON_HASH_SIZE; i++) {
        IconEntry* e = s_buckets[i];
        while (e) {
            IconEntry* next = e->next;
            if (e->texture.id > 0) UnloadTexture(e->texture);
            free(e->icon_id);
            free(e);
            e = next;
        }
        s_buckets[i] = NULL;
    }
}

void ui_icon_draw(const char* icon_id, float cx, float cy,
                  int size, bool bounce, float phase) {
    if (!icon_id || icon_id[0] == '\0') return;

    /* ── Bounce: smooth ease-in-out sine float ───────────────────────── */
    float offset_y = 0.0f;
    if (bounce) {
        float t = (float)GetTime();
        float raw = sinf((t * UI_ICON_BOUNCE_FREQ * 2.0f * PI) + phase);
        /* Ease-in-out: cube the sine for a softer reversal at peaks. */
        offset_y = raw * raw * raw * UI_ICON_BOUNCE_AMP;
    }
    float draw_cy = cy + offset_y;

    /* ── Look up (or create) the cached icon entry ───────────────────── */
    IconEntry* e = lookup(icon_id);
    if (!e) e = create_and_fetch(icon_id);
    if (!e) return;

    if (e->state == ICON_READY && e->texture.id > 0) {
        /* Texture loaded — draw centred at (cx, draw_cy). */
        Rectangle src = { 0, 0, (float)e->texture.width, (float)e->texture.height };
        Rectangle dst = { cx - size * 0.5f, draw_cy - size * 0.5f,
                          (float)size, (float)size };
        DrawTexturePro(e->texture, src, dst, (Vector2){0, 0}, 0.0f, WHITE);
    } else if (e->state != ICON_ERROR) {
        /* Still loading — subtle pulsing placeholder dot. */
        float pulse = 0.5f + 0.5f * sinf((float)GetTime() * 4.0f);
        unsigned char alpha = (unsigned char)(40 + (int)(pulse * 60.0f));
        float r = size * 0.25f;
        DrawCircle((int)cx, (int)draw_cy, r, (Color){150, 160, 180, alpha});
    }
}
