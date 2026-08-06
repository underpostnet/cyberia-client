#ifndef UI_ICON_H
#define UI_ICON_H

#include <raylib.h>
#include <stdbool.h>

/* UI icon renderer. Draws small PNG icons fetched from the engine API at
 * /assets/ui-icons/{icon_id}.png through the engine_client queue, cached in
 * a module-local LRU texture cache. Until a texture lands, a pulsing
 * placeholder dot holds the layout. Active icons float on a sine bounce
 * whose phase the caller seeds, so neighbouring icons do not move together.
 *
 * This module renders; entity_status picks which icon an entity shows. */

/* Rendered size of an overhead status icon (pixels). Large enough to read
 * at normal zoom without competing with the nameplate or the HP bar. */
#define UI_ICON_DEFAULT_SIZE    28

/* Bounce amplitude in pixels — half of the total vertical travel. */
#define UI_ICON_BOUNCE_AMP      2.5f

/* Bounce frequency in Hz. */
#define UI_ICON_BOUNCE_FREQ     1.1f

/* Capacity (entry count) of the module-local icon texture cache. */
#define UI_ICON_CACHE_CAPACITY  64

/* Create the module-local icon TextureCache. Call once during
 * game_render_init(). capacity sets the LRU ceiling. */
void ui_icon_init(int capacity);

/* Destroy the icon cache (unloads all icon textures). Call during
 * game_render_cleanup(). */
void ui_icon_cleanup(void);

/* Draw an icon centred on (cx, cy) in world pixels — call inside
 * BeginMode2D. `icon_id` is the file stem (e.g. "skull"); NULL or "" draws
 * nothing. `phase` offsets the bounce, in radians. */
void ui_icon_draw(const char* icon_id, float cx, float cy, int size, bool bounce, float phase);

/* Draw an icon with an arbitrary float size, rotation (degrees, around the
 * centre), and tint (tint.a fades it). No loading placeholder — for decorative,
 * transient FX (e.g. reward celebration stars). No-op until the texture loads. */
void ui_icon_draw_ex(const char* icon_id, float cx, float cy, float size,
                     float rotation_deg, Color tint);

#endif /* UI_ICON_H */
