#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "input/input.h"
#include "game_state.h"
#include "render.h"
#include "network/game_client.h"
#include "network/replication.h"
#include "config.h"

#include <raylib.h>
#include <emscripten/emscripten.h>

#include "js/interact_bridge.h"
#include "js/loading_bridge.h"
#include "network/engine_client.h"

#include "domain/camera.h"
#include "domain/presentation_runtime.h"
#include "util/log.h"
#include "ui/ui_dispatch.h"
#include "ui/fx_tap.h"
#include "ui/interaction_bubble.h"
#include "ui/text.h"
#include "domain/local_player.h"
#include "domain/local_player_view.h"

static const double fixed_step = 1.0/(double)TICK_RATE_HZ;
static double sim_acc = 0.0;

/* True when the world-px tap position lands within a finger-sized radius of
 * a quest/action provider bot. */
static bool tap_hits_provider(Vector2 world_pos) {
    float cell = g_game_state.cell_size > 0.0f ? g_game_state.cell_size : 12.0f;
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
static void gameloop(void) {
    float frame_dt = GetFrameTime();
#ifndef CYBERIA_DEBUG
    if ( frame_dt > 0.25 ) { frame_dt = 0.25; } // runnaway clamp
#endif
    sim_acc += (double)frame_dt;

    text_font_sync();
    game_client_on_tick();
    local_player_on_tick();

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
            if(!consumed && INPUT_TAP == evt.type) {
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
     * predicted/reconciled simulation position. Prediction itself is
     * untouched — the outputs written back below feed rendering/camera only,
     * keeping sprite motion, facing, and animation in lockstep instead of
     * following the server's asynchronous snapshot cadence. */
    local_player_view_update(prediction_self_position(),
                             prediction_consume_correction(),
                             g_game_state.player.base.direction,
                             g_game_state.player.base.mode,
                             frame_dt);
    g_game_state.player.base.interp_pos = local_player_view_position();
    g_game_state.player.base.direction  = local_player_view_direction();
    g_game_state.player.base.mode       = local_player_view_mode();

    /* Remote-entity render-time interpolation. */
    interpolation_compute_view();

    // render interpolated state
    render_on_tick(frame_dt);
}

/* ── Loading stages ──────────────────────────────────────────────────────
 * Each stage completes on a REAL initialization signal — never a timer.
 * The world renders beneath the DOM loading overlay for the whole preload,
 * so atlas/ObjectLayer fetches and texture creation are warmed before the
 * player ever sees the scene. The order is the true dependency chain. */
enum {
    LOAD_RUNTIME = 0, /* WASM runtime, renderer, UI systems initialized     */
    LOAD_CONNECT,     /* WebSocket to the simulation server open            */
    LOAD_WORLD,       /* authoritative init_data received                   */
    LOAD_HINTS,       /* presentation client-hints fetched                  */
    LOAD_ASSETS,      /* engine REST pipeline (atlas/OL/textures) went idle */
    LOAD_STABLE,      /* sustained frames with zero outstanding fetches     */
    LOAD_STAGE_COUNT,
};

/* Label of the stage IN PROGRESS. */
static const char* LOAD_STAGE_LABEL[LOAD_STAGE_COUNT] = {
    "INITIALIZING RUNTIME...",
    "CONNECTING TO CYBERIA SERVER...",
    "SYNCHRONIZING WORLD STATE...",
    "LOADING PRESENTATION HINTS...",
    "STREAMING ATLAS TEXTURES...",
    "STABILIZING INSTANCE...",
};

/* Share of the progress bar per stage (sums to 100). Asset streaming
 * dominates real load time, so it owns most of the bar and advances
 * continuously with the fetch completion ratio. */
static const float LOAD_STAGE_WEIGHT[LOAD_STAGE_COUNT] = {
    5.0f, 10.0f, 10.0f, 10.0f, 55.0f, 10.0f,
};

/* The stabilization window: this many consecutive rendered frames with an
 * idle fetch pipeline before the world counts as visually stable. */
#define LOAD_STABLE_FRAMES 45

static int  s_load_done      = 0;     /* stages completed so far            */
static int  s_stable_frames  = 0;
static bool s_load_ready     = false; /* all stages done, awaiting the tap  */

/* True when the current stage's real completion signal is observed. */
static bool load_stage_complete(int stage) {
    switch (stage) {
        case LOAD_RUNTIME: return true; /* main() finished all init calls   */
        case LOAD_CONNECT: return connection_is_open();
        case LOAD_WORLD:   return g_game_state.init_received;
        case LOAD_HINTS:   return presentation_runtime_is_ready();
        case LOAD_ASSETS:  return fetch_total_started() > 0 &&
                                  0 == fetch_pending_count();
        case LOAD_STABLE:
            if (0 == fetch_pending_count()) s_stable_frames++;
            else                            s_stable_frames = 0;
            return LOAD_STABLE_FRAMES <= s_stable_frames;
    }
    return false;
}

/* 0..1 completion of the stage in progress — real measurements only. */
static float load_stage_fraction(int stage) {
    switch (stage) {
        case LOAD_ASSETS: {
            int total = fetch_total_started();
            if (0 >= total) return 0.0f;
            return (float)(total - fetch_pending_count()) / (float)total;
        }
        case LOAD_STABLE:
            return (float)s_stable_frames / (float)LOAD_STABLE_FRAMES;
        default:
            return 0.0f;
    }
}

/* Bar percent + streamed label for this frame. While assets stream, the
 * label is the id of the last completed fetch so the player sees exactly
 * what is loading. */
static void report_loading_progress(void) {
    float pct = 0.0f;
    for (int i = 0; i < s_load_done; i++) pct += LOAD_STAGE_WEIGHT[i];
    pct += LOAD_STAGE_WEIGHT[s_load_done] * load_stage_fraction(s_load_done);

    const char* label = LOAD_STAGE_LABEL[s_load_done];
    static char asset_label[128];
    if (LOAD_ASSETS == s_load_done && '\0' != fetch_last_completed_id()[0]) {
        /* Bare asset name: last path segment, extension stripped
         * ("/api/atlas-sprite-sheet/blob/eiri.png" → "eiri"). */
        const char* id    = fetch_last_completed_id();
        const char* slash = strrchr(id, '/');
        snprintf(asset_label, sizeof(asset_label), " %s",
                 slash ? slash + 1 : id);
        char* dot = strrchr(asset_label, '.');
        if (dot) *dot = '\0';
        label = asset_label;
    }
    loading_bridge_progress(pct, label);
}

static void preloading_loop(void) {
    const float frame_dt = GetFrameTime();
    text_font_sync();
    game_client_on_tick();

    /* Render the (still hidden) world every preload frame: this is what
     * drives the lazy atlas/ObjectLayer fetches and texture creation, so
     * LOAD_ASSETS / LOAD_STABLE measure genuine readiness. */
    render_on_tick(frame_dt);

    /* Stages complete strictly in order — each is gated on the previous. */
    while (!s_load_ready && load_stage_complete(s_load_done)) {
        s_load_done++;
        if (LOAD_STAGE_COUNT <= s_load_done) {
            s_load_ready = true;
            loading_bridge_ready();
        }
    }
    if (!s_load_ready) report_loading_progress();

    /* Gameplay begins only on the player's explicit Tap-to-Start. */
    if (s_load_ready && loading_bridge_start_requested()) {
        loading_bridge_hide();
        client_confirm_loading_done(); /* release the server "loading" freeze */
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
    text_font_init(); // main UI font (loaded async once client-hints name a fontFamily)

    // [preload] start loading step, fetch from Data Server (Engine)
    js_init_engine_api(API_BASE_URL);

    // NOTE: Do not mix the start fetch loop with the running game loop
    // if need to be non blocking then wait in a loading screen before starting main_loop
    // all the initializations should be consolidated in related modules
    presentation_runtime_start_fetch(CYBERIA_CLIENT_HINTS_CODE);

    // [preload] it should handle the switch to gameloop on callback
    emscripten_set_main_loop(preloading_loop, 0, 1);

    // Note: close steps do not make sense in a web environment, but we should keep them for a while
    connection_close();
    render_cleanup();
    CloseWindow();
    return 0;
}
