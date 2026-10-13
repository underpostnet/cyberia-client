// Hit burst: a few red sparks jump out of the point where damage lands, arc up,
// fall back and shrink away. Small and short, so a fight shows each hit
// without hiding the action.
//
// Integration pattern:
//   spawn:   fx_hit_burst_spawn(world_position);   // a combat_text damage event
//   update:  fx_hit_burst_update(delta_time);
//   draw:    fx_hit_burst_draw();   // inside BeginMode2D, above the entities

#ifndef CYBERIA_FX_FX_HIT_BURST_H
#define CYBERIA_FX_FX_HIT_BURST_H

#include <raylib.h>

/* Starts one burst at `world_position`, in grid units. */
void fx_hit_burst_spawn(Vector2 world_position);
void fx_hit_burst_update(float dt);
void fx_hit_burst_draw(void);
void fx_hit_burst_reset(void);

#endif /* CYBERIA_FX_FX_HIT_BURST_H */
