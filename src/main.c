#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

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
#include "interpolation/interpolation.h"
#include "network/session.h"
#include "prediction/prediction.h"
#include "util/log.h"

static const double fixed_step = 1.0/(double)TICK_RATE_HZ;
static double sim_acc = 0.0;
static void gameloop(void) {
    const float frame_dt = GetFrameTime();
    // if ( frame_dt > 0.25 ) { frame_dt = 0.25; } // prevents runaways, but we are not using it, keep commented
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

static void preloading_loop(void) {
    const float frame_dt = GetFrameTime();
    game_client_tick();

    // will fall into render fallback... those should be here and not there(?)
    render_update(frame_dt);

    bool hints_ready = presentation_runtime_is_ready(); // TODO: Turn this into a fetch request
    bool world_ready = g_game_state.init_received;
    if (hints_ready && world_ready) {
        // when loading is done we replace loop with normal game loop
        emscripten_cancel_main_loop();
        emscripten_set_main_loop(gameloop, 0, 1);
    }
}

int main(void) {
    // init window
    const int vp_w = EM_ASM_INT({ return window.innerWidth; });
    const int vp_h = EM_ASM_INT({ return window.innerHeight; });
    InitWindow(vp_w, vp_h, NULL);
    SetTargetFPS(TICK_RATE_HZ);

    // Connects to Game Server
    connection_open();

    prediction_init(); // Note: this is just data, should be replaced by GameState
    render_init(vp_w, vp_h); // NOTE: if render is the window, then combine with it

    // [preload] start loading step, fetch from Data Server (Engine)
    js_init_engine_api(API_BASE_URL);

    // NOTE: Do not mix the start fetch loop with the running game loop
    // if need to be non blocking then wait in a loading screen before starting main_loop
    // all the initializations should be consolidated in related modules
    presentation_runtime_start_fetch(API_BASE_URL, CYBERIA_CLIENT_HINTS_CODE);

    // [preload] it should handle the switch to gameloop on callback
    emscripten_set_main_loop(preloading_loop, 0, 1);

    // Note: close steps do not make sense in a web environment, but we should keep them for a while
    connection_close();
    render_cleanup();
    CloseWindow();
    return 0;
}
