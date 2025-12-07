#include <stdio.h>
#include <stdlib.h>

#include "raylib.h"
#include "render.h"
#include "client.h"
#include "config.h"

#include <emscripten/emscripten.h>

// External JavaScript function to set configuration (defined in js/services.js)
void js_set_config(const char* api_url, const char* assets_url);

// Application configuration
#define WINDOW_WIDTH 600
#define WINDOW_HEIGHT 800

// Application state
static struct {
    int running;
    int initialized;
} app_state = {0};

// Initialize all subsystems
int init_app(void) {
    printf("[INIT] Cyberia Client - Starting initialization...\n");

    // Synchronize JavaScript configuration with C config
    printf("[INIT] Step 0 - Synchronizing JavaScript configuration...\n");
    js_set_config(API_BASE_URL, ASSETS_BASE_URL);
    printf("[INIT] JavaScript config synchronized (API: %s)\n", API_BASE_URL);

    // Initialize rendering subsystem
    printf("[INIT] Step 1/2 - Initializing renderer...\n");
    if (0 != render_init(APP_NAME, WINDOW_WIDTH, WINDOW_HEIGHT)) {
        printf("[ERROR] Failed to initialize renderer\n");
        return -1;
    }
    printf("[INIT] Renderer initialized successfully\n");

    // Initialize client subsystem (WebSocket)
    printf("[INIT] Step 2/2 - Initializing network client...\n");
    if (0 != client_init()) {
        printf("[ERROR] Failed to initialize client\n");
        render_cleanup();
        return -1;
    }
    printf("[INIT] Network client initialized successfully\n");

    app_state.initialized = 1;

    printf("[INIT] All subsystems initialized - Ready!\n");
    return 0;
}

// Cleanup all subsystems
void cleanup_app(void) {
    if (!app_state.initialized) {
        return;
    }

    printf("[CLEANUP] Shutting down application...\n");

    client_cleanup();
    render_cleanup();

    app_state.initialized = 0;

    printf("[CLEANUP] Shutdown complete\n");
}

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
    if (init_app() != 0) {
        printf("[FATAL] Application initialization failed\n");
        return EXIT_FAILURE;
    }
    emscripten_set_main_loop(main_loop, 0, 1);

    cleanup_app();
    return EXIT_SUCCESS;
}