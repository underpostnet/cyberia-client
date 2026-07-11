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


/* ── Shared panel chrome ──────────────────────────────────────────────────
 *
 * Standardized look-and-feel for every panel-style modal (inventory,
 * interaction, dialogue, quest journal). One dark background, one border,
 * one entrance animation — so modals never appear abruptly or diverge.
 * Each modal tracks its own age (seconds since open) and passes it here.
 */

#define MODAL_POP_DURATION 0.15f

extern const Color MODAL_OVERLAY_BG;   /* dim backdrop behind centred modals */
extern const Color MODAL_PANEL_BG;     /* standardized dark panel fill        */
extern const Color MODAL_PANEL_BORDER; /* standardized panel border           */

/* Eased pop-in scale (0.80 → 1.0) over MODAL_POP_DURATION; 1.0 afterwards. */
float modal_pop_scale(float age);

/* Eased fade factor (0 → 1) over MODAL_POP_DURATION; multiply any colour
 * alpha by this for a uniform fade-in. */
float modal_pop_alpha(float age);

/* Scale a rect about its centre — pairs with modal_pop_scale. */
Rectangle modal_scale_rect(Rectangle rect, float scale);

/* Dim the whole screen behind a centred modal (alpha eased by age). */
void modal_draw_overlay(int screen_width, int screen_height, float age);

/* Fill + border a panel rect with the standardized chrome (faded by age). */
void modal_draw_panel(Rectangle rect, float age);
void modal_draw_panel_ex(Rectangle rect, float age, Color border, float border_width);

/* Landscape breakpoint: the UI area is significantly wider than tall, so
 * modals prefer two-column layouts over vertical stacking. */
bool modal_wide_layout(void);


#endif // MODAL_H
