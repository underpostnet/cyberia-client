#ifndef OL_AS_ANIMATED_ICO_H
#define OL_AS_ANIMATED_ICO_H

#include "object_layers_management.h"
#include "object_layer.h"
#include <raylib.h>

/* Draws an atlas-backed ObjectLayer item as a looping animated icon at any
 * screen size. Falls back to a neutral grey circle while the atlas loads. */

/* Default direction and mode string, used when dir_str is NULL. */
#define OL_ICO_DEFAULT_DIR     "down_idle"

/* Default frame duration, in ms. Matches the in-world entity render. */
#define OL_ICO_DEFAULT_FRAME_MS 100

/* Draw one animation frame of `item_key` at (x, y), icon_size square, in
 * screen pixels. Frames advance off GetTime(), so the caller keeps no state.
 * `dir_str` names an atlas direction ("down_idle", "right_walking", ...);
 * NULL and frame_ms 0 take the defaults above. WHITE means no tint. */
void ol_as_ico_draw(ObjectLayersManager* mgr,
                    const char* item_key,
                    int x, int y, int icon_size,
                    const char* dir_str,
                    int frame_ms,
                    Color tint);

/* Same, but falls back to "down_idle" then "default_idle" when the atlas has
 * no frames for `dir_str`. */
void ol_as_ico_draw_safe(ObjectLayersManager* mgr,
                         const char* item_key,
                         int x, int y, int icon_size,
                         const char* dir_str,
                         int frame_ms);

#endif /* OL_AS_ANIMATED_ICO_H */
