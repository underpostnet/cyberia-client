#include "dev_ui.h"
#include "game_state.h"
#include "client.h"
#include <stdio.h>
#include <string.h>

const char* direction_to_string(Direction dir) {
    switch (dir) {
        case DIRECTION_UP: return "UP";
        case DIRECTION_UP_RIGHT: return "UP_RIGHT";
        case DIRECTION_RIGHT: return "RIGHT";
        case DIRECTION_DOWN_RIGHT: return "DOWN_RIGHT";
        case DIRECTION_DOWN: return "DOWN";
        case DIRECTION_DOWN_LEFT: return "DOWN_LEFT";
        case DIRECTION_LEFT: return "LEFT";
        case DIRECTION_UP_LEFT: return "UP_LEFT";
        case DIRECTION_NONE: return "NONE";
        default: return "UNKNOWN";
    }
}

const char* mode_to_string(ObjectLayerMode mode) {
    switch (mode) {
        case MODE_IDLE: return "IDLE";
        case MODE_WALKING: return "WALKING";
        case MODE_TELEPORTING: return "TELEPORTING";
        default: return "UNKNOWN";
    }
}

// Global dev UI instance
DevUI g_dev_ui = {0};

int dev_ui_init(void) {
    printf("[DEV_UI] Initializing development UI...\n");

    memset(&g_dev_ui, 0, sizeof(DevUI));

    // Set default dimensions
    g_dev_ui.dev_ui_width = 450;
    g_dev_ui.dev_ui_height = 280;
    g_dev_ui.background_alpha = 0.4f;

    // Set default colors
    g_dev_ui.background_color = (Color){0, 0, 0, (unsigned char)(255 * 0.4f)};
    g_dev_ui.text_color = (Color){255, 255, 255, 255};
    g_dev_ui.debug_text_color = (Color){220, 220, 220, 255};
    g_dev_ui.error_text_color = (Color){255, 50, 50, 255};

    // Network stats
    g_dev_ui.download_kbps = 0.0f;
    g_dev_ui.upload_kbps = 0.0f;
    g_dev_ui.last_network_update = 0.0;
    g_dev_ui.last_download_bytes = 0;
    g_dev_ui.last_upload_bytes = 0;

    // Performance tracking
    g_dev_ui.last_fps = 60;
    g_dev_ui.last_fps_update = 0.0;

    // Display toggles (all on by default)
    g_dev_ui.show_network_stats = true;
    g_dev_ui.show_player_stats = true;
    g_dev_ui.show_game_stats = true;
    g_dev_ui.show_error_section = true;

    printf("[DEV_UI] Development UI initialized\n");
    return 0;
}

void dev_ui_cleanup(void) {
    printf("[DEV_UI] Cleaning up development UI...\n");
    memset(&g_dev_ui, 0, sizeof(DevUI));
}

void dev_ui_update(float delta_time) {
    // Update FPS tracking
    double current_time = GetTime();
    if (current_time - g_dev_ui.last_fps_update >= 0.1) {
        g_dev_ui.last_fps = GetFPS();
        g_dev_ui.last_fps_update = current_time;
    }

    // Update network statistics from client and game_state
    // Only update every 0.5 seconds to avoid flickering
    if (current_time - g_dev_ui.last_network_update >= 0.5) {
        size_t download_bytes = 0;
        size_t upload_bytes = 0;

        client_get_network_stats(&download_bytes, &upload_bytes);
        dev_ui_update_network_stats(download_bytes, upload_bytes);
    }
}

void dev_ui_update_network_stats(size_t download_bytes, size_t upload_bytes) {
    double current_time = GetTime();
    double time_delta = current_time - g_dev_ui.last_network_update;

    // Calculate kbps if we have a valid time delta (minimum 0.1 seconds)
    if (time_delta >= 0.1 && g_dev_ui.last_network_update > 0.0) {
        // Calculate bytes transferred
        size_t download_delta = download_bytes - g_dev_ui.last_download_bytes;
        size_t upload_delta = upload_bytes - g_dev_ui.last_upload_bytes;

        // Convert to kbps (bytes/sec * 8 / 1000)
        float new_download_kbps = (download_delta / time_delta) * 8.0f / 1000.0f;
        float new_upload_kbps = (upload_delta / time_delta) * 8.0f / 1000.0f;

        // Apply smoothing (exponential moving average) to reduce flickering
        // Only apply smoothing if we already have values
        if (g_dev_ui.download_kbps > 0.0f || g_dev_ui.upload_kbps > 0.0f) {
            g_dev_ui.download_kbps = g_dev_ui.download_kbps * 0.7f + new_download_kbps * 0.3f;
            g_dev_ui.upload_kbps = g_dev_ui.upload_kbps * 0.7f + new_upload_kbps * 0.3f;
        } else {
            // First time, just set the values
            g_dev_ui.download_kbps = new_download_kbps;
            g_dev_ui.upload_kbps = new_upload_kbps;
        }
    }

    // Update stored values
    g_dev_ui.last_download_bytes = download_bytes;
    g_dev_ui.last_upload_bytes = upload_bytes;
    g_dev_ui.last_network_update = current_time;
}

float dev_ui_get_download_kbps(void) {
    return g_dev_ui.download_kbps;
}

float dev_ui_get_upload_kbps(void) {
    return g_dev_ui.upload_kbps;
}

void dev_ui_set_display_options(bool show_network, bool show_player,
                                bool show_game, bool show_error) {
    g_dev_ui.show_network_stats = show_network;
    g_dev_ui.show_player_stats = show_player;
    g_dev_ui.show_game_stats = show_game;
    g_dev_ui.show_error_section = show_error;
}

