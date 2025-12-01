#ifndef DEV_UI_H
#define DEV_UI_H

#include <raylib.h>
#include <stdbool.h>
#include <stddef.h>

/**
 * @file dev_ui.h
 * @brief Development UI component for monitoring game metrics
 * 
 * This module provides a standalone development UI that displays various
 * runtime metrics including FPS, network statistics, player information,
 * and game state details. It is toggled via the dev_ui flag in game state.
 * Migrated from Python dev_ui.py.
 */

/**
 * @brief Development UI state structure
 */
typedef struct {
    // Network statistics
    float download_kbps;
    float upload_kbps;
    double last_network_update;
    size_t last_download_bytes;
    size_t last_upload_bytes;
    
    // UI layout
    int dev_ui_width;
    int dev_ui_height;
    float background_alpha;
    
    // Colors
    Color background_color;
    Color text_color;
    Color debug_text_color;
    Color error_text_color;
    
    // Performance tracking
    int last_fps;
    double last_fps_update;
    
    // Display toggles
    bool show_network_stats;
    bool show_player_stats;
    bool show_game_stats;
    bool show_error_section;
    
} DevUI;

// Global dev UI instance
extern DevUI g_dev_ui;

/**
 * @brief Initialize the development UI component
 * @return 0 on success, -1 on failure
 */
int dev_ui_init(void);

/**
 * @brief Cleanup development UI resources
 */
void dev_ui_cleanup(void);

/**
 * @brief Update development UI state (network stats, etc.)
 * @param delta_time Time elapsed since last update
 * 
 * This should be called each frame to update dynamic statistics
 * like network throughput calculations.
 */
void dev_ui_update(float delta_time);

/**
 * @brief Render the development UI
 * @param screen_width Current screen width
 * @param screen_height Current screen height
 * @param hud_occupied Height occupied by the HUD (if any)
 * 
 * Renders the complete development UI including:
 * - FPS counter
 * - Player ID, map ID, position
 * - Player mode and direction
 * - Target position
 * - Network statistics (download/upload kbps)
 * - Stats limit and active stats sum
 * - Active items count
 * - Error messages (if any)
 */
void dev_ui_draw(int screen_width, int screen_height, int hud_occupied);

/**
 * @brief Update network statistics
 * @param download_bytes Total bytes downloaded
 * @param upload_bytes Total bytes uploaded
 * 
 * Call this when network statistics are updated to recalculate
 * the kbps values for display.
 */
void dev_ui_update_network_stats(size_t download_bytes, size_t upload_bytes);

/**
 * @brief Get current download speed in kbps
 * @return Download speed in kilobits per second
 */
float dev_ui_get_download_kbps(void);

/**
 * @brief Get current upload speed in kbps
 * @return Upload speed in kilobits per second
 */
float dev_ui_get_upload_kbps(void);

/**
 * @brief Set dev UI display options
 * @param show_network Show network statistics section
 * @param show_player Show player information section
 * @param show_game Show game state section
 * @param show_error Show error message section
 */
void dev_ui_set_display_options(bool show_network, bool show_player, 
                                bool show_game, bool show_error);

/**
 * @brief Calculate active stats sum for a player
 * @param player_id Player ID to calculate for
 * @return Sum of active stats, or 0 if player not found
 * 
 * Helper function to calculate the sum of all active item stats
 * for the specified player.
 */
int dev_ui_get_active_stats_sum(const char* player_id);

/**
 * @brief Count active items for a player
 * @param player_id Player ID to count for
 * @return Number of active items
 */
int dev_ui_get_active_item_count(const char* player_id);

/**
 * @brief Set dev UI dimensions
 * @param width Width of the dev UI panel
 * @param height Height of the dev UI panel
 */
void dev_ui_set_dimensions(int width, int height);

/**
 * @brief Set dev UI background transparency
 * @param alpha Alpha value (0.0 to 1.0)
 */
void dev_ui_set_background_alpha(float alpha);

#endif // DEV_UI_H