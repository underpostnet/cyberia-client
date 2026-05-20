/**
 * @file domain/tick.h
 * @brief Canonical tick types and timing constants for the client.
 *
 * Tick is the universal simulation coordinate. It is produced authoritatively
 * by the Go server and stamped into every snapshot header (v2 AOI format).
 * The client keeps:
 *
 *   - last_server_tick    — the highest Tick observed in a snapshot.
 *   - predicted_tick      — what the client believes the server is currently
 *                           simulating. Advances at TICK_RATE_HZ via the
 *                           fixed-timestep accumulator in app_loop.
 *
 * Rendering frames are decoupled from ticks: render runs at vsync; the
 * accumulator inside app_loop fires prediction_step zero or more times per
 * render frame so the *simulation* stays at a fixed rate regardless of FPS.
 *
 * No client module should derive timing from frame_dt for simulation
 * purposes — only for visual smoothing of view-models.
 */

#ifndef CYBERIA_DOMAIN_TICK_H
#define CYBERIA_DOMAIN_TICK_H

#include <stdint.h>

/** Authoritative server simulation Hz. Must match the server's tickRate.
 *  Sent in InitPayload.tickRate; until init lands, prediction uses this
 *  compile-time default so the client can run before first init. */
#define TICK_RATE_HZ          30
#define TICK_DURATION_S       (1.0 / (double)TICK_RATE_HZ)

/** Interpolation delay in ticks. Render-time view positions for remote
 *  entities are computed at (server_tick_estimate - INTERP_TICKS). Larger
 *  values give smoother motion at the cost of more apparent latency for
 *  observers. Two ticks (~67ms @ 30Hz) is a standard FPS/MMO default. */
#define INTERP_TICKS          2

/** Maximum number of ticks the interpolator is allowed to extrapolate
 *  past the last snapshot before freezing the view. */
#define MAX_EXTRAP_TICKS      4

/** Tick — the simulation step counter. uint32 gives ~828 days at 60Hz. */
typedef uint32_t cyberia_tick_t;

/** Sequence — monotonic client-local InputCommand counter. */
typedef uint32_t cyberia_input_seq_t;

#endif /* CYBERIA_DOMAIN_TICK_H */
