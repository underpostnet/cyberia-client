/* Dust puffs — the ground particles kicked up under a moving entity's feet.
 *
 * A standalone particle pool: it knows nothing about entities, movement, or
 * gait. Callers hand it a world position and a heading; it spawns a burst that
 * drifts backward, spreads, fades, and shrinks out. fx_footsteps owns the
 * decision of when a step happens — this module only draws the puff.
 *
 * Positions are WORLD PIXELS (grid units × cell_size), so fx_dust_draw must run
 * inside BeginMode2D, beneath the entity pass, keeping the dust on the ground
 * instead of over the sprites.
 *
 * Fixed pool, no allocation: a burst that would exceed the pool recycles the
 * oldest puffs, so a crowd of runners degrades by dropping the stalest dust
 * rather than by growing memory.
 *
 * Integration pattern:
 *   init:    fx_dust_init();
 *   update:  fx_dust_update(delta_time);
 *   draw:    fx_dust_draw();   // inside BeginMode2D, before entities
 */

#ifndef FX_DUST_H
#define FX_DUST_H

#include <raylib.h>

#define FX_DUST_MAX_PUFFS 160

typedef struct {
    Color color;      /* body tint; its alpha is ignored — life drives opacity */
    float size;       /* puff edge at birth, world px                          */
    float duration;   /* lifetime in seconds                                   */
    float drift;      /* backward travel over the whole life, world px         */
    float rise;       /* upward travel over the whole life, world px — pairs
                       * with drift to carry the puff diagonally up and back   */
    float fan;        /* sideways travel over the life, world px; puffs take
                       * alternating sides, so a burst opens both ways         */
    float spread;     /* lateral scatter at birth, world px                    */
    int   count;      /* puffs per burst                                       */
} FxDustParams;

void fx_dust_init(void);
void fx_dust_reset(void);

/* Walking baseline. fx_footsteps scales it up for a run. */
FxDustParams fx_dust_default_params(void);

/* Spawn one burst at `world_pos`, thrown opposite `heading` (any non-zero
 * vector; normalized internally). A zero heading scatters evenly instead. */
void fx_dust_spawn(Vector2 world_pos, Vector2 heading, const FxDustParams* params);

void fx_dust_update(float dt);
void fx_dust_draw(void);

/* Live puff count — pool diagnostics. */
int fx_dust_active_count(void);

#endif /* FX_DUST_H */
