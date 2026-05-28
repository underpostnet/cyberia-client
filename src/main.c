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

static bool is_loading = true;
static void loading(void) {
    const float frame_dt = GetFrameTime();
    game_client_tick();

    // will fall into render fallback... those should be here and not there(?)
    render_update(frame_dt);

    bool hints_ready = presentation_runtime_is_ready(); // TODO: Turn this into a fetch request
    bool world_ready = g_game_state.init_received;
    if (hints_ready && world_ready) {
        is_loading   = false;
    }
}

static const double fixed_step = TICK_DURATION_S;
static double sim_acc = 0.0;
static void gameloop(void) {
    const float frame_dt = GetFrameTime();
    // if ( frame_dt > 0.25 ) { frame_dt = 0.25; } // draft, prevents runaways, keep commented, ignore
    sim_acc += (double)frame_dt;

    game_client_tick();

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
    prediction_display_step((double)frame_dt);
    g_game_state.player.base.interp_pos = prediction_self_position();

    /* Remote-entity render-time interpolation. */
    interpolation_compute_view();

    // render interpolated state
    render_update(frame_dt);
}

void main_loop(void) {
    /* If we lost the connection at runtime, drop back to LOADING so the
     * splash screen renders while the reconnect FSM works. */
    if (!is_loading && !g_game_state.init_received) {
        is_loading = true; // TODO: a connection lost while in game should be a different loading screen, not the initial
        LOG_INFO("RUNNING → LOADING (init lost)");
    }

    if (is_loading) {
        loading();
    } else {
        gameloop();
    }
}

int main(void) {
    js_init_engine_api(API_BASE_URL);

    const int vp_w = EM_ASM_INT({ return window.innerWidth; });
    const int vp_h = EM_ASM_INT({ return window.innerHeight; });
    InitWindow(vp_w, vp_h, NULL);
    SetTargetFPS(60);

    render_init(vp_w, vp_h); // NOTE: if render is the window, then combine with it

    // NOTE: Do not mix the start fetch loop with the running game loop
    // if need to be non blocking then wait in a loading screen before starting main_loop
    // all the initializations should be consolidated in related modules
    presentation_runtime_start_fetch(API_BASE_URL, CYBERIA_CLIENT_HINTS_CODE);
    prediction_init();
    ///

    if (!connection_open()) {
        connection_close();
        return -1; // UNIX returns don't make sense for a webapp
    }

    emscripten_set_main_loop(main_loop, 0, 1);

    connection_close();
    render_cleanup();
    CloseWindow();
    return 0;
}
