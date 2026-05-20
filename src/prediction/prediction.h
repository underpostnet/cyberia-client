/**
 * @file prediction/prediction.h
 * @brief Client-side prediction + input replay for the self-entity.
 *
 * This module owns the predicted self position. It is the only writer of
 * the predicted position; the renderer reads via prediction_self_position().
 *
 * Pipeline (per render frame, driven by app_loop.c):
 *
 *   1. INPUT      tap → input_command_build_tap → prediction_apply(cmd)
 *                                              → uplink_send(cmd)
 *      `prediction_apply` pushes the command onto the replay buffer AND
 *      applies it optimistically to the predicted position so the player
 *      sees movement immediately.
 *
 *   2. NETWORK    snapshot arrives → binary_aoi_decoder updates authoritative
 *                                    state in g_game_state.player.base.pos_server
 *                                    → calls prediction_reconcile()
 *
 *   3. RECONCILE  prediction_reconcile() drops acked inputs (sequence ≤
 *                 last_acked_input_sequence), then rewinds predicted state to
 *                 the authoritative position and replays the remaining
 *                 unacked input commands.
 *
 *   4. STEP       fixed-timestep prediction_step(TICK_DURATION_S) advances
 *                 the predicted state by one tick using the same
 *                 path-toward-target movement the server simulates.
 *                 main_loop calls this 0..N times per render frame via
 *                 the fixed-timestep accumulator.
 *
 *   5. RENDER     game_render reads prediction_self_position() and uses it
 *                 to draw the local player.
 *
 * Ownership: prediction.c is the only writer of the predicted self
 * position. Remote-entity smoothing is the responsibility of
 * interpolation_compute_view (see interpolation/).
 */

#ifndef CYBERIA_PREDICTION_H
#define CYBERIA_PREDICTION_H

#include <stdbool.h>
#include <raylib.h>
#include "../domain/tick.h"
#include "../input/input_command.h"

/** Initialise the prediction state. Call once at startup, before the first
 *  snapshot. Idempotent. */
void prediction_init(void);

/** Reset the prediction to a known authoritative position. Called on world
 *  load / teleport / respawn — anywhere the server hard-sets self position. */
void prediction_reset(Vector2 authoritative_pos);

/** Apply a fresh InputCommand: push to replay buffer + optimistic local
 *  mutation. Caller has already sent the command on the wire (or will).
 *  Returns false if the buffer is full (overflow drops oldest). */
bool prediction_apply(const input_command_t* cmd);

/** Advance the predicted self position by one simulation tick. Called from
 *  the fixed-timestep accumulator in app_loop.c. dt is fixed at
 *  TICK_DURATION_S — do not pass a render dt here. */
void prediction_step(double tick_dt);

/** Reconciliation: drop acked inputs, then rewind to the latest
 *  authoritative server position and replay the remaining unacked inputs.
 *  Called once per arriving snapshot. */
void prediction_reconcile(void);

/** Current predicted self position. Renderer-facing accessor. */
Vector2 prediction_self_position(void);

#endif /* CYBERIA_PREDICTION_H */
