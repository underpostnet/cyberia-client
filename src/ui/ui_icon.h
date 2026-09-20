#ifndef UI_ICON_H
#define UI_ICON_H

#include <raylib.h>
#include <stdbool.h>
#include <stddef.h>

// The central request table owns icon textures.

/* Bounce amplitude in pixels — half of the total vertical travel. */
#define UI_ICON_BOUNCE_AMP      2.5f

/* Bounce frequency in Hz. */
#define UI_ICON_BOUNCE_FREQ     1.1f

/* Bounce phase in radians, derived from `id` — two icons next to each other
 * then bounce out of step. NULL or "" gives 0. */
static inline float ui_icon_phase(const char* id) {
    unsigned int h = 0;
    if (NULL == id) return 0.0f;
    for (const char* c = id; *c; c++) h = h * 31 + (unsigned char)*c;
    return (float)(h % 1000) * 0.001f * 6.2832f;
}

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
