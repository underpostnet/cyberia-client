#include "render.h"
#include "raylib.h"
#include <string.h>
#include <stdio.h>

#if defined(PLATFORM_WEB)
    #include <emscripten/emscripten.h>
#endif

// Maximum length for displayed text
#define MAX_TEXT_LENGTH 4096

// Global state for rendering
static struct {
    int initialized;
    int width;
    int height;
    char display_text[MAX_TEXT_LENGTH];
    int text_updated;
} render_state = {0};

// Initialize the rendering subsystem
int render_init(const char* title, int width, int height) {
    if (render_state.initialized) {
        printf("Render: already initialized\n");
        return 0;
    }

    render_state.width = width;
    render_state.height = height;
    strncpy(render_state.display_text, "Connecting...", MAX_TEXT_LENGTH - 1);
    render_state.display_text[MAX_TEXT_LENGTH - 1] = '\0';
    render_state.text_updated = 1;

    InitWindow(width, height, title);
    SetTargetFPS(60);

    render_state.initialized = 1;
    printf("Render: initialized (%dx%d)\n", width, height);

    return 0;
}

// Main rendering loop iteration
void render_update(void) {
    if (!render_state.initialized) {
        return;
    }

    BeginDrawing();
    ClearBackground(RAYWHITE);

    // Draw title
    DrawText("MMO Client - WebSocket Status", 20, 20, 20, DARKGRAY);
    DrawLine(20, 50, render_state.width - 20, 50, LIGHTGRAY);

    // Draw the last received JSON message
    DrawText("Last Message:", 20, 70, 18, DARKBLUE);

    // Word wrap the text for better display
    const int max_line_width = render_state.width - 40;
    const int font_size = 16;
    const int line_spacing = 5;
    int y_offset = 100;

    // Simple text display (raylib handles basic wrapping with MeasureText)
    // For production, you'd want more sophisticated text wrapping
    const char* text_ptr = render_state.display_text;
    char line_buffer[256];
    int line_length = 0;

    while (*text_ptr) {
        line_buffer[line_length] = *text_ptr;
        line_buffer[line_length + 1] = '\0';

        int text_width = MeasureText(line_buffer, font_size);

        if (text_width > max_line_width || *text_ptr == '\n') {
            if (*text_ptr == '\n') {
                line_buffer[line_length] = '\0';
            } else {
                // Backtrack to last space
                int i = line_length;
                while (i > 0 && line_buffer[i] != ' ') {
                    i--;
                }
                if (i > 0) {
                    line_buffer[i] = '\0';
                    text_ptr -= (line_length - i);
                } else {
                    line_buffer[line_length] = '\0';
                }
            }

            DrawText(line_buffer, 20, y_offset, font_size, DARKGRAY);
            y_offset += font_size + line_spacing;
            line_length = 0;

            if (y_offset > render_state.height - 50) {
                DrawText("... (message truncated)", 20, y_offset, font_size, LIGHTGRAY);
                break;
            }
        } else {
            line_length++;
        }

        text_ptr++;
    }

    // Draw remaining text
    if (line_length > 0 && y_offset < render_state.height - 50) {
        DrawText(line_buffer, 20, y_offset, font_size, DARKGRAY);
    }

    // Draw FPS counter
    DrawFPS(render_state.width - 100, 10);

    EndDrawing();
}

// Update the text to be displayed
void render_set_text(const char* text) {
    if (!text) {
        return;
    }

    strncpy(render_state.display_text, text, MAX_TEXT_LENGTH - 1);
    render_state.display_text[MAX_TEXT_LENGTH - 1] = '\0';
    render_state.text_updated = 1;

    printf("Render: text updated (%zu bytes)\n", strlen(text));
}

// Cleanup rendering subsystem
void render_cleanup(void) {
    if (!render_state.initialized) {
        return;
    }

    CloseWindow();
    render_state.initialized = 0;
    printf("Render: cleanup complete\n");
}

// Check if window should close
int render_should_close(void) {
    return WindowShouldClose();
}