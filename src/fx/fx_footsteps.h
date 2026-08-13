/* Footstep emitter — decides when a moving entity kicks up dust.
 *
 * The client has no per-entity controller: the local player's gait comes from
 * domain/local_player_view and every remote entity's from interpolation, and
 * both have written their render position, facing, and walk/idle mode into
 * g_game_state by the time the frame updates. So this module reads that one
 * settled state each frame rather than being called from several controllers.
 *
 * Cadence is stride-based, not timer-based: a puff is emitted every fixed
 * distance travelled, so the dust lands with the gait at any speed and a run
 * simply produces steps more often. Running is measured, not flagged — an
 * entity covering ground faster than its authoritative move speed is running,
 * which is what a speed buff, a mount, or a dash all look like from here.
 *
 * Emits for players and walking bots; loot, coin, and skill projectiles carry
 * no feet, matching the entities the ground shadow is drawn for.
 *
 * Integration pattern:
 *   init:    fx_footsteps_init();
 *   update:  fx_footsteps_update(delta_time);   // before the frame draws
 * Drawing belongs to fx_dust.
 */

#ifndef FX_FOOTSTEPS_H
#define FX_FOOTSTEPS_H

/* Entities tracked at once; the rest simply raise no dust this frame. */
#define FX_FOOTSTEPS_MAX_TRACKED 64

void fx_footsteps_init(void);

/* Drop every stride tracker — a map change or teleport makes the last known
 * position meaningless. */
void fx_footsteps_reset(void);

/* Scan the world's entities, advance their strides, and spawn the dust. Call
 * once per render frame, after the render positions are settled. */
void fx_footsteps_update(float dt);

#endif /* FX_FOOTSTEPS_H */
