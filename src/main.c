#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "input/input.h"
#include "game_state.h"
#include "network/engine_client.h"
#include "entity_render.h"
#include "world_types.h"
#include "object_layer.h"
#include "util/utils.h"
#include "render.h"
#include "network/game_client.h"
#include "network/replication.h"
#include "config.h"

#include <raylib.h>
#include <emscripten/emscripten.h>

#include "js/interact_bridge.h"
#include "js/loading_bridge.h"

#include "domain/camera.h"
#include "domain/presentation_runtime.h"
#include "domain/audio_context.h"
#include "audio/audio.h"
#include "util/log.h"
#include "ui/ui_dispatch.h"
#include "fx/fx_tap.h"
#include "ui/interaction_bubble.h"
#include "ui/text.h"
#include "domain/local_player.h"
#include "domain/local_player_view.h"

static const double fixed_step = 1.0/(double)TICK_RATE_HZ;
static double sim_acc = 0.0;

/* True when the world-px tap position lands within a finger-sized radius of
 * a quest/action provider bot. */
static bool tap_hits_provider(Vector2 world_pos) {
    float cell = world_cell_size();
    for (int i = 0; i < g_game_state.bot_count; i++) {
        const BotState* bot = &g_game_state.bots[i];
        if ('\0' == bot->action_code[0] && 0 == bot->quest_code_count) continue;
        float half_w = bot->base.dims.x * 0.5f;
        float half_h = bot->base.dims.y * 0.5f;
        float cx = (bot->base.interp_pos.x + half_w) * cell;
        float cy = (bot->base.interp_pos.y + half_h) * cell;
        float reach = cell * (0.9f + (half_w > half_h ? half_w : half_h));
        float dx = world_pos.x - cx, dy = world_pos.y - cy;
        if (dx * dx + dy * dy <= reach * reach) return true;
    }
    return false;
}
static char s_trace_map[MAX_ID_LENGTH];
static bool s_trace_player;
static bool s_trace_frame;
static bool s_trace_scene;
static bool s_trace_movement;
static bool s_destination;
static Vector2 s_trace_position;

static void observe_scene(bool gameplay) {
    const EntityState* player = &g_game_state.player.base;
    if ('\0' == g_local_player.map_code[0]) return;
    if (0 != strcmp(s_trace_map, g_local_player.map_code)) {
        s_destination = '\0' != s_trace_map[0];
        copy_str(s_trace_map, sizeof(s_trace_map), g_local_player.map_code);
        s_trace_player = s_trace_frame = s_trace_movement = s_trace_scene = false;
        s_trace_position = player->pos_server;
    }
    if (!s_trace_player && player_render_ready()) {
        s_trace_player = true;
        fetch_event(s_destination ? "destination_player_ready" : "main_player_ready", s_trace_map, 0, 0);
    }
    if (!s_trace_scene && s_trace_player && immediate_scene_ready()) {
        s_trace_scene = true;
        fetch_event(s_destination ? "destination_activation" : "initial_scene_ready", s_trace_map, 0, 0);
    }
    if (gameplay && !s_trace_frame) {
        s_trace_frame = true;
        fetch_event(s_destination ? "first_destination_frame" : "first_frame", s_trace_map, 0, 0);
    }
    if (gameplay && !s_trace_movement &&
        (s_trace_position.x != player->pos_server.x || s_trace_position.y != player->pos_server.y)) {
        s_trace_movement = true;
        fetch_event(s_destination ? "first_destination_movement" : "first_movement", s_trace_map, 0, 0);
    }
}

