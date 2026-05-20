#include <stdio.h>
#include <stdlib.h>

#include "input.h"
#include "game_state.h"
#include "render.h"
#include "network/client.h"
#include "config.h"

#include <raylib.h>
#include <emscripten/emscripten.h>

#include "js/services.h"

/* New tick-based architecture entry points.
 *
 * domain/tick.h          authoritative timing constants (TICK_RATE_HZ,
 *                        TICK_DURATION_S, INTERP_TICKS).
 * network/session.h      per-connection tick + ack state. Updated by the
 *                        binary AOI decoder; consulted by prediction.
 * prediction/prediction  self-entity prediction with input replay
 *                        + reconciliation.
 * interpolation/         remote-entity render-time interpolation.
 *
 * Rendering and simulation are now conceptually separated:
 *   - prediction_step    fixed-dt simulation (one tick per call); driven by
 *                        the accumulator in this file.
 *   - render_update      visual frame; reads view-models only.
 */
#include "domain/tick.h"
#include "domain/presentation_runtime.h"
#include "prediction/prediction.h"
#include "interpolation/interpolation.h"
#include "network/session.h"

/* Main event loop (called every render frame).
 *
 * Update ordering — fixed by design, do not reorder:
 *
 *   1. INPUT CAPTURE     poll raw OS events.
 *   2. INPUT DISPATCH    drain queued events → UI hit-test → InputCommand
 *                        → prediction_apply (optimistic) → wire send.
 *   3. FIXED-TIMESTEP    accumulator drains TICK_DURATION_S chunks, each
 *                        calling prediction_step exactly once. This is
 *                        where simulation lives — decoupled from FPS.
 *   4. INTERPOLATION     remote entity view-models for the current render
 *                        tick.
 *   5. RENDER            read view-models only; never mutate world state.
 *
 * Snapshot ingestion happens asynchronously on the WS callback (see
 * network/client.c). The decoder calls session_on_snapshot which feeds
 * the prediction reconciliation downstream of the next prediction_step.
 */
static double g_sim_accumulator = 0.0;
static int g_frame_counter = 0;
/* Last time a reconnect was attempted (GetTime seconds).
 * Initialised to 0 so the first attempt cannot fire until at least 3 s
 * after InitWindow(), giving the initial connection time to establish. */
static double g_last_reconnect_time = 0.0;
/* GetTime() when the current socket entered CONNECTING. Used to time-out
 * a hung handshake (connected, no init_data) so we don't sit on the
 * splash screen indefinitely when the server is in a degraded state. */
static double g_socket_attempt_time = 0.0;

#define INIT_DATA_TIMEOUT_S 15.0
#define RECONNECT_INTERVAL_S 3.0

void main_loop(void) {
    const float frame_dt = GetFrameTime();

    /* Periodic heartbeat so we know C is running even without WS callbacks. */
    g_frame_counter++;
    if (g_frame_counter % 300 == 0) {
        printf("[LOOP] frame=%d t=%.1fs connected=%d init_received=%d\n",
               g_frame_counter, (float)GetTime(),
               connection_is_active() ? 1 : 0,
               g_game_state.init_received ? 1 : 0);
    }

    /* Reconnect when the socket is dead. The guard intentionally does not
     * gate on init_received: when the WS dies mid-session, on_websocket_close
     * already resets init_received to false, but the relevant signal here
     * is simply "no socket → reconnect." */
    if (!connection_is_active()) {
        double now = GetTime();
        if (now - g_last_reconnect_time >= RECONNECT_INTERVAL_S) {
            g_last_reconnect_time = now;
            g_socket_attempt_time = now;
            printf("[RECONNECT] Socket inactive — reconnecting...\n");
            connection_open();
        }
    } else if (!g_game_state.init_received) {
        /* Socket exists but init_data never landed. If we've been waiting
         * too long, drop the socket so the next iteration of the guard
         * above can reconnect. Covers the reload-stuck-on-splash case
         * where the server accepted the upgrade but never sent init_data
         * (e.g. degraded gRPC, empty maps). */
        if (g_socket_attempt_time == 0.0) g_socket_attempt_time = GetTime();
        if (GetTime() - g_socket_attempt_time >= INIT_DATA_TIMEOUT_S) {
            printf("[RECONNECT] init_data did not arrive within %.0fs — closing socket to force reconnect.\n",
                   INIT_DATA_TIMEOUT_S);
            connection_close();
            g_socket_attempt_time = 0.0;
        }
    } else {
        /* Healthy connection — reset the handshake timer so a future
         * disconnect starts from a fresh budget. */
        g_socket_attempt_time = 0.0;
    }

    /* 0. Poll the optional engine-side presentation override fetch.
     * Returns true exactly once — the frame the response settles. From
     * that frame on, presentation_runtime_palette() and status-icon
     * lookups reflect the engine's per-instance overrides. */
    (void)presentation_runtime_poll();

    /* 1 + 2. Input capture and dispatch. */
    input_update();
    input_event_queue_handle();

    /* Reconciliation is event-driven — it fires from binary_aoi_decoder
     * the moment a fresh snapshot is parsed.  Calling it again here every
     * render frame would undo the prediction_step below by re-rebasing to
     * the same authoritative_pos and replaying the same input buffer,
     * which caused the post-tap "freeze / oscillation" symptom.
     */

    /* 3. Fixed-timestep simulation. The accumulator decouples sim from
     *    render: at 144 FPS this fires zero times most frames; at 30 FPS
     *    it fires once per frame; at 10 FPS it fires three times catching
     *    up. */
    g_sim_accumulator += (double)frame_dt;
    /* Cap accumulator to one second to avoid runaway after a long pause. */
    if (g_sim_accumulator > 1.0) g_sim_accumulator = 1.0;
    while (g_sim_accumulator >= TICK_DURATION_S) {
        prediction_step(TICK_DURATION_S);
        g_sim_accumulator -= TICK_DURATION_S;
    }

    /* Publish the predicted self position as the renderable view-model. */
    g_game_state.player.base.interp_pos = prediction_self_position();

    /* 4. Remote entity interpolation. */
    interpolation_compute_view();

    /* 5. Render frame. */
    render_update(frame_dt);
}

// Main entry point
int main(void) {
    // Initialize engine API base URL for public API access
    js_init_engine_api(API_BASE_URL);

    // Read the browser viewport size so the canvas fills the full screen.
    // Subsequent browser resizes are handled by IsWindowResized() in render_update.
    int vp_w = EM_ASM_INT({ return window.innerWidth; });
    int vp_h = EM_ASM_INT({ return window.innerHeight; });

    // Initialize window with raylib
    InitWindow(vp_w, vp_h, NULL);
    SetTargetFPS(60);

    // Post Raylib init, init
    render_init(vp_w, vp_h);

    // Kick off optional per-instance presentation overrides. Non-blocking:
    // the client renders with built-in defaults from frame 0; if the engine
    // returns overrides, they apply once the fetch completes. The Go
    // simulation server never sees this request.
    presentation_runtime_start_fetch(API_BASE_URL, INSTANCE_CODE);

    // Initialise prediction before the WS connects so the first snapshot
    // arriving on the callback has somewhere to land.
    prediction_init();

    // post window init - init ws
    if(!connection_open()) {
        connection_close();
        return -1;
    }
    g_socket_attempt_time = GetTime();
    g_last_reconnect_time = GetTime();

    emscripten_set_main_loop(main_loop, 0, 1);

    connection_close();
    render_cleanup();

    CloseWindow();
    return 0;
}
