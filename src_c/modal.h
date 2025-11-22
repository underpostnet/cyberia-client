#ifndef MODAL_H
#define MODAL_H

#include <raylib.h>
#include <stdbool.h>

/**
 * @file modal.h
 * @brief Modal container component for status labels
 * 
 * This module provides a modal/overlay container that displays status information
 * including connection status, map ID, and player position. It is only rendered
 * when the dev_ui is disabled (false), providing a clean minimal UI for players
 * who don't need the full development interface.
 */

/**
 * @brief Modal state structure
 */
typedef struct {
    // Layout configuration
    int width;
    int height;
    int padding;
    int margin_top;
    int margin_right;
    
    // Styling
    Color background_color;
    Color text_color;
    Color label_color;
    Color shadow_color;
    float background_alpha;
    
    // Font settings
    int font_size;
    int line_spacing;
    
    // Visibility
    bool visible;
    
    // Animation
    float fade_alpha;
    bool fade_in;
    
} Modal;

// Global modal instance
extern Modal g_modal;

/**
 * @brief Initialize the modal component
 * @return 0 on success, -1 on failure
 */
int modal_init(void);

/**
 * @brief Cleanup modal resources
 */
void modal_cleanup(void);

/**
 * @brief Update modal state (animations, etc.)
 * @param delta_time Time elapsed since last update
 */
void modal_update(float delta_time);

/**
 * @brief Render the modal with status labels
 * @param screen_width Current screen width
 * @param screen_height Current screen height
 * 
 * Renders a modal in the upper-right corner containing:
 * - WebSocket connection status
 * - Current map ID
 * - Player position (x, y)
 * 
 * Only renders when dev_ui is false.
 */
void modal_draw(int screen_width, int screen_height);

/**
 * @brief Set modal visibility
 * @param visible True to show, false to hide
 */
void modal_set_visible(bool visible);

/**
 * @brief Check if modal is currently visible
 * @return True if visible, false otherwise
 */
bool modal_is_visible(void);

/**
 * @brief Set modal styling
 * @param bg_color Background color
 * @param text_color Text color
 * @param alpha Background transparency (0.0 to 1.0)
 */
void modal_set_style(Color bg_color, Color text_color, float alpha);

/**
 * @brief Set modal dimensions
 * @param width Modal width
 * @param height Modal height
 */
void modal_set_dimensions(int width, int height);

/**
 * @brief Set modal position offset from top-right corner
 * @param margin_top Top margin in pixels
 * @param margin_right Right margin in pixels
 */
void modal_set_position(int margin_top, int margin_right);

#endif // MODAL_H