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

#include "domain/camera.h"
#include "domain/presentation_runtime.h"
#include "domain/tick.h"
#include "interpolation/interpolation.h"
#include "network/session.h"
#include "prediction/prediction.h"
#include "util/log.h"

/* Maximum render-frame delta accepted by the fixed-timestep accumulator.
 *
 * Gaffer-on-Games "Fix Your Timestep" guards against the spiral-of-death:
 * a single very-long render frame would otherwise pile up dozens of
 * simulation steps and freeze the next frame. Clamp the delta at
 * 0.25 s so we run at most ~8 sim steps per render at TICK_RATE_HZ=30. */
#define MAIN_LOOP_MAX_FRAME_DT 0.25f

typedef enum {
    MAIN_STATE_LOADING = 0, /* presentation fetch + initial server handshake */
    MAIN_STATE_RUNNING,
} MainState;

static MainState s_state    = MAIN_STATE_LOADING;
static double    s_sim_acc  = 0.0;

static void main_loop_loading(float frame_dt) {
    (void)frame_dt;
    /* Drive the network FSM so reconnect attempts still happen during
     * startup; this is what gets us out of LOADING once init_data lands. */
    client_tick();

    /* Render a splash frame. game_state.init_received drives the renderer's
     * fallback path. */
    render_update(GetFrameTime());

    bool hints_ready = presentation_runtime_is_ready();
    bool world_ready = g_game_state.init_received;
    if (hints_ready && world_ready) {
        s_state   = MAIN_STATE_RUNNING;
        s_sim_acc = 0.0; /* avoid catch-up burst on entry */
        LOG_INFO("LOADING → RUNNING");
    }
}

static void main_loop_running(float frame_dt) {
    if (frame_dt > MAIN_LOOP_MAX_FRAME_DT) frame_dt = MAIN_LOOP_MAX_FRAME_DT;
    s_sim_acc += (double)frame_dt;

    client_tick();

    /* Sample raw input, then convert queued events into commands. The
     * dependency direction is now: input → input_command queue → prediction
     * (consumed inside prediction_step) + uplink. */
    input_update();
    input_event_queue_handle();

    /* Fixed-timestep simulation. The accumulator drains at TICK_DURATION_S
     * per step regardless of frame rate, matching the server's tick. */
    while (s_sim_acc >= TICK_DURATION_S) {
        prediction_step(TICK_DURATION_S);
        s_sim_acc -= TICK_DURATION_S;
    }

    /* Render-frame smoothed display position decouples the visible main
     * player from per-tick stepping. */
    prediction_display_step((double)frame_dt);
    g_game_state.player.base.interp_pos = prediction_self_position();

    /* Remote-entity render-time interpolation. */
    interpolation_compute_view();

    render_update(frame_dt);
}

void main_loop(void) {
    const float frame_dt = GetFrameTime();

    /* If we lost the connection at runtime, drop back to LOADING so the
     * splash screen renders while the reconnect FSM works. */
    if (s_state == MAIN_STATE_RUNNING && !g_game_state.init_received) {
        s_state = MAIN_STATE_LOADING;
        LOG_INFO("RUNNING → LOADING (init lost)");
    }

    if (s_state == MAIN_STATE_LOADING) {
        main_loop_loading(frame_dt);
    } else {
        main_loop_running(frame_dt);
    }
}

int main(void) {
    js_init_engine_api(API_BASE_URL);

    int vp_w = EM_ASM_INT({ return window.innerWidth; });
    int vp_h = EM_ASM_INT({ return window.innerHeight; });
    InitWindow(vp_w, vp_h, NULL);
    SetTargetFPS(60);

    render_init(vp_w, vp_h);

    /* Optional per-instance presentation overrides. Non-blocking. The
     * client renders the bootstrap splash until the response settles or
     * times out. */
    presentation_runtime_start_fetch(API_BASE_URL, CYBERIA_CLIENT_HINTS_CODE);

    prediction_init();

    if (!connection_open()) {
        connection_close();
        return -1;
    }

    emscripten_set_main_loop(main_loop, 0, 1);

    connection_close();
    render_cleanup();
    CloseWindow();
    return 0;
}
