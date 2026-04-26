#include "render.h"
#include "game_render.h"
#include "floating_combat_text.h"
#include "input.h"
#include "dev_ui.h"
#include "modal_player.h"
#include "inventory_bar.h"
#include "inventory_modal.h"
#include "modal_dialogue.h"
#include "dialogue_data.h"
#include "interaction_bubble.h"
#include "tap_effect.h"
#include "ui_icon.h"
#include "raylib.h"
#include <stdio.h>
#include <assert.h>


// Default configuration for fallback rendering
// Forward declaration
void render_fallback(int width, int height);

// Global state for rendering
struct {
    Texture2D splash_texture;
} render_state = {0};

// Initialize the rendering subsystem
void render_init(int width, int height) {
    // Load splash texture
    render_state.splash_texture = LoadTexture("splash.png");

    // Initialize game-specific rendering systems
    game_render_init(width, height);

    // Initialize development UI
    if (0 != dev_ui_init()) {
        printf("[RENDER] Failed to initialize development UI\n");
    }

    // Initialize player modal component
    if (0 != modal_player_init()) {
        printf("[RENDER] Failed to initialize player modal component\n");
    }

    tap_effect_init();
}

// Main rendering loop iteration
void render_update(float delta_time) {
    // Get current canvas dimensions (handles browser resize)
    int current_width = GetScreenWidth();
    int current_height = GetScreenHeight();

    // Update stored dimensions if canvas was resized
    if (IsWindowResized()) {
        // Update game renderer screen size
        game_render_set_screen_size(current_width, current_height);

        // Update camera offset to keep it centered
        game_state_update_camera_offset(current_width, current_height);

        // Handle input resize event
        input_handle_window_resize(current_width, current_height);
    }

    game_state_update_camera(delta_time); // TODO: camera shouldn't be in the GameState

    // Update effects (click effects, floating texts, FCT pop-ups)
    game_render_update_effects(delta_time);
    fct_update(delta_time);
    tap_effect_update(delta_time);

    // Update inventory UI
    inventory_bar_update(delta_time);
    if (inventory_modal_is_open()) {
        inventory_modal_update(delta_time);
    }
    if (modal_dialogue_is_open()) {
        modal_dialogue_update(delta_time);
    }

    // Poll async dialogue data fetches and rebuild bubble column
    dialogue_data_poll();
    ui_icon_poll();
    interaction_bubble_update();

    // Update dev UI
    dev_ui_update(delta_time);

    // Update player modal
    modal_player_update(delta_time);

    // Use game renderer if available and game state is initialized
    if (g_game_state.init_received) {
        // Full game rendering
        game_render_frame();
    } else {
        // Fallback rendering for when game isn't ready yet
        render_fallback(current_width, current_height);
    }
}

// Fallback rendering when game systems aren't ready
void render_fallback(int width, int height) {
    BeginDrawing();
    // Clear background to game background color (reduces flicker)
    ClearBackground(DARKGRAY);

    // Calculate center position
    float center_x = (float)width / 2.0f;
    float center_y = (float)height / 2.0f;

    // Draw splash logo if loaded
    if (render_state.splash_texture.id != 0) {
        DrawTexture(render_state.splash_texture,
            (int)(center_x - render_state.splash_texture.width / 2),
            (int)(center_y - render_state.splash_texture.height / 2),
            WHITE);
    }

    // Draw connection status
    const char* status_text = "Connecting to server...";
    int text_width = MeasureText(status_text, 20);
    DrawText(status_text, (width - text_width) / 2, height - 40, 20, WHITE);
    EndDrawing();
}

// Cleanup rendering subsystem
void render_cleanup(void) {
    // Unload splash texture
    UnloadTexture(render_state.splash_texture);

    tap_effect_reset();

    // Cleanup game renderer
    game_render_cleanup();

    // Cleanup development UI
    dev_ui_cleanup();

    // Cleanup player modal component
    modal_player_cleanup();
}
