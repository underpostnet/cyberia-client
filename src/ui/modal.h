#ifndef MODAL_H
#define MODAL_H

#include <raylib.h>
#include <stdbool.h>

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
void modal_draw_panel_ex(Rectangle rect, float age, Color border, float border_width);

/* Soft drop shadow under a panel that floats over the live world with no
 * dimmed backdrop behind it — without it the card has no edge against a busy
 * scene. Draw before the panel fill. */
#define MODAL_SHADOW_LAYERS 3
void modal_draw_float_shadow(Rectangle rect, float age);

/* Draw a single line at (x, y), truncated with an ellipsis to fit `max_w`
 * pixels. Used for panel header titles, which share their strip with a close
 * button and must never run under it. */
void modal_draw_clipped_text(const char* text, int x, int y, int max_w,
                             int font_size, Color color);

/* Landscape breakpoint: the UI area is significantly wider than tall, so
 * modals prefer two-column layouts over vertical stacking. */
bool modal_wide_layout(void);


#endif // MODAL_H
