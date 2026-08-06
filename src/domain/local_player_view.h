#ifndef LOCAL_PLAYER_VIEW_H
#define LOCAL_PLAYER_VIEW_H

#include <raylib.h>

#include "object_layer.h"

/* Presentation layer for the local player, drawn at screen centre. Only the
 * renderer and the camera read it. Prediction, reconciliation, collision,
 * and all simulation code ignore it.
 *
 * It owns its own position, velocity, facing, and walk/idle mode. A
 * critically damped spring advances that state toward the predicted position
 * each render frame.
 *
 * A reconciliation displacement goes into a correction offset the moment it
 * lands, so the spring target never jumps. The offset bleeds away at a speed
 * below the walk speed. A correction can slow the walk a little, but it can
 * never reverse it or look like a hesitation.
 *
 * Facing and animation mode come from the presentation velocity, with
 * hysteresis, so a correction never flips or flickers the sprite. A teleport
 * or a respawn snaps: explicit MODE_TELEPORTING, or a jump longer than the
 * snap distance. */

/* Advance the presentation state toward `sim_pos` over `dt` seconds.
 * `sim_correction` is this frame's reconciliation displacement, from
 * prediction_consume_correction(). `sim_direction` and `sim_mode` are the
 * snapshot values. They apply on a snap only, because a teleport gives no
 * motion vector to derive the facing from, and they signal
 * MODE_TELEPORTING. Call once per render frame, then read the accessors
 * below. Frame-rate independent. */
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
