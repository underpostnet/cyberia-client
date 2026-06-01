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
 * @brief Render the modal
 * @param modal Pointer to modal
 * @param screen_width Current screen width
 * @param screen_height Current screen height
 */
void modal_draw_struct(const Modal* modal, int screen_width, int screen_height);


#endif // MODAL_H
