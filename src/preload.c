#include "preload.h"

#include "audio/audio.h"
#include "config.h"
#include "content/presentation_runtime.h"
#include "data/engine_client.h"
#include "net/game_client.h"
#include "sim/game_state.h"
#include "sim/local_player.h"
#include "sim/replication.h"
#include "telemetry.h"
#include "ui/modal.h"
#include "ui/text.h"
#include "view/camera.h"
#include "view/entity_render.h"

#include <math.h>
#include <raylib.h>
#include <emscripten/emscripten.h>

/* Seconds without a ready gate before the splash offers a page reload. */
#define PRELOAD_RELOAD_AFTER_S 15.0

static const Color SPLASH_BG    = {   5,   5,   5, 255 };
static const Color SPLASH_GOLD  = { 255, 204,   0, 255 };
static const Color SPLASH_LABEL = { 170, 170, 170, 255 };

static Font   s_font;
static double s_started_at;
static bool   s_ready;
static int    s_touches;

void preload_start(void) {
    s_font = LoadFontEx("assets/fonts/Jersey15-Regular.ttf", 40, NULL, 0);
    s_started_at = GetTime();
    connection_open();
}

/* First unmet gate, NULL when the world is ready to show. */
static const char* pending(void) {
    if (!connection_is_open()) return "Connecting to Cyberia...";
    /* Assets start after the connection. Repeat calls do nothing. */
    presentation_runtime_start_fetch(CYBERIA_CLIENT_HINTS_CODE);
    if (!g_game_state.init_received || 0 == g_local_player.id) return "Entering the world...";
    if (!presentation_runtime_is_ready()) return "Preparing the view...";
    /* The two render checks also queue the P0 fetches they wait for. */
    if (!player_render_ready()) return "Loading your character...";
    if (!immediate_scene_ready()) return "Preparing your location...";
    if (!text_font_settled()) return "Preparing the interface...";
    return NULL;
}

/* A touch or mouse release, or a key press, this frame. On release, so the
 * pointer is up when gameplay starts and gives no world tap. */
static bool tapped(void) {
    const int touches = GetTouchPointCount();
    const bool touch_end = 0 == touches && 0 < s_touches;
    s_touches = touches;
    return touch_end || IsMouseButtonReleased(MOUSE_BUTTON_LEFT) || 0 != GetKeyPressed();
}

static void draw_centered(const char* text, float y, float size, Color color) {
    const Vector2 extent = MeasureTextEx(s_font, text, size, 2.0f);
    DrawTextEx(s_font, text, (Vector2){ (GetScreenWidth() - extent.x) * 0.5f, y }, size, 2.0f, color);
}

/* shell.html centres the CSS logo above y = h/2; the text goes below it. */
static void draw_splash(const char* label, const char* prompt) {
    const float y = GetScreenHeight() * 0.5f;
    BeginDrawing();
    ClearBackground(SPLASH_BG);
    draw_centered("CYBERIA", y + 10.0f, 40.0f, SPLASH_GOLD);
    draw_centered(label, y + 65.0f, 18.0f, NULL == prompt ? SPLASH_LABEL : SPLASH_GOLD);
    if (NULL != prompt && 0.5 > fmod(GetTime(), 1.0)) draw_centered(prompt, y + 100.0f, 30.0f, WHITE);
    EndDrawing();
}

/* Splash to gameplay. The camera starts on the player, so it does not pan
 * in from the boot position. */
static void enter_game(void) {
    prediction_reconcile();
    camera_init(GetScreenWidth(), GetScreenHeight());
    telemetry_event("gameplay_start");
    audio_start();
    modal_closed(); /* the splash is the first open modal */
    emscripten_run_script("document.getElementById('fx').remove()");
    UnloadFont(s_font);
}

bool preload_frame(void) {
    const float frame_dt = GetFrameTime();
    fetch_frame_begin(frame_dt, true);
    text_font_sync();
    game_client_on_tick();

    const char* label = s_ready ? NULL : pending();
    if (!s_ready && NULL == label) {
        s_ready = true;
        telemetry_event("tap_to_start_visible");
    }
    const bool stuck = !s_ready && PRELOAD_RELOAD_AFTER_S < GetTime() - s_started_at;
    telemetry_observe_scene(false);

    /* Read input before EndDrawing: its PollInputEvents clears the release. */
    const bool tap = tapped();
    draw_splash(s_ready ? "Your character is ready." : label,
                s_ready ? "TAP TO START" : stuck ? "TAP TO RELOAD" : NULL);
    audio_update(frame_dt);
    fetch_process_frame();
    fetch_frame_end();

    if (tap && stuck) emscripten_run_script("location.reload()");
    if (!tap || !s_ready) return false;
    enter_game();
    return true;
}
