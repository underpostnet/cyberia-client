/**
 * @file ol_as_animated_ico.c
 * @brief General-purpose animated ObjectLayer icon renderer implementation.
 *
 * Direction fallback chain:
 *   1. Requested dir_str (e.g. "right_walking")
 *   2. "down_idle"
 *   3. "default_idle"
 *   4. Grey circle fallback
 *
 * Frame selection is driven exclusively by GetTime() — no per-icon state is
 * needed, which keeps the API stateless and safe to call from any draw site.
 */

#include "ol_as_animated_ico.h"
#include <raylib.h>
#include <string.h>

/* ── Internal helpers ─────────────────────────────────────────────────── */

/* fallback_circle draws a neutral grey circle when no sprite is available. */
static void fallback_circle(int x, int y, int icon_size) {
    int r = icon_size / 2;
    DrawCircle(x + r, y + r, (float)r, (Color){140, 140, 160, 180});
}

/* get_frames_with_fallback tries dir_str, then "down_idle", then "default_idle". */
static const DirectionFrameData* get_frames_with_fallback(
        const AtlasSpriteSheetData* atlas, const char* dir_str) {
    const DirectionFrameData* dfd = NULL;

    if (dir_str && dir_str[0] != '\0') {
        dfd = atlas_get_direction_frames(atlas, dir_str);
        if (dfd && dfd->count > 0) return dfd;
    }

    /* Fallback 1: down_idle */
    dfd = atlas_get_direction_frames(atlas, "down_idle");
    if (dfd && dfd->count > 0) return dfd;

    /* Fallback 2: default_idle */
    dfd = atlas_get_direction_frames(atlas, "default_idle");
    if (dfd && dfd->count > 0) return dfd;

    return NULL;
}

/* ── Public API ────────────────────────────────────────────────────────── */

void ol_as_ico_draw(ObjectLayersManager* mgr,
                    const char* item_key,
                    int x, int y, int icon_size,
                    const char* dir_str,
                    int frame_ms,
                    Color tint) {
    if (!mgr || !item_key || item_key[0] == '\0') {
        fallback_circle(x, y, icon_size);
        return;
    }

    AtlasSpriteSheetData* atlas = get_or_fetch_atlas_data(mgr, item_key);
    if (!atlas) {
        /* Atlas not cached yet — pump the fetch state machine so metadata REST
         * request is scheduled (first call) or polled (subsequent calls).
         * Once metadata arrives, get_atlas_texture will automatically kick off
         * the PNG blob fetch on the next frame. */
        get_atlas_texture(mgr, item_key);
        fallback_circle(x, y, icon_size);
        return;
    }

    Texture2D tex = get_atlas_texture(mgr, item_key);
    if (tex.id == 0) {
        fallback_circle(x, y, icon_size);
        return;
    }

    const DirectionFrameData* dfd = get_frames_with_fallback(atlas, dir_str);
    if (!dfd || dfd->count == 0) {
        fallback_circle(x, y, icon_size);
        return;
    }

    int ms_per_frame = (frame_ms > 0) ? frame_ms : OL_ICO_DEFAULT_FRAME_MS;
    int frame_idx    = (int)(GetTime() * 1000.0 / ms_per_frame) % dfd->count;
    const FrameMetadata* fm = &dfd->frames[frame_idx];

    Rectangle src = { (float)fm->x, (float)fm->y,
                      (float)fm->width, (float)fm->height };
    Rectangle dst = { (float)x, (float)y,
                      (float)icon_size, (float)icon_size };
    DrawTexturePro(tex, src, dst, (Vector2){0.0f, 0.0f}, 0.0f, tint);
}

void ol_as_ico_draw_safe(ObjectLayersManager* mgr,
                         const char* item_key,
                         int x, int y, int icon_size,
                         const char* dir_str,
                         int frame_ms) {
    ol_as_ico_draw(mgr, item_key, x, y, icon_size,
                   dir_str ? dir_str : OL_ICO_DEFAULT_DIR,
                   frame_ms, WHITE);
}
