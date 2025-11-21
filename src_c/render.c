#include "render.h"
#include "raylib.h"
#include <stdio.h>

#if defined(PLATFORM_WEB)
    #include <emscripten/emscripten.h>
#endif

// Border configuration
#define BORDER_WIDTH 5
#define BORDER_COLOR (Color){0, 100, 255, 255}  // Blue border

// Circle configuration
#define CIRCLE_COLOR RED

// Global state for rendering
static struct {
    int initialized;
    int width;
    int height;
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
    printf("[RENDER] Initialized successfully (%dx%d)\n", width, height);

    return 0;
}

// Main rendering loop iteration
void render_update(void) {
    if (!render_state.initialized) {
        return;
    }

    // Get current canvas dimensions (handles browser resize)
    int current_width = GetScreenWidth();
    int current_height = GetScreenHeight();

    // Update stored dimensions if canvas was resized
    if (current_width != render_state.width || current_height != render_state.height) {
        render_state.width = current_width;
        render_state.height = current_height;
        printf("[RENDER] Canvas resized to %dx%d\n", current_width, current_height);
    }

    BeginDrawing();

    // Clear background to black for immersive experience
    ClearBackground(BLACK);

    // Calculate center position
    float center_x = (float)current_width / 2.0f;
    float center_y = (float)current_height / 2.0f;

    // Calculate circle radius (relative to screen size for responsiveness)
    // Use 10% of the smaller dimension
    float radius = (current_width < current_height ? current_width : current_height) * 0.1f;

    // Draw red circle in the center
    DrawCircle((int)center_x, (int)center_y, radius, CIRCLE_COLOR);

    // Draw blue border around the edges
    // Top border
    DrawRectangle(0, 0, current_width, BORDER_WIDTH, BORDER_COLOR);
    
    // Bottom border
    DrawRectangle(0, current_height - BORDER_WIDTH, current_width, BORDER_WIDTH, BORDER_COLOR);
    
    // Left border
    DrawRectangle(0, 0, BORDER_WIDTH, current_height, BORDER_COLOR);
    
    // Right border
    DrawRectangle(current_width - BORDER_WIDTH, 0, BORDER_WIDTH, current_height, BORDER_COLOR);

    EndDrawing();
}

// Cleanup rendering subsystem
void render_cleanup(void) {
    if (!render_state.initialized) {
        return;
    }

    CloseWindow();
    render_state.initialized = 0;
    printf("[RENDER] Cleanup complete\n");
}

// Check if window should close
int render_should_close(void) {
    return WindowShouldClose();
}