void dev_ui_set_dimensions(int width, int height) {
    g_dev_ui.dev_ui_width = width;
    g_dev_ui.dev_ui_height = height;
}

void dev_ui_set_background_alpha(float alpha) {
    if (alpha < 0.0f) alpha = 0.0f;
    if (alpha > 1.0f) alpha = 1.0f;

    g_dev_ui.background_alpha = alpha;
    g_dev_ui.background_color.a = (unsigned char)(255 * alpha);
}

int dev_ui_get_active_stats_sum(const char* player_id) {
    if (!player_id) return 0;

    int total_stats = 0;

    // Check if it's the main player
    if (strcmp(g_game_state.player_id, player_id) == 0) {
        EntityState* entity = &g_game_state.player.base;

        // Sum up stats from active object layers
        for (int i = 0; i < entity->object_layer_count; i++) {
            if (entity->object_layers[i].active) {
                // Note: This is a placeholder - actual implementation would need
                // to look up the ObjectLayer data by item_id and sum the stats
                // For now, we just count active items * some baseline
                total_stats += 1; // Simplified - would need full item data access
            }
        }
    }

    return total_stats;
}

int dev_ui_get_active_item_count(const char* player_id) {
    if (!player_id) return 0;

    int active_count = 0;

    // Check if it's the main player
    if (strcmp(g_game_state.player_id, player_id) == 0) {
        EntityState* entity = &g_game_state.player.base;

        // Count active object layers
        for (int i = 0; i < entity->object_layer_count; i++) {
            if (entity->object_layers[i].active) {
                active_count++;
            }
        }
    }

    return active_count;
}

void dev_ui_draw(int screen_width, int screen_height, int hud_occupied) {
    // Only draw if dev_ui is enabled in game state
    bool dev_ui_enabled = g_game_state.dev_ui;

    if (!dev_ui_enabled) {
        return;
    }

    // Calculate dev UI height based on HUD occupation
    int dev_ui_height = g_dev_ui.dev_ui_height;
    if (hud_occupied > 0) {
        dev_ui_height = screen_height - hud_occupied;
        if (dev_ui_height < 80) {
            dev_ui_height = 80;
        }
    }

    // Draw background panel (top-left)
    Rectangle bg_rect = {0, 0, (float)g_dev_ui.dev_ui_width, (float)dev_ui_height};
    DrawRectangleRec(bg_rect, g_dev_ui.background_color);

    int y_offset = 10;
    int x_margin = 10;
    int font_size_title = 20;
    int font_size_text = 18;
    int line_spacing = 20;

    // Draw FPS at the top (replacing "DEV UI" label from Python version)
    char fps_text[32];
    snprintf(fps_text, sizeof(fps_text), "%d FPS", g_dev_ui.last_fps);
    DrawText(fps_text, x_margin, y_offset, font_size_title, g_dev_ui.debug_text_color);
    y_offset += font_size_title + 10;

    // Get player information
    const char* player_id = g_game_state.player_id[0] != '\0' ? g_game_state.player_id : "N/A";
    int map_id = g_game_state.player.map_id;
    const char* mode_str = mode_to_string(g_game_state.player.base.mode);
    const char* dir_str = direction_to_string(g_game_state.player.base.direction);
    Vector2 player_pos = g_game_state.player.base.interp_pos;
    Vector2 target_pos = g_game_state.player.target_pos;
    int sum_stats_limit = g_game_state.sum_stats_limit;
    const char* error_msg = g_game_state.last_error_message;

    // Get active stats and item count
    int active_stats_sum = dev_ui_get_active_stats_sum(player_id);
    int active_item_count = dev_ui_get_active_item_count(player_id);

    // Prepare text lines
    char text_lines[9][128];
    int line_count = 0;

    snprintf(text_lines[line_count++], 128, "Player ID: %s", player_id);
    snprintf(text_lines[line_count++], 128, "Map ID: %d", map_id);
    snprintf(text_lines[line_count++], 128, "Mode: %s | Direction: %s", mode_str, dir_str);
    snprintf(text_lines[line_count++], 128, "Pos: (%.2f, %.2f)", player_pos.x, player_pos.y);
    snprintf(text_lines[line_count++], 128, "Target: (%.0f, %.0f)", target_pos.x, target_pos.y);
    snprintf(text_lines[line_count++], 128, "Download: %.2f kbps | Upload: %.2f kbps",
             g_dev_ui.download_kbps, g_dev_ui.upload_kbps);
    snprintf(text_lines[line_count++], 128, "SumStatsLimit: %d", sum_stats_limit);
    snprintf(text_lines[line_count++], 128, "ActiveStatsSum: %d", active_stats_sum);
    snprintf(text_lines[line_count++], 128, "ActiveItems: %d", active_item_count);

    // Draw all text lines
    for (int i = 0; i < line_count; i++) {
        DrawText(text_lines[i], x_margin, y_offset, font_size_text, g_dev_ui.text_color);
        y_offset += line_spacing;
    }

    // Draw error message at the bottom if present
    if (g_dev_ui.show_error_section && error_msg[0] != '\0') {
        int error_y = dev_ui_height - 30;
        char error_text[256];
        snprintf(error_text, sizeof(error_text), "Error: %s", error_msg);
        DrawText(error_text, x_margin, error_y, font_size_text, g_dev_ui.error_text_color);
    }
}