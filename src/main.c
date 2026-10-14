#include "config.h"
#include "preload.h"
#include "telemetry.h"

#include "input/input.h"
#include "sim/game_state.h"
#include "sim/local_player.h"
#include "sim/replication.h"
#include "data/engine_client.h"
#include "net/game_client.h"
#include "net/client_event.h"
#include "view/render.h"
#include "view/entity_render.h"
#include "view/local_player_view.h"
#include "audio/audio_context.h"
#include "audio/audio.h"
#include "ui/ui_dispatch.h"
#include "ui/text.h"
#include "js/text_input_bridge.h"

#include <raylib.h>
#include <emscripten/emscripten.h>

static const double fixed_step = 1.0/(double)TICK_RATE_HZ;
static double sim_acc = 0.0;
static void game_frame(void) {
    float frame_dt = GetFrameTime();
#ifndef CYBERIA_DEBUG
    if ( frame_dt > 0.25 ) { frame_dt = 0.25; } // runnaway clamp
#endif
    sim_acc += (double)frame_dt;

    // reconcile first, from the tick boundary: the decoder only stores the snapshot
    prediction_reconcile();

    fetch_frame_begin(frame_dt, !player_render_ready() || !immediate_scene_ready() || game_state_own_player()->on_portal);
    text_font_sync();
    game_client_on_tick();
    audio_context_update(frame_dt);
    audio_update(frame_dt);

    // input capture in realtime
    input_queue_t frame_input = {0};
    input_queue_on_tick(&frame_input, frame_dt, ui_dispatch_covers_point);
    ui_on_tick(&frame_input, frame_dt);
    input_push_client_events(frame_input);

    // fixed step simulation
    bool stepped = false;
    while (sim_acc >= fixed_step)
    {
        session_frame_advance();
        prediction_step(session_frame());
        sim_acc -= fixed_step;
        stepped = true;
    }
    /* One events message per pass that ran a step. */
    if (stepped) {
        client_event_flush();
    }
    /* interp_pos is the presentation position of every entity; for the local
     * player the view owns it. */
    local_player_view_update(prediction_prev(), prediction_self(), (float)(sim_acc / fixed_step));
    if (0 != g_local_player.id) {
        game_state_render(g_local_player.id)->interp_pos = local_player_view_position();
    }

    /* Remote-entity render-time interpolation. */
    interpolation_compute_view();

    // render interpolated state
    render_on_tick(frame_dt);
    telemetry_observe_scene(true);
    fetch_process_frame();
    fetch_frame_end();
}

/* One main loop for both phases: the splash until the player taps to start,
 * then gameplay. */
static bool s_playing = false;
static void frame(void) {
    if (s_playing) {
        game_frame();
    } else {
        s_playing = preload_frame();
    }
}

int main(int argc, char** argv) {
    // init window
    const int vp_w = EM_ASM_INT({ return window.innerWidth; });
    const int vp_h = EM_ASM_INT({ return window.innerHeight; });
    text_input_bridge_init(); // before InitWindow: its key guard must precede GLFW's
    InitWindow(vp_w, vp_h, "CYBERIA MMO");

    // Resolves the instance code from the URL and the Data Server URL from the
    // command line. Must precede any connection or Data Server call.
    config_init(argc, argv);
    fetch_init(argc, argv);
    telemetry_event("boot_start"); // after fetch_init: it reads --stream-profile
    audio_init();
    render_init(vp_w, vp_h);
    text_font_init(); // main UI font (loaded async once client-hints name a fontFamily)

    preload_start();

    // simulate_infinite_loop = 1: this call never returns
    emscripten_set_main_loop(frame, 0, 1);
    return 0;
}
