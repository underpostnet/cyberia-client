#include <stdio.h>
#include <stdlib.h>

#include "input.h"
#include "game_state.h"
#include "render.h"
#include "client.h"
#include "config.h"

#include <raylib.h>
#include <emscripten/emscripten.h>

#include "js/services.h"

// Main event loop (called every frame)
void main_loop(void) {
    const float dt = GetFrameTime();

    // Update input system at the beggining of the frame
    input_update();

    // Process queued events
    input_process_events();

    // update frame state
    game_state_update_interpolation(dt);

    // render the frame/
    render_update(dt);
}

// Main entry point
int main(void) {
    // Initialize engine API base URL for public API access
    js_init_engine_api(API_BASE_URL);

    // Read the browser viewport size so the canvas fills the full screen.
    // Subsequent browser resizes are handled by IsWindowResized() in render_update.
    int vp_w = EM_ASM_INT({ return window.innerWidth; });
    int vp_h = EM_ASM_INT({ return window.innerHeight; });

    // Initialize window with raylib
    InitWindow(vp_w, vp_h, NULL);
    SetTargetFPS(60);

    // Post Raylib init, init
    render_init(vp_w, vp_h);

    // post window init - init ws
    if (0 != client_init())
    {
        return EXIT_FAILURE;
    }

    emscripten_set_main_loop(main_loop, 0, 1);

    client_cleanup();
    input_cleanup();
    render_cleanup();

    CloseWindow();
    return EXIT_SUCCESS;
}
