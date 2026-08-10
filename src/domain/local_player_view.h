#ifndef LOCAL_PLAYER_VIEW_H
#define LOCAL_PLAYER_VIEW_H

#include <raylib.h>

#include "object_layer.h"

/* Presentation layer for the local player, drawn at screen centre. Only the
 * renderer and the camera read it. Prediction, reconciliation, collision,
 * and all simulation code ignore it.
 *
 * The render position is a sub-tick interpolation of the predicted position,
 * plus a visual error offset:
 *
 *   render_pos = lerp(prev_sim_pos, curr_sim_pos, alpha) + error_offset
 *
 * The sim advances at 30 Hz while the client predicts, and at the 20 Hz
 * snapshot rate while the server drives the walk. The render loop is faster
 * than both, so the layer interpolates across the measured interval between
 * two changes of the predicted position. The window starts at the position
 * the sprite already shows, so the path stays continuous at any cadence, and
 * it ends on the newest predicted position, so the sprite never runs ahead of
 * the sim. The delay is one window, not a filter: there is no inertia and no
 * overshoot.
 *
 * A reconciliation is not always an error. The client stops predicting when
 * the server acks the input command, so the rest of the walk arrives as
 * reconciliation: that movement drives the sprite. Only the share above the
 * speed of the player is a misprediction — it goes to error_offset, which
 * holds the rendered position and then bleeds toward zero, always slower than
 * the travel of the frame, so a correction can slow the walk a little but can
 * never reverse it.
 *
 * Facing and animation mode come from the motion rate of the window, with
 * hysteresis, so a correction never flips or flickers the sprite. A teleport
 * or a respawn snaps and clears the offset: explicit MODE_TELEPORTING, or a
 * jump longer than the snap distance. */

/* Advance the presentation state to `sim_pos` over `dt` seconds.
 * `sim_correction` is this frame's reconciliation displacement, from
 * prediction_consume_correction(). `sim_direction` and `sim_mode` are the
 * snapshot values. They apply on a snap only, because a teleport gives no
 * motion vector to derive the facing from, and they signal
 * MODE_TELEPORTING. Call once per render frame, then read the accessors
 * below. Frame-rate independent. */
void local_player_view_update(Vector2 sim_pos, Vector2 sim_correction,
                              Direction sim_direction, ObjectLayerMode sim_mode,
                              float dt);

/* On-screen position (grid units): the interpolated sim position plus the
 * residual error offset. */
Vector2 local_player_view_position(void);

/* Facing derived from the motion rate of the window, committed only after the
 * motion has pointed to a new octant for a hold interval; holds the last
 * committed facing while idle or during correction transients. */
Direction local_player_view_direction(void);

/* Walk/idle animation mode derived from the motion rate of the window —
 * always consistent with the motion actually rendered, whether the client
 * predicts that motion or the server sends it. */
ObjectLayerMode local_player_view_mode(void);

/* Local player's draw footprint (grid units) scaled up so the character
 * reads slightly larger than other entities. Anchored at the same feet
 * (bottom-centre) point as the unscaled footprint so growth never shifts
 * the character's ground contact or facing offset. */
Rectangle local_player_view_scaled_footprint(float pos_x, float pos_y, float width, float height);

#endif /* LOCAL_PLAYER_VIEW_H */
