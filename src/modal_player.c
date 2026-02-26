#include "modal_player.h"
#include "game_state.h"
#include "client.h"
#include <stdio.h>
#include <string.h>

// Global player modal instance
ModalPlayer g_modal_player = {0};

int modal_player_init(void) {
    printf("[MODAL_PLAYER] Initializing player modal component...\n");

    memset(&g_modal_player, 0, sizeof(ModalPlayer));

    // Initialize the underlying modal
    if (modal_init_struct(&g_modal_player.modal) != 0) {
        printf("[MODAL_PLAYER] Failed to initialize modal structure\n");
        return -1;
    }

    // Configure modal position and style
    modal_set_position(&g_modal_player.modal, MODAL_POS_TOP_RIGHT, 10, 10, 0, 0);
    modal_set_style(&g_modal_player.modal,
                   (Color){0, 0, 0, 200},
                   (Color){100, 100, 100, 200},
                   0.78f);
    modal_set_font(&g_modal_player.modal, 16, 22);
    modal_set_text_alignment(&g_modal_player.modal, MODAL_ALIGN_CENTER);

    // Set display options (show all by default)
    g_modal_player.show_connection = true;
    g_modal_player.show_map = true;
    g_modal_player.show_position = true;
    g_modal_player.show_fps = true;

    // Initialize FPS tracking
    g_modal_player.cached_fps = 60.0f;
    g_modal_player.last_fps_update = 0.0;

    printf("[MODAL_PLAYER] Player modal component initialized\n");
    return 0;
}

void modal_player_cleanup(void) {
    printf("[MODAL_PLAYER] Cleaning up player modal component...\n");
    memset(&g_modal_player, 0, sizeof(ModalPlayer));
}

void modal_player_update(float delta_time) {
    // Update FPS tracking
    double current_time = GetTime();
    if (current_time - g_modal_player.last_fps_update >= 0.5) {
        g_modal_player.cached_fps = GetFPS();
        g_modal_player.last_fps_update = current_time;
    }

    // Clear existing lines
    modal_clear_lines(&g_modal_player.modal);

    // Get game state data
    bool init_received = g_game_state.init_received;
    int map_id = g_game_state.player.map_id;
    Vector2 pos = g_game_state.player.base.interp_pos;

    // Get connection status
    bool is_connected = client_is_connected();

    char line_buffer[MODAL_MAX_LINE_LENGTH];

    // Add connection status line
    if (g_modal_player.show_connection) {
        const char* status_text = is_connected ? "Connected" : "Disconnected";
        Color status_color = is_connected ? GREEN : RED;
        modal_add_line(&g_modal_player.modal, status_text, status_color);
    }

    // Add map ID line
    if (g_modal_player.show_map) {
        if (init_received) {
            snprintf(line_buffer, sizeof(line_buffer), "Map: %d", map_id);
        } else {
            snprintf(line_buffer, sizeof(line_buffer), "Map: --");
        }
        modal_add_line(&g_modal_player.modal, line_buffer, YELLOW);
    }

    // Add position line
    if (g_modal_player.show_position) {
        if (init_received) {
            snprintf(line_buffer, sizeof(line_buffer), "Pos: (%.1f, %.1f)", pos.x, pos.y);
        } else {
            snprintf(line_buffer, sizeof(line_buffer), "Pos: (--, --)");
        }
        modal_add_line(&g_modal_player.modal, line_buffer, YELLOW);
    }

    // Add FPS line
    if (g_modal_player.show_fps) {
        snprintf(line_buffer, sizeof(line_buffer), "FPS: %.0f", g_modal_player.cached_fps);
        modal_add_line(&g_modal_player.modal, line_buffer, WHITE);
    }

    // Update modal animation state
    modal_update_struct(&g_modal_player.modal, delta_time);
}

void modal_player_draw(int screen_width, int screen_height) {
    modal_draw_struct(&g_modal_player.modal, screen_width, screen_height);
}

void modal_player_set_display_options(bool show_connection, bool show_map,
                                      bool show_position, bool show_fps) {
    g_modal_player.show_connection = show_connection;
    g_modal_player.show_map = show_map;
    g_modal_player.show_position = show_position;
    g_modal_player.show_fps = show_fps;
}

void modal_player_set_position(int position_mode, int margin_top, int margin_right) {
    modal_set_position(&g_modal_player.modal, position_mode,
                      margin_top, margin_right, 0, 0);
}