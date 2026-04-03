#include "modal_player.h"
#include "game_render.h"
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
    const char* map_code = g_game_state.player.map_code;
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
            snprintf(line_buffer, sizeof(line_buffer), "Map: %s", map_code[0] ? map_code : "--");
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

    // Add coin count line (number only — icon drawn separately in modal_player_draw)
    if (init_received) {
        int coins = game_state_get_player_coins();
        snprintf(line_buffer, sizeof(line_buffer), "%d", coins);
        modal_add_line(&g_modal_player.modal, line_buffer, g_game_state.colors.coin);
    }

    // Update modal animation state
    modal_update_struct(&g_modal_player.modal, delta_time);
}

void modal_player_draw(int screen_width, int screen_height) {
    modal_draw_struct(&g_modal_player.modal, screen_width, screen_height);

    // Draw coin icon to the left of the coin number (last modal line)
    if (g_game_state.init_received && g_modal_player.modal.line_count > 0) {
        const Modal* m = &g_modal_player.modal;

        // Replicate modal layout to find the coin line's rendered position
        int max_text_width = 0;
        for (int i = 0; i < m->line_count; i++) {
            int w = MeasureText(m->lines[i].text, m->font_size);
            if (w > max_text_width) max_text_width = w;
        }
        int modal_width = max_text_width + (m->padding * 2);
        if (modal_width < m->min_width) modal_width = m->min_width;
        int modal_x = screen_width - modal_width - m->margin_right;

        int coin_idx = m->line_count - 1;
        int coin_y = m->margin_top + m->padding + (coin_idx * m->line_spacing);
        int coin_text_w = MeasureText(m->lines[coin_idx].text, m->font_size);
        int coin_text_x = modal_x + (modal_width - coin_text_w) / 2; // CENTER aligned

        int icon_size = m->font_size;
        const EntityTypeDefault* _cdef = game_state_get_entity_default("coin");
        const char* _ckey = (_cdef && _cdef->live_item_id_count > 0) ? _cdef->live_item_ids[0] : NULL;
        game_render_draw_object_layer_animated_ico(_ckey, coin_text_x - icon_size - 4, coin_y, icon_size);
    }
}

