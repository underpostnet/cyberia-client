#ifndef LOCAL_PLAYER_VIEW_H
#define LOCAL_PLAYER_VIEW_H

#include <raylib.h>

#include "object_layer.h"

/* Presentation layer for the LOCAL player — the entity rendered at screen
 * centre. Purely visual: never read by prediction, reconciliation, collision,
 * or any gameplay/simulation code — only by the renderer/camera.
 *
 * It owns an independent presentation state (position, velocity, facing,
 * walk/idle mode) advanced each render frame toward the simulation's
 * predicted/reconciled position by a critically damped spring. Reconciliation
 * displacements are absorbed into a correction offset the instant they land —
 * the spring target never jumps — and the offset bleeds away with its speed
 * capped below the walk speed, so a correction can slightly slow the walk but
 * can never reverse it or read as a hesitation. Facing and animation mode are
 * derived from the presentation velocity with hysteresis, so corrections can
 * never flip or flicker the sprite. Teleports and respawns (explicit
 * MODE_TELEPORTING, or a jump beyond the snap distance) snap instantly. */

/* Advance the presentation state toward `sim_pos` over `dt` seconds.
 * `sim_correction` is this frame's reconciliation displacement
 * (prediction_consume_correction()), absorbed so it never disturbs the
 * rendered trajectory. `sim_direction` and `sim_mode` are the authoritative
 * snapshot values; they are consumed only on snaps (a teleport has no organic
 * motion vector to derive facing from) and to detect MODE_TELEPORTING. Call
 * once per render frame, then read the accessors below. Frame-rate
 * independent. */
void local_player_view_update(Vector2 sim_pos, Vector2 sim_correction,
                              Direction sim_direction, ObjectLayerMode sim_mode,
                              float dt);

/* Smoothed on-screen position (grid units). */
Vector2 local_player_view_position(void);

/* Facing derived from the presentation velocity, committed only after the
 * motion has pointed to a new octant for a hold interval; holds the last
 * committed facing while idle or during correction transients. */
Direction local_player_view_direction(void);

/* Walk/idle animation mode derived from the presentation speed — always
 * consistent with the motion actually rendered, never with the server's
 * asynchronous snapshot cadence. */
ObjectLayerMode local_player_view_mode(void);

/* Local player's draw footprint (grid units) scaled up so the character
 * reads slightly larger than other entities. Anchored at the same feet
 * (bottom-centre) point as the unscaled footprint so growth never shifts
 * the character's ground contact or facing offset. */
Rectangle local_player_view_scaled_footprint(float pos_x, float pos_y, float width, float height);

#endif /* LOCAL_PLAYER_VIEW_H */
