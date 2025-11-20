#include "raylib.h"

#if defined(PLATFORM_WEB)
    #include <emscripten/emscripten.h>
#endif

static void UpdateDrawFrame(void);    // Main loop function

int main(void)
{
    InitWindow(800, 450, "raylib WASM Hello World");
    SetTargetFPS(60);

#if defined(PLATFORM_WEB)
    // In Web builds, Emscripten controls the main loop
    emscripten_set_main_loop(UpdateDrawFrame, 60, 1);
#else
    // Desktop: normal while-loop
    while (!WindowShouldClose()) {
        UpdateDrawFrame();
    }
#endif

    CloseWindow();
    return 0;
}

static void UpdateDrawFrame(void)
{
    BeginDrawing();

    ClearBackground(RAYWHITE);
    DrawText("Hello from raylib WebAssembly!", 180, 200, 20, DARKGRAY);

    EndDrawing();
}
