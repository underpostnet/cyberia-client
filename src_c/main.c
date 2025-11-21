#include "render.h"
#include "client.h"
#include <stdio.h>
#include <stdlib.h>

#if defined(PLATFORM_WEB)
    #include <emscripten/emscripten.h>
#endif

// Application configuration
#define WINDOW_WIDTH 1024
#define WINDOW_HEIGHT 768
#define WINDOW_TITLE "Cyberia MMO Client"

// Forward declaration of main loop function
static void main_loop(void);

// Application state
static struct {
    int running;
    int initialized;
} app_state = {0};

// Initialize all subsystems
static int init_app(void) {
    printf("==============================================\n");
    printf("  Cyberia MMO Client - Initializing...\n");
    printf("==============================================\n");

    // Initialize rendering subsystem
    printf("\n[1/2] Initializing renderer...\n");
    if (render_init(WINDOW_TITLE, WINDOW_WIDTH, WINDOW_HEIGHT) != 0) {
        printf("ERROR: Failed to initialize renderer\n");
        return -1;
    }

    // Initialize client subsystem (WebSocket)
    printf("\n[2/2] Initializing client...\n");
    if (client_init() != 0) {
        printf("ERROR: Failed to initialize client\n");
        render_cleanup();
        return -1;
    }

    app_state.initialized = 1;
    app_state.running = 1;

    printf("\n==============================================\n");
    printf("  All subsystems initialized successfully!\n");
    printf("==============================================\n\n");

    return 0;
}

// Cleanup all subsystems
static void cleanup_app(void) {
    if (!app_state.initialized) {
        return;
    }

    printf("\n==============================================\n");
    printf("  Shutting down Cyberia MMO Client...\n");
    printf("==============================================\n");

    client_cleanup();
    render_cleanup();

    app_state.initialized = 0;
    app_state.running = 0;

    printf("Shutdown complete.\n");
}

// Main event loop (called every frame)
static void main_loop(void) {
    // Check if window should close
    if (render_should_close()) {
        app_state.running = 0;
#if defined(PLATFORM_WEB)
        // In web builds, we need to explicitly cancel the main loop
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
        printf("FATAL: Application initialization failed\n");
        return EXIT_FAILURE;
    }

#if defined(PLATFORM_WEB)
    // Web platform: Use Emscripten's main loop
    printf("Running in WebAssembly mode...\n");
    emscripten_set_main_loop(main_loop, 60, 1);
#else
    // Desktop platform: Traditional game loop
    printf("Running in desktop mode...\n");
    while (app_state.running) {
        main_loop();
    }
    cleanup_app();
#endif

    return EXIT_SUCCESS;
}