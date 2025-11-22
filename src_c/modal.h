#ifndef MODAL_H
#define MODAL_H

#include <raylib.h>
#include <stdbool.h>

/**
 * @file modal.h
 * @brief General-purpose modal container component for displaying text
 * 
 * This module provides a flexible modal/overlay container that can display
 * multiple lines of text with customizable styling and positioning.
 * It serves as a reusable UI component for various game status displays.
 */

#define MODAL_MAX_LINES 10
#define MODAL_MAX_LINE_LENGTH 128

/**
 * @brief Text line structure for modal content
 */
typedef struct {
    char text[MODAL_MAX_LINE_LENGTH];
    Color color;
    bool visible;
} ModalLine;

/**
 * @brief Modal container state structure
 */
typedef struct {
    // Content
    ModalLine lines[MODAL_MAX_LINES];
    int line_count;
    
    // Layout configuration
    int min_width;
    int min_height;
    int padding;
    int margin_top;
    int margin_right;
    int margin_bottom;
    int margin_left;
    
    // Positioning mode
    enum {
        MODAL_POS_TOP_LEFT,
        MODAL_POS_TOP_RIGHT,
        MODAL_POS_BOTTOM_LEFT,
        MODAL_POS_BOTTOM_RIGHT,
        MODAL_POS_CENTER,
        MODAL_POS_CUSTOM
    } position_mode;
    
    // Custom position (used when position_mode is MODAL_POS_CUSTOM)
    int custom_x;
    int custom_y;
    
    // Styling
    Color background_color;
    Color border_color;
    Color shadow_color;
    float background_alpha;
    float border_width;
    bool draw_shadow;
    bool draw_border;
    
    // Font settings
    int font_size;
    int line_spacing;
    
    // Text alignment
    enum {
        MODAL_ALIGN_LEFT,
        MODAL_ALIGN_CENTER,
        MODAL_ALIGN_RIGHT
    } text_align;
    
    // Visibility
    bool visible;
    
    // Animation
    float fade_alpha;
    bool fade_in;
    
} Modal;

/**
 * @brief Initialize a modal structure with default values
 * @param modal Pointer to modal to initialize
 * @return 0 on success, -1 on failure
 */
int modal_init_struct(Modal* modal);

/**
 * @brief Clear all lines in a modal
 * @param modal Pointer to modal
 */
void modal_clear_lines(Modal* modal);

/**
 * @brief Add a text line to the modal
 * @param modal Pointer to modal
 * @param text Text content
 * @param color Text color
 * @return 0 on success, -1 if modal is full
 */
int modal_add_line(Modal* modal, const char* text, Color color);

/**
 * @brief Set a specific line in the modal
 * @param modal Pointer to modal
 * @param line_index Line index (0-based)
 * @param text Text content
 * @param color Text color
 * @return 0 on success, -1 on failure
 */
int modal_set_line(Modal* modal, int line_index, const char* text, Color color);

/**
 * @brief Update modal state (animations, etc.)
 * @param modal Pointer to modal
 * @param delta_time Time elapsed since last update
 */
void modal_update_struct(Modal* modal, float delta_time);

/**
 * @brief Render the modal
 * @param modal Pointer to modal
 * @param screen_width Current screen width
 * @param screen_height Current screen height
 */
void modal_draw_struct(const Modal* modal, int screen_width, int screen_height);

/**
 * @brief Set modal visibility
 * @param modal Pointer to modal
 * @param visible True to show, false to hide
 */
void modal_set_visible(Modal* modal, bool visible);

/**
 * @brief Set modal styling
 * @param modal Pointer to modal
 * @param bg_color Background color
 * @param border_color Border color
 * @param alpha Background transparency (0.0 to 1.0)
 */
void modal_set_style(Modal* modal, Color bg_color, Color border_color, float alpha);

/**
 * @brief Set modal dimensions
 * @param modal Pointer to modal
 * @param min_width Minimum modal width
 * @param min_height Minimum modal height
 */
void modal_set_dimensions(Modal* modal, int min_width, int min_height);

/**
 * @brief Set modal position mode and margins
 * @param modal Pointer to modal
 * @param position_mode Position mode (e.g., MODAL_POS_TOP_RIGHT)
 * @param margin_top Top margin in pixels
 * @param margin_right Right margin in pixels
 * @param margin_bottom Bottom margin in pixels
 * @param margin_left Left margin in pixels
 */
void modal_set_position(Modal* modal, int position_mode, 
                       int margin_top, int margin_right, 
                       int margin_bottom, int margin_left);

/**
 * @brief Set custom modal position
 * @param modal Pointer to modal
 * @param x X position in pixels
 * @param y Y position in pixels
 */
void modal_set_custom_position(Modal* modal, int x, int y);

/**
 * @brief Set text alignment within the modal
 * @param modal Pointer to modal
 * @param align Alignment mode (MODAL_ALIGN_LEFT, MODAL_ALIGN_CENTER, MODAL_ALIGN_RIGHT)
 */
void modal_set_text_alignment(Modal* modal, int align);

/**
 * @brief Set font settings
 * @param modal Pointer to modal
 * @param font_size Font size in pixels
 * @param line_spacing Spacing between lines in pixels
 */
void modal_set_font(Modal* modal, int font_size, int line_spacing);

#endif // MODAL_H