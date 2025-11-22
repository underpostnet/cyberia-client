#include "modal.h"
#include "game_state.h"
#include "client.h"
#include <stdio.h>
#include <string.h>

// Global modal instance
Modal g_modal = {0};

int modal_init(void) {
    printf("[MODAL] Initializing modal component...\n");
    
    memset(&g_modal, 0, sizeof(Modal));
    
    // Set default dimensions
    g_modal.width = 200;
    g_modal.height = 100;
    g_modal.padding = 15;
    g_modal.margin_top = 10;
    g_modal.margin_right = 10;
    
    // Set default colors
    g_modal.background_color = (Color){0, 0, 0, 200};
    g_modal.text_color = (Color){255, 255, 255, 255};
    g_modal.label_color = (Color){255, 255, 100, 255}; // Yellow
    g_modal.shadow_color = (Color){0, 0, 0, 180};
    g_modal.background_alpha = 0.78f;
    
    // Font settings
    g_modal.font_size = 18;
    g_modal.line_spacing = 25;
    
    // Visibility
    g_modal.visible = true;
    
    // Animation
    g_modal.fade_alpha = 1.0f;
    g_modal.fade_in = true;
    
    printf("[MODAL] Modal component initialized\n");
    return 0;
}

void modal_cleanup(void) {
    printf("[MODAL] Cleaning up modal component...\n");
    memset(&g_modal, 0, sizeof(Modal));
}

void modal_update(float delta_time) {
    // Optional: Add fade in/out animations here if needed
    (void)delta_time; // Unused for now
}

void modal_draw(int screen_width, int screen_height) {
    // Only render when dev_ui is false
    game_state_lock();
    bool dev_ui_enabled = g_game_state.dev_ui;
    bool init_received = g_game_state.init_received;
    int map_id = g_game_state.player.map_id;
    Vector2 pos = g_game_state.player.base.interp_pos;
    game_state_unlock();
    
    // Don't render if dev_ui is enabled
    if (dev_ui_enabled) {
        return;
    }
    
    if (!g_modal.visible) {
        return;
    }
    
    // Get connection status
    bool is_connected = client_is_connected();
    const char* status_text = is_connected ? "Connected" : "Disconnected";
    Color status_color = is_connected ? GREEN : RED;
    
    // Prepare text lines
    char map_text[64];
    char pos_text[64];
    
    if (init_received) {
        snprintf(map_text, sizeof(map_text), "Map: %d", map_id);
        snprintf(pos_text, sizeof(pos_text), "Pos: (%.1f, %.1f)", pos.x, pos.y);
    } else {
        snprintf(map_text, sizeof(map_text), "Map: --");
        snprintf(pos_text, sizeof(pos_text), "Pos: (--, --)");
    }
    
    // Calculate text widths for centering and modal sizing
    int status_width = MeasureText(status_text, g_modal.font_size);
    int map_width = MeasureText(map_text, g_modal.font_size);
    int pos_width = MeasureText(pos_text, g_modal.font_size);
    
    // Find the maximum width
    int max_text_width = status_width;
    if (map_width > max_text_width) max_text_width = map_width;
    if (pos_width > max_text_width) max_text_width = pos_width;
    
    // Calculate modal dimensions based on content
    int modal_width = max_text_width + (g_modal.padding * 2);
    int modal_height = (g_modal.line_spacing * 3) + (g_modal.padding * 2);
    
    // Calculate modal position (top-right corner)
    int modal_x = screen_width - modal_width - g_modal.margin_right;
    int modal_y = g_modal.margin_top;
    
    // Draw modal background with rounded corners effect
    Rectangle modal_rect = {
        (float)modal_x,
        (float)modal_y,
        (float)modal_width,
        (float)modal_height
    };
    
    // Draw background
    Color bg_color = g_modal.background_color;
    bg_color.a = (unsigned char)(255 * g_modal.background_alpha);
    DrawRectangleRec(modal_rect, bg_color);
    
    // Draw border
    DrawRectangleLinesEx(modal_rect, 1.0f, (Color){100, 100, 100, 200});
    
    // Calculate text positions (centered horizontally)
    int text_start_y = modal_y + g_modal.padding;
    
    // Draw connection status
    int status_x = modal_x + (modal_width - status_width) / 2;
    DrawText(status_text, status_x + 1, text_start_y + 1, g_modal.font_size, g_modal.shadow_color);
    DrawText(status_text, status_x, text_start_y, g_modal.font_size, status_color);
    
    // Draw map ID
    int map_x = modal_x + (modal_width - map_width) / 2;
    DrawText(map_text, map_x + 1, text_start_y + g_modal.line_spacing + 1, g_modal.font_size, g_modal.shadow_color);
    DrawText(map_text, map_x, text_start_y + g_modal.line_spacing, g_modal.font_size, g_modal.label_color);
    
    // Draw position
    int pos_x = modal_x + (modal_width - pos_width) / 2;
    DrawText(pos_text, pos_x + 1, text_start_y + (g_modal.line_spacing * 2) + 1, g_modal.font_size, g_modal.shadow_color);
    DrawText(pos_text, pos_x, text_start_y + (g_modal.line_spacing * 2), g_modal.font_size, g_modal.label_color);
}

void modal_set_visible(bool visible) {
    g_modal.visible = visible;
}

bool modal_is_visible(void) {
    return g_modal.visible;
}

void modal_set_style(Color bg_color, Color text_color, float alpha) {
    g_modal.background_color = bg_color;
    g_modal.text_color = text_color;
    
    if (alpha < 0.0f) alpha = 0.0f;
    if (alpha > 1.0f) alpha = 1.0f;
    g_modal.background_alpha = alpha;
}

void modal_set_dimensions(int width, int height) {
    g_modal.width = width;
    g_modal.height = height;
}

void modal_set_position(int margin_top, int margin_right) {
    g_modal.margin_top = margin_top;
    g_modal.margin_right = margin_right;
}