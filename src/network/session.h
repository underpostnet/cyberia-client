/**
 * @file network/session.h
 * @brief Per-connection tick + acknowledgement state.
 *
 * session.* tracks everything the client needs to translate between
 * "now" and "the tick the server is currently simulating":
 *
 *   - last_server_tick           highest Tick observed in any snapshot.
 *   - last_acked_input_sequence  highest InputCommand.Sequence the server
 *                                has applied (echoed in every snapshot
 *                                header). Used by prediction to drop
 *                                acknowledged inputs from its replay buffer.
 *   - server_tick_offset_estimate  smoothed delta between the local monotonic
 *                                clock and the server's authoritative tick
 *                                stream, used by interpolation to pick the
 *                                render-time tick.
 *
 * Ownership rules:
 *   - session.c is the ONLY writer of these fields.
 *   - binary_aoi_decoder.c calls session_on_snapshot(tick, lastAckSeq).
 *   - prediction.c reads last_acked_input_sequence and the snapshot tick
 *     to know how far to rewind/replay.
 *   - interpolation.c reads session_render_tick() to decide what view-time
 *     to interpolate to.
 */

#ifndef CYBERIA_NETWORK_SESSION_H
#define CYBERIA_NETWORK_SESSION_H

#include <stdint.h>

// TODO: the usage of sequence counting is inconsistent - Input should be using it, not Session
typedef uint32_t cyberia_tick_t;
typedef uint32_t cyberia_input_seq_t;

/** Called by binary_aoi_decoder.c whenever a v2 snapshot header is parsed.
 *  Updates last_server_tick and last_acked_input_sequence, and feeds the
 *  server-clock estimator. Safe to call from the WS callback. */
void session_on_snapshot(uint32_t snapshot_tick, uint32_t last_acked_sequence);

/** Highest snapshot tick observed so far. Zero until first snapshot. */
cyberia_tick_t session_last_server_tick(void);

/** Highest InputCommand.Sequence the server has applied. Zero until first
 *  snapshot with non-zero ack. Prediction uses this to drop acked inputs. */
cyberia_input_seq_t session_last_acked_input_sequence(void);

/** Client estimate of what the server is currently simulating. Combines
 *  last_server_tick with the elapsed wall time since the snapshot arrived,
 *  divided by TICK_DURATION_S. */
cyberia_tick_t session_server_tick_estimate(void);

/** The tick the interpolator should render at: server_tick_estimate -
 *  INTERP_TICKS. Returns 0 until first snapshot. */
cyberia_tick_t session_render_tick(void);

/** Allocate the next client InputCommand sequence number. Monotonic; never
 *  returns the same value twice in a session. */
cyberia_input_seq_t session_next_input_sequence(void);

#endif /* CYBERIA_NETWORK_SESSION_H */
