#ifndef CYBERIA_NETWORK_REPLICATION_H
#define CYBERIA_NETWORK_REPLICATION_H

#include "input/input.h"
#include "input/input_command.h"

#include <stdbool.h>
#include <stdint.h>
#include <raylib.h>

/*
 * Client→server input replication.
 *
 * Drains the per-frame input event queue, builds typed input commands, applies
 * them to the prediction replay buffer, and ships them on the wire. This is the
 * single uplink path for player actions; it sits above the raw WebSocket I/O
 * owned by game_client and below the main loop that captures input.
 */

/* Drain a (deep-copied) frame input queue: per tap, build the command, enqueue
 * it for prediction, send it to the server, and set the local on-tap target. */
void replication_prepare_input(input_queue_t in_queue);


// WIP

/* Prediction — predicted self position (sole writer); replay + reconcile. */
void prediction_init(void);
void prediction_reset(Vector2 authoritative_pos);
bool prediction_apply(const input_command_t* cmd);
void prediction_enqueue_input(const input_command_t* cmd);
void prediction_step(double tick_dt);
void prediction_reconcile(void);
Vector2 prediction_self_position(void);
/* Net reconciliation displacement since the last call (then zeroed). Consumed
 * once per render frame by the local-player presentation layer, which absorbs
 * it so corrections never disturb the rendered trajectory. */
Vector2 prediction_consume_correction(void);

/* Interpolation — render-time smoothing of remote entities (sole writer of
 * EntityState.interp_pos; never the local player). */
void interpolation_compute_view(void);

/* Session — per-connection tick + acknowledgement bookkeeping (sole writer). */
void session_on_snapshot(uint32_t snapshot_tick, uint32_t last_acked_sequence);
cyberia_tick_t session_last_server_tick(void);
cyberia_input_seq_t session_last_acked_input_sequence(void);
cyberia_tick_t session_server_tick_estimate(void);
cyberia_tick_t session_render_tick(void);
cyberia_input_seq_t session_next_input_sequence(void);

#endif /* CYBERIA_NETWORK_REPLICATION_H */
