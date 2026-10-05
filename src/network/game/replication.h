#ifndef CYBERIA_NETWORK_REPLICATION_H
#define CYBERIA_NETWORK_REPLICATION_H

#include "network/game/client_event.h"

#include <stdbool.h>
#include <stdint.h>
#include <raylib.h>

/* A server tick. Only the session_* functions use it. */
typedef uint32_t cyberia_tick_t;

/* Prediction — predicted self position (sole writer); replay + reconcile. */
void prediction_init(void);
void prediction_reset(Vector2 authoritative_pos);
void prediction_enqueue_input(const client_event_t* cmd);
void prediction_step(double tick_dt);
void prediction_reconcile(void);
Vector2 prediction_self_position(void);
/* Net reconciliation displacement since the last call (then zeroed). Consumed
 * once per render frame by the local-player presentation layer, which absorbs
 * it so corrections never disturb the rendered trajectory. */
Vector2 prediction_consume_correction(void);
/* The authoritative walk the latest snapshot describes: the A* polyline the
 * server follows and the destination it planned for. Self-only, so it lives
 * here and not in the entity mirror; the snapshot decoder is the sole writer.
 * prediction_route() also backs the debug overlay. */
void prediction_set_route(const Vector2* points, int count, Vector2 target);
const Vector2* prediction_route(int* count);
Vector2 prediction_route_target(void);

/* Interpolation — render-time smoothing of remote entities (sole writer of
 * EntityState.interp_pos; never the local player). */
void interpolation_compute_view(void);

/* Session — per-connection tick + acknowledgement bookkeeping (sole writer). */
void session_on_snapshot(uint32_t snapshot_tick, uint32_t input_consumed_through,
                         uint32_t last_movement_sequence);
cyberia_tick_t session_last_server_tick(void);
/* Highest command sequence the server actually re-planned movement for. Always
 * at or behind the consumed cursor: the server consumes every tap of a tick
 * but plans only the newest. */
cyberia_input_seq_t session_last_movement_sequence(void);
cyberia_tick_t session_server_tick_estimate(void);
cyberia_input_seq_t session_next_input_sequence(void);
/* Completed fixed steps since the page loaded. Not the server tick: the loop
 * drops steps that the server runs. session_frame_advance is its only writer,
 * once per fixed step. */
cyberia_frame_t session_frame(void);
void            session_frame_advance(void);

#endif /* CYBERIA_NETWORK_REPLICATION_H */
