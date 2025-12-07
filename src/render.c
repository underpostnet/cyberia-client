#include "render.h"
#include "game_render.h"
#include "game_state.h"
#include "input.h"
#include "dev_ui.h"
#include "modal.h"
#include "modal_player.h"
#include "raylib.h"
#include <stdio.h>

#include <emscripten/emscripten.h>


// Default configuration for fallback rendering
// Forward declaration
void render_fallback(int width, int height);

// Global state for rendering
struct {
    int initialized;
    int width;
    int height;
    bool game_initialized;
    Texture2D splash_texture;
} render_state = {0};

// Initialize the rendering subsystem
int render_init(const char* title, int width, int height) {
    if (render_state.initialized) {
        printf("[RENDER] Already initialized\n");
        return 0;
    }

    render_state.width = width;
    render_state.height = height;

    // Initialize window with raylib
    InitWindow(width, height, title);

    // Set target FPS for smooth rendering
    SetTargetFPS(60);

    render_state.initialized = 1;
    printf("[RENDER] Basic renderer initialized successfully (%dx%d)\n", width, height);

    // Load splash texture
    render_state.splash_texture = LoadTexture("splash.png");
    if (render_state.splash_texture.id == 0) {
        printf("[RENDER] Failed to load splash texture 'splash.png'\n");
    }

    // Initialize game-specific rendering systems
    if (game_render_init(width, height) == 0) {
        render_state.game_initialized = true;
        printf("[RENDER] Game renderer initialized successfully\n");
    } else {
        printf("[RENDER] Failed to initialize game renderer, using fallback\n");
        render_state.game_initialized = false;
    }

    // Initialize development UI
    if (dev_ui_init() == 0) {
        printf("[RENDER] Development UI initialized successfully\n");
    } else {
        printf("[RENDER] Failed to initialize development UI\n");
    }

    // Initialize player modal component
    if (modal_player_init() == 0) {
        printf("[RENDER] Player modal component initialized successfully\n");
    } else {
        printf("[RENDER] Failed to initialize player modal component\n");
    }

    return 0;
}

// Main rendering loop iteration
void render_update(void) {
    if (!render_state.initialized) {
        return;
    }

    // Update input system first
    input_update();

    // Get current canvas dimensions (handles browser resize)
    int current_width = GetScreenWidth();
    int current_height = GetScreenHeight();

    // Update stored dimensions if canvas was resized
    if (current_width != render_state.width || current_height != render_state.height) {
        render_state.width = current_width;
        render_state.height = current_height;
        printf("[RENDER] Canvas resized to %dx%d\n", current_width, current_height);

        // Update game renderer screen size
        if (render_state.game_initialized) {
            game_render_set_screen_size(current_width, current_height);
        }

        // Update camera offset to keep it centered
        game_state_update_camera_offset(current_width, current_height);

        // Handle input resize event
        input_handle_window_resize(current_width, current_height);
    }

    // Update game state interpolation
    static double last_time = 0.0;
    double current_time = GetTime();
    float delta_time = (float)(current_time - last_time);
    last_time = current_time;

    game_state_update_interpolation(delta_time);
    game_state_update_camera();

    // Update effects (click effects, floating texts)
    game_render_update_effects(delta_time);

    // Update dev UI
    dev_ui_update(delta_time);

    // Update player modal
    modal_player_update(delta_time);

    // Use game renderer if available and game state is initialized
    if (render_state.game_initialized && g_game_state.init_received) {
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
    ClearBackground((Color){30, 30, 30, 255});

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
    if (!render_state.initialized) {
        return;
    }

    // Unload splash texture
    if (render_state.splash_texture.id != 0) {
        UnloadTexture(render_state.splash_texture);
    }

    // Cleanup game renderer
    if (render_state.game_initialized) {
        game_render_cleanup();
        render_state.game_initialized = false;
    }

    // Cleanup development UI
    dev_ui_cleanup();

    // Cleanup player modal component
    modal_player_cleanup();

    // Cleanup input system
    input_cleanup();

    CloseWindow();
    render_state.initialized = 0;
    printf("[RENDER] Cleanup complete\n");
}
