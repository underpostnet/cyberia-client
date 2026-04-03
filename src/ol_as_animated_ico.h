/**
 * @file ol_as_animated_ico.h
 * @brief General-purpose animated ObjectLayer icon renderer.
 *
 * Renders an atlas-backed ObjectLayer item as a looping animated icon (GIF-like)
 * at any size on screen.  Direction and mode are fully configurable, defaulting
 * to "down_idle".  Falls back to a neutral grey circle while the atlas is loading.
 *
 * Usage:
 *   // Simple animated icon at default direction/mode:
 *   ol_as_ico_draw(mgr, "anon", x, y, 48, "down_idle", 100);
 *
 *   // Walking right:
 *   ol_as_ico_draw(mgr, "sword", x, y, 48, "right_walking", 80);
 *
 *   Direction strings match atlas field names:
 *     "down_idle", "up_idle", "left_idle", "right_idle",
 *     "down_walking", "up_walking", "left_walking", "right_walking",
 *     "default_idle", etc.
 *
 * Dependencies: object_layers_management.h, object_layer.h, raylib.h
 */

#ifndef OL_AS_ANIMATED_ICO_H
#define OL_AS_ANIMATED_ICO_H

#include "object_layers_management.h"
#include "object_layer.h"
#include <raylib.h>

/* ── Default animation parameters ────────────────────────────────────── */

/** Default direction+mode string used when dir_str is NULL. */
#define OL_ICO_DEFAULT_DIR     "down_idle"

/** Default frame duration in milliseconds (matches in-world entity render). */
#define OL_ICO_DEFAULT_FRAME_MS 100

/**
 * @brief Draw an animated ObjectLayer icon.
 *
 * Renders the current animation frame for @p item_key at (x, y) with
 * dimensions icon_size × icon_size.  Frame selection advances automatically
 * using GetTime() so callers do not need to track state per icon.
 *
 * @param mgr           ObjectLayersManager used for atlas + metadata lookup.
 * @param item_key      ObjectLayer item identifier (e.g. "anon", "coin").
 * @param x             Left edge of the icon in screen pixels.
 * @param y             Top edge of the icon in screen pixels.
 * @param icon_size     Width = Height of the drawn square in pixels.
 * @param dir_str       Atlas direction string (e.g. "down_idle"); NULL → default.
 * @param frame_ms      Milliseconds per animation frame; 0 → default (100 ms).
 * @param tint          Colour tint applied to the sprite (WHITE = no tint).
 */
void ol_as_ico_draw(ObjectLayersManager* mgr,
                    const char* item_key,
                    int x, int y, int icon_size,
                    const char* dir_str,
                    int frame_ms,
                    Color tint);

/**
 * @brief Draw an animated icon, trying dir_str then falling back to "down_idle"
 *        then "default_idle".  Convenience wrapper over ol_as_ico_draw.
 *
 * @param mgr       ObjectLayersManager.
 * @param item_key  ObjectLayer item identifier.
 * @param x         Left edge in pixels.
 * @param y         Top edge in pixels.
 * @param icon_size Icon size in pixels.
 * @param dir_str   Preferred direction string; NULL → "down_idle".
 * @param frame_ms  Frame duration ms; 0 → 100 ms.
 */
void ol_as_ico_draw_safe(ObjectLayersManager* mgr,
                         const char* item_key,
                         int x, int y, int icon_size,
                         const char* dir_str,
                         int frame_ms);

#endif /* OL_AS_ANIMATED_ICO_H */
