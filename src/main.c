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

// TODO: this should live in client, which should handle its own reconnection logic
void reconnection_logic(bool init_received) {
    // function local variables - temporry
    static int frame_counter = 0;
    static double last_reconnect_time = 0.0;
    static double socket_attempt_time = 0.0;
    // local consts
    const double INIT_DATA_TIMEOUT_S = 15.0;
    const double RECONNECT_INTERVAL_S = 3.0;
    /* Periodic heartbeat so we know C is running even without WS callbacks. */
    frame_counter++;
    if (frame_counter % 300 == 0) {
        printf("[LOOP] frame=%d t=%.1fs connected=%d init_received=%d\n",
               frame_counter, (float)GetTime(),
               connection_is_active() ? 1 : 0,
               init_received ? 1 : 0);
    }

    /* Reconnect when the socket is dead. The guard intentionally does not
     * gate on init_received: when the WS dies mid-session, on_websocket_close
     * already resets init_received to false, but the relevant signal here
     * is simply "no socket → reconnect." */
    if (!connection_is_active()) {
        double now = GetTime();
        if (now - last_reconnect_time >= RECONNECT_INTERVAL_S) {
            last_reconnect_time = now;
            socket_attempt_time = now;
            printf("[RECONNECT] Socket inactive — reconnecting...\n");
            connection_open();
        }
    } else if (!init_received) {
        /* Socket exists but init_data never landed. If we've been waiting
         * too long, drop the socket so the next iteration of the guard
         * above can reconnect. Covers the reload-stuck-on-splash case
         * where the server accepted the upgrade but never sent init_data
         * (e.g. degraded gRPC, empty maps). */
        if (socket_attempt_time == 0.0) socket_attempt_time = GetTime();
        if (GetTime() - socket_attempt_time >= INIT_DATA_TIMEOUT_S) {
            printf("[RECONNECT] init_data did not arrive within %.0fs — closing socket to force reconnect.\n",
                   INIT_DATA_TIMEOUT_S);
            connection_close();
            socket_attempt_time = 0.0;
        }
    } else {
        /* Healthy connection — reset the handshake timer so a future
         * disconnect starts from a fresh budget. */
        socket_attempt_time = 0.0;
    }
}

static const double fixed_step = TICK_DURATION_S;
static double sim_acc = 0.0;
void main_loop(void) {
    const float frame_dt = GetFrameTime();
    // if ( frame_dt > 0.25 ) { frame_dt = 0.25; } // draft, prevents runaways, keep commented, ignore
    sim_acc += (double)frame_dt;

    reconnection_logic(g_game_state.init_received); // TODO: remove from here

    // input capture in realtime
    input_update();
    input_event_queue_handle();

    // fixed step simulation
    while (sim_acc >= fixed_step)
    {
        // physics_update(fixed_step); -> prev = curr; integrate(curr, curr_frame, fixed_step)
        prediction_step(fixed_step);
        sim_acc -= fixed_step;
    }
    const double alpha = sim_acc / fixed_step;
    // state_interpolation(alpha) -> curr * alpha +  prev * ( 1.0 - alpha );

    /* Advance the render-frame smoothed display position and publish it as
     * the main-player view-model. Without this step, the renderer reads
     * the raw per-tick predicted position and the player jumps in discrete
     * sim-tick increments — visible as abrupt animation, especially right
     * after a reconcile rebases on a fresh snapshot. */
    prediction_display_step((double)frame_dt);
    g_game_state.player.base.interp_pos = prediction_self_position();

    /* 4. Remote entity interpolation. */
    interpolation_compute_view();

    // render interpolated state
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

    // TODO: Do not mix the start fetch loop with the running game loop
    // if need to be non blocking then wait in a loading screen before starting main_loop
    presentation_runtime_start_fetch(API_BASE_URL, CYBERIA_CLIENT_HINTS_CODE);

    // Initialise prediction before the WS connects so the first snapshot
    // arriving on the callback has somewhere to land.
    prediction_init();

    // post window init - init ws
    if(!connection_open()) {
        connection_close();
        return -1;
    }

    emscripten_set_main_loop(main_loop, 0, 1);

    connection_close();
    render_cleanup();

    CloseWindow();
    return 0;
}
