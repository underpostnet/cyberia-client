#include <stdio.h>
#include <stdlib.h>

#include "raylib.h"
#include "render.h"
#include "client.h"
#include "config.h"

#include <emscripten/emscripten.h>

// External JavaScript function to initialize engine API base URL
// (defined in js/services.js)
void js_init_engine_api(const char* api_base_url);

// Application configuration
#define WINDOW_WIDTH 600
#define WINDOW_HEIGHT 800

// Main event loop (called every frame)
void main_loop(void) {
    // Check if window should close
    if (WindowShouldClose()) {
        emscripten_cancel_main_loop();
    }

    // Update and render the frame
    render_update();
}

// Main entry point
int main(/*int argc, char* argv[]*/) {
    // Initialize engine API base URL for public API access
    js_init_engine_api(API_BASE_URL);

    // Initialize window with raylib
    InitWindow(WINDOW_WIDTH, WINDOW_HEIGHT, NULL);
    SetTargetFPS(60);

    // Post Raylib init, init
    render_init(WINDOW_WIDTH, WINDOW_HEIGHT);

    // post window init - init ws
    if (0 != client_init())
    {
        return EXIT_FAILURE;
    }

    emscripten_set_main_loop(main_loop, 0, 1);

    client_cleanup();
    render_cleanup();

    CloseWindow();
    return EXIT_SUCCESS;
}