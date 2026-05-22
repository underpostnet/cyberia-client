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
#include "helper.h"
#include "network/engine_client.h"

#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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
    uint32_t          request_id;
    struct IconEntry* next;
} IconEntry;

static IconEntry* s_buckets[ICON_HASH_SIZE];

static void on_icon_fetched(uint32_t request_id, FetchState state, void* data, size_t size) {
    /* Locate entry by request_id across all hash buckets. */
    IconEntry* e = NULL;
    for (int i = 0; NULL == e && i < ICON_HASH_SIZE; i++) {
        for (IconEntry* it = s_buckets[i]; it; it = it->next) {
            if (it->request_id == request_id) { e = it; break; }
        }
    }

    if (NULL == e) { free(data); return; }

    if (FETCH_STATE_READY != state) {
        e->state = ICON_ERROR;
        fprintf(stderr, "[UI_ICON] Fetch error for '%s'\n", e->icon_id);
        free(data);
        return;
    }

    Image img = LoadImageFromMemory(".png", (unsigned char*)data, (int)size);
    free(data);

    if (NULL == img.data) {
        e->state = ICON_ERROR;
        fprintf(stderr, "[UI_ICON] Image decode failed for '%s'\n", e->icon_id);
        return;
    }

    e->texture = LoadTextureFromImage(img);
    UnloadImage(img);
    e->state = ICON_READY;
    printf("[UI_ICON] Loaded '%s' (%dx%d)\n",
           e->icon_id, e->texture.width, e->texture.height);
}

static IconEntry* lookup(const char* icon_id) {
    unsigned long idx = hash_string(icon_id) % ICON_HASH_SIZE;
    IconEntry* e = s_buckets[idx];
    while (e) {
        if (strcmp(e->icon_id, icon_id) == 0) return e;
        e = e->next;
    }
    return NULL;
}

/** Create a new entry and kick off the async HTTP fetch. */
static IconEntry* create_and_fetch(const char* icon_id) {
    assert(icon_id);

    IconEntry* e = (IconEntry*)calloc(1, sizeof(IconEntry));
    e->icon_id = strdup(icon_id);

    /* Insert into hash table. */
    unsigned long idx = hash_string(icon_id) % ICON_HASH_SIZE;
    e->next        = s_buckets[idx];
    s_buckets[idx] = e;

    /* Start the async fetch:  {API_BASE_URL}/assets/ui-icons/{icon_id}.png */
    char url[512];
    snprintf(url, sizeof(url), "%s/assets/ui-icons/%s.png", API_BASE_URL, icon_id);
    e->request_id = fetch_request_start(url, on_icon_fetched);
    e->state      = ICON_LOADING;

    printf("[UI_ICON] Fetch started for '%s' (req %u)\n", icon_id, e->request_id);
    return e;
}

/* ── Public API ─────────────────────────────────────────────────────── */

void ui_icon_init(void) {
    memset(s_buckets, 0, sizeof(s_buckets));
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
