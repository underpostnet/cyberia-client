#include "render.h"
#include "game_render.h"
#include "game_state.h"
#include "input.h"
#include "dev_ui.h"
#include "modal.h"
#include "modal_player.h"
#include "raylib.h"
#include <stdio.h>

#if defined(PLATFORM_WEB)
    #include <emscripten/emscripten.h>
#endif

// Default configuration for fallback rendering
#define BORDER_WIDTH 5
#define BORDER_COLOR (Color){0, 100, 255, 255}  // Blue border
#define CIRCLE_COLOR RED

// Forward declaration
static void render_fallback(int width, int height);

// Global state for rendering
static struct {
    int initialized;
    int width;
    int height;
    bool game_initialized;
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
static void render_fallback(int width, int height) {
    BeginDrawing();

    // Clear background to game background color (reduces flicker)
    ClearBackground((Color){30, 30, 30, 255});

    // Calculate center position
    float center_x = (float)width / 2.0f;
    float center_y = (float)height / 2.0f;

    // Calculate circle radius (relative to screen size for responsiveness)
    float radius = (width < height ? width : height) * 0.1f;

    // Draw red circle in the center
    DrawCircle((int)center_x, (int)center_y, radius, CIRCLE_COLOR);

    // Draw blue border around the edges
    DrawRectangle(0, 0, width, BORDER_WIDTH, BORDER_COLOR);
    DrawRectangle(0, height - BORDER_WIDTH, width, BORDER_WIDTH, BORDER_COLOR);
    DrawRectangle(0, 0, BORDER_WIDTH, height, BORDER_COLOR);
    DrawRectangle(width - BORDER_WIDTH, 0, BORDER_WIDTH, height, BORDER_COLOR);

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

// Check if window should close
int render_should_close(void) {
    return WindowShouldClose();
}
