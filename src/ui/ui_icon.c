/**
 * @file ui_icon.c
 * @brief General-purpose UI Icon System — implementation.
 *
 * Self-contained async texture cache for small PNG icons served by the
 * engine API.
 *
 * See ui_icon.h for full documentation.
 */

#include "ui_icon.h"

#include "config.h"
#include "hash_table.h"
#include "network/engine_client.h"

#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ── Icon texture cache ─────────────────────────────────────────────── */

typedef enum {
    ICON_NONE,
    ICON_LOADING,
    ICON_READY,
    ICON_ERROR
} IconState;

typedef struct {
    Texture2D texture;
    IconState state;
} IconEntry;

static HashTable ht;

static void icon_entry_free(void* p) {
    IconEntry* e = p;
    UnloadTexture(e->texture);
    free(e);
}

static void on_icon_fetched(const FetchResponse* r) {
    IconEntry* e = hash_table_get(&ht, r->asset_id);
    if (NULL == e) { free(r->data); return; }

    if (FETCH_STATE_READY != r->state) {
        e->state = ICON_ERROR;
        fprintf(stderr, "[UI_ICON] Fetch error for '%s'\n", r->asset_id);
        free(r->data);
        return;
    }

    Image img = LoadImageFromMemory(".png", (unsigned char*)r->data, (int)r->size);
    free(r->data);

    if (NULL == img.data) {
        e->state = ICON_ERROR;
        fprintf(stderr, "[UI_ICON] Image decode failed for '%s'\n", r->asset_id);
        return;
    }

    e->texture = LoadTextureFromImage(img);
    UnloadImage(img);
    e->state = ICON_READY;
    printf("[UI_ICON] Loaded '%s' (%dx%d)\n",
           r->asset_id, e->texture.width, e->texture.height);
}

static IconEntry* create_and_fetch(const char* icon_id) {
    assert(icon_id);

    IconEntry* e = calloc(1, sizeof(IconEntry));
    e->state = ICON_LOADING;
    hash_table_put(&ht, icon_id, e);

    char url[512];
    snprintf(url, sizeof(url), "%s/assets/ui-icons/%s.png", API_BASE_URL, icon_id);
    uint32_t req_id = fetch_request_start(icon_id, url, on_icon_fetched);

    printf("[UI_ICON] Fetch started for '%s' (req %u)\n", icon_id, req_id);
    return e;
}

/* ── Public API ─────────────────────────────────────────────────────── */

void ui_icon_init(void) {
    hash_table_init(&ht, 64, icon_entry_free);
}

void ui_icon_cleanup(void) {
    hash_table_destroy(&ht);
}

void ui_icon_draw(const char* icon_id, float cx, float cy, int size, bool bounce, float phase) {
    assert(icon_id);
    assert(strlen(icon_id));

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
    IconEntry* e = hash_table_get(&ht, icon_id);
    if (!e) e = create_and_fetch(icon_id);
    if (!e) return;

    if (e->state == ICON_READY) {
        /* Texture loaded — draw centred at (cx, draw_cy). */
        Rectangle src = { 0, 0, (float)e->texture.width, (float)e->texture.height };
        Rectangle dst = { cx - size * 0.5f, draw_cy - size * 0.5f,
                          (float)size, (float)size };
        DrawTexturePro(e->texture, src, dst, (Vector2){0, 0}, 0.0f, WHITE);
    } else {
        /* Still loading — subtle pulsing placeholder dot. */
        float pulse = 0.5f + 0.5f * sinf((float)GetTime() * 4.0f);
        unsigned char alpha = (unsigned char)(40 + (int)(pulse * 60.0f));
        float r = size * 0.25f;
        DrawCircle((int)cx, (int)draw_cy, r, (Color){150, 160, 180, alpha});
    }
}
