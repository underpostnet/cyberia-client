#include "render.h"

#include "dialogue_data.h"
#include "domain/camera.h"
#include "game_render.h"
#include "object_layers_management.h"
#include "ui/dev_ui.h"
#include "ui/floating_combat_text.h"
#include "ui/interaction_bubble.h"
#include "ui/inventory_bar.h"
#include "ui/inventory_modal.h"
#include "ui/modal_dialogue.h"
#include "ui/modal_player.h"
#include "ui/tap_effect.h"
#include "ui/ui_icon.h"
#include "network/engine_client.h"
#include "util/log.h"

#include <assert.h>
#include <raylib.h>

/* render.c — top-level frame orchestrator. Owns nothing; only sequences
 * the camera update, effect timers, UI ticks, and the world-render pass.
 * The Camera2D itself lives in domain/camera.{c,h}. */

struct {
    Texture2D splash_texture;
} render_state = {0};

void render_fallback(int width, int height);

void render_init(int width, int height) {
    render_state.splash_texture = LoadTexture("splash.png");

    game_render_init(width, height);

    if (0 != dev_ui_init()) {
        LOG_WARN("dev_ui_init failed");
    }
    if (0 != modal_player_init()) {
        LOG_WARN("modal_player_init failed");
    }

    tap_effect_init();
    camera_init(width, height);
}

void render_on_tick(float delta_time) {
    int current_width  = GetScreenWidth();
    int current_height = GetScreenHeight();

    // TODO: window should be resized via emscripten callback, no js, raylib does not work
    if (IsWindowResized()) {
        LOG_INFO("window resized %dx%d", current_width, current_height);
        game_render_set_screen_size(current_width, current_height);
        camera_resize(current_width, current_height);
    }

    camera_on_tick(delta_time);

    game_render_update_effects(delta_time);
    fct_update(delta_time);
    tap_effect_update(delta_time);

    inventory_bar_update(delta_time);
    if (inventory_modal_is_open())  inventory_modal_update(delta_time);
    if (modal_dialogue_is_open())   modal_dialogue_update(delta_time);

    interaction_bubble_update();
    dev_ui_on_tick(delta_time);
    modal_player_update(delta_time);

    if (g_game_state.init_received) {
        game_render_frame();
    } else {
        render_fallback(current_width, current_height);
    }
}

void render_fallback(int width, int height) {
    BeginDrawing();
    ClearBackground(DARKGRAY);

    float cx = (float)width  / 2.0f;
    float cy = (float)height / 2.0f;

    if (render_state.splash_texture.id != 0) {
        DrawTexture(render_state.splash_texture,
                    (int)(cx - render_state.splash_texture.width  / 2),
                    (int)(cy - render_state.splash_texture.height / 2),
                    WHITE);
    }

    const char* status_text = "Connecting to server...";
    int text_width = MeasureText(status_text, 20);
    DrawText(status_text, (width - text_width) / 2, height - 40, 20, WHITE);
    EndDrawing();
}

void render_cleanup(void) {
    UnloadTexture(render_state.splash_texture);
    tap_effect_reset();
    game_render_cleanup();
    dev_ui_cleanup();
    modal_player_cleanup();
}
