#include "render.h"
#include "client.h"
#include "config.h"
#include <stdio.h>
#include <stdlib.h>

#if defined(PLATFORM_WEB)
    #include <emscripten/emscripten.h>
    
    // External JavaScript function to set configuration (defined in js/services.js)
    extern void js_set_config(const char* api_url, const char* assets_url);
#endif

// Application configuration
#define WINDOW_WIDTH 600
#define WINDOW_HEIGHT 800

// Forward declaration of main loop function
static void main_loop(void);

// Application state
static struct {
    int running;
    int initialized;
} app_state = {0};

// Initialize all subsystems
static int init_app(void) {
    printf("[INIT] Cyberia Client - Starting initialization...\n");

#if defined(PLATFORM_WEB)
    // Synchronize JavaScript configuration with C config
    printf("[INIT] Step 0 - Synchronizing JavaScript configuration...\n");
    js_set_config(API_BASE_URL, ASSETS_BASE_URL);
    printf("[INIT] JavaScript config synchronized (API: %s)\n", API_BASE_URL);
#endif

    // Initialize rendering subsystem
    printf("[INIT] Step 1/2 - Initializing renderer...\n");
    if (render_init(APP_NAME, WINDOW_WIDTH, WINDOW_HEIGHT) != 0) {
        printf("[ERROR] Failed to initialize renderer\n");
        return -1;
    }
    printf("[INIT] Renderer initialized successfully\n");

    // Initialize client subsystem (WebSocket)
    printf("[INIT] Step 2/2 - Initializing network client...\n");
    if (client_init() != 0) {
        printf("[ERROR] Failed to initialize client\n");
        render_cleanup();
        return -1;
    }
    printf("[INIT] Network client initialized successfully\n");

    app_state.initialized = 1;
    app_state.running = 1;

    printf("[INIT] All subsystems initialized - Ready!\n");
    return 0;
}

// Cleanup all subsystems
static void cleanup_app(void) {
    if (!app_state.initialized) {
        return;
    }

    printf("[CLEANUP] Shutting down application...\n");

    client_cleanup();
    render_cleanup();

    app_state.initialized = 0;
    app_state.running = 0;

    printf("[CLEANUP] Shutdown complete\n");
}

// Main event loop (called every frame)
static void main_loop(void) {
    // Check if window should close
    if (render_should_close()) {
        app_state.running = 0;
#if defined(PLATFORM_WEB)
        emscripten_cancel_main_loop();
#endif
        cleanup_app();
        return;
    }

    // Update client subsystem (process WebSocket events)
    client_update();

    // Update and render the frame
    render_update();
}

// Main entry point
int main(int argc, char* argv[]) {
    // Initialize the application
    if (init_app() != 0) {
        printf("[FATAL] Application initialization failed\n");
        return EXIT_FAILURE;
    }

#if defined(PLATFORM_WEB)
    // Web platform: Use Emscripten's main loop
    printf("[MAIN] Running in WebAssembly mode\n");
    emscripten_set_main_loop(main_loop, 0, 1);
#else
    // Desktop platform: Traditional game loop
    printf("[MAIN] Running in desktop mode\n");
    while (app_state.running) {
        main_loop();
    }
    cleanup_app();
#endif

    return EXIT_SUCCESS;
}