static void gameloop(void) {
    float frame_dt = GetFrameTime();
#ifndef CYBERIA_DEBUG
    if ( frame_dt > 0.25 ) { frame_dt = 0.25; } // runnaway clamp
#endif
    sim_acc += (double)frame_dt;

    fetch_frame_begin((double)frame_dt * 1000, !player_render_ready() || !immediate_scene_ready() || local_player_on_portal());
    text_font_sync();
    game_client_on_tick();
    local_player_on_tick();
    audio_context_update(frame_dt);
    audio_update(frame_dt);

    // input capture in realtime
    input_queue_t frame_input = {0};
    input_queue_on_tick(&frame_input, frame_dt);

    ui_on_tick(&frame_input, frame_dt);

    // TODO: collapse this into a function, this is temporary to remove input.c dependency
    {
        input_queue_t bkp_queue = { 0 };
        input_event_t evt = { 0 };
        while (input_pop(&frame_input, &evt)) {
            bool consumed = false;
            if(!consumed && INPUT_TAP == evt.type) {
                /* FrozenInteractionState — server says we're frozen, drop the tap. */
                if (local_player_is_frozen()) { consumed = true; }
                if (g_game_state.player.base.respawn_in > 0.0f) { consumed = true; }
            }
            // unconsumed event back to the queue
            if(!consumed) {
                input_push(&bkp_queue, evt);
                continue;
            }
        }
        // return unconsummed events to the original queue
        input_event_t bkp_evt = { 0 };
        while (input_pop(&bkp_queue, &bkp_evt)) { input_push(&frame_input, bkp_evt ); }
    }

    replication_prepare_input(frame_input);

    // TODO: collapse this into a function, this is temporary to remove input.c dependency
    {
        // Tap Effect
        input_queue_t bkp_queue = { 0 };
        input_event_t evt = { 0 };
        while (input_pop(&frame_input, &evt)) {
            bool consumed = false;
            /* Keyboard steering repeats at the screen edge; it draws no tap
             * effect and opens no bubble. Both belong to a real pointer. */
            if(!consumed && INPUT_TAP == evt.type && !evt.synthetic) {
                FxTapParams fx = fx_tap_default_params();
                fx.scale = 1.15f;
                fx.duration = 0.70f;
                fx.intensity = 1.25f;
                fx_tap_spawn(evt.world_position, &fx);
                /* A tap landing on a quest/action provider auto-opens the
                 * collapsed bubble column so the interaction is reachable. */
                if (interaction_bubble_is_collapsed() &&
                    tap_hits_provider(evt.world_position)) {
                    interaction_bubble_expand();
                }
                consumed = false; // TAP EFFECTS DON'T CONSUME THE INPUT, BUT ALSO SHOULDN'T HAPPEN BEFORE PROCESS
            }
             // unconsumed event back to the queue
            if(!consumed) {
                input_push(&bkp_queue, evt);
                continue;
            }
        }
         // return unconsummed events to the original queue
        input_event_t bkp_evt = { 0 };
        while (input_pop(&bkp_queue, &bkp_evt)) { input_push(&frame_input, bkp_evt ); }
    }

    // fixed step simulation
    while (sim_acc >= fixed_step)
    {
        // physics_update(frame_input, fixed_step); -> prev = curr; integrate(curr, curr_frame, fixed_step)
        prediction_step(fixed_step);
        sim_acc -= fixed_step;
    }
    /* Presentation-only: advance the local player's visual state (spring
     * position, velocity-derived facing and walk/idle mode) toward the
     * predicted/reconciled simulation position. Facing and mode go in
     * authoritative and come out smoothed; the smoothed pair stays in the view
     * module, so what the renderer draws never becomes what the smoother reads
     * next frame. interp_pos is the presentation position of every entity, and
     * for the local player the view owns it. */
    local_player_view_update(prediction_self_position(),
                             prediction_consume_correction(),
                             g_game_state.player.base.direction,
                             g_game_state.player.base.mode,
                             frame_dt);
    g_game_state.player.base.interp_pos = local_player_view_position();

    /* Remote-entity render-time interpolation. */
    interpolation_compute_view();

    // render interpolated state
    render_on_tick(frame_dt);
    observe_scene(true);
    fetch_process_frame();
    fetch_frame_end();
}

static bool s_load_ready;

static void preloading_loop(void) {
    const float frame_dt = GetFrameTime();
    fetch_frame_begin((double)frame_dt * 1000, true);
    text_font_sync();
    game_client_on_tick();

    render_on_tick(frame_dt);
    audio_update(frame_dt);
    observe_scene(false);

    if (!s_load_ready) {
        const char* label = NULL;
        if (!connection_is_open()) label = "Connecting to Cyberia...";
        else if (!g_game_state.init_received || '\0' == g_game_state.player.base.id[0]) label = "Entering the world...";
        else if (!presentation_runtime_is_ready()) label = "Preparing the view...";
        else if (!player_render_ready()) label = "Loading your character...";
        else if (!immediate_scene_ready()) label = "Preparing your location...";
        else if (!text_font_settled()) label = "Preparing the interface...";
        if (NULL != label) loading_bridge_progress(-1, label);
        else {
            s_load_ready = true;
            fetch_event("tap_to_start_visible", s_trace_map, 0, 0);
            loading_bridge_ready();
        }
    }
    fetch_process_frame();
    fetch_frame_end();

    /* Gameplay begins only on the player's explicit Tap-to-Start. */
    if (s_load_ready && loading_bridge_start_requested()) {
        fetch_event("gameplay_start", s_trace_map, 0, 0);
        audio_start();
        loading_bridge_hide();
        client_confirm_loading_done(); /* release the server "loading" freeze */
        emscripten_cancel_main_loop();
        emscripten_set_main_loop(gameloop, 0, 1);
    }
}

int main(int argc, char** argv) {
    // init window
    const int vp_w = EM_ASM_INT({ return window.innerWidth; });
    const int vp_h = EM_ASM_INT({ return window.innerHeight; });
    InitWindow(vp_w, vp_h, NULL);

    // Resolves the instance code from the URL and the Data Server URL from the
    // command line. Must precede any connection or Data Server call.
    config_init(argc, argv);
    fetch_init();
    fetch_event("boot_start", "", 0, 0);
    audio_init();

    // Connects to Game Server
    connection_open();

    prediction_init(); // Note: this is just data, should be replaced by GameState
    render_init(vp_w, vp_h); // NOTE: if render is the window, then combine with it
    text_font_init(); // main UI font (loaded async once client-hints name a fontFamily)

    // [preload] start loading step, fetch from Data Server (Engine)
    js_init_engine_api(config_data_server_url());

    // NOTE: Do not mix the start fetch loop with the running game loop
    // if need to be non blocking then wait in a loading screen before starting main_loop
    // all the initializations should be consolidated in related modules
    presentation_runtime_start_fetch(CYBERIA_CLIENT_HINTS_CODE);

    // [preload] it should handle the switch to gameloop on callback
    emscripten_set_main_loop(preloading_loop, 0, 1);

    // Note: close steps do not make sense in a web environment, but we should keep them for a while
    connection_close();
    render_cleanup();
    audio_shutdown();
    fetch_shutdown();
    CloseWindow();
    return 0;
}
