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

/* Shared UI palette. Only exact copies live here — two colours that merely
 * look alike stay with their own module. */
extern const Color MODAL_TEXT;         /* body text on a panel                */
extern const Color MODAL_LABEL;        /* dimmer label beside a value         */
extern const Color MODAL_POSITIVE;     /* a gain, a match, an affordable price */
extern const Color MODAL_ACCEPT;       /* the green of an accept button       */
extern const Color MODAL_QUEST_FRAME;  /* gold frame marking a quest context  */
extern const Color MODAL_QUEST;        /* quest capability marker             */
extern const Color MODAL_ACTION;       /* action capability marker            */

/* Eased pop-in scale (0.80 → 1.0) over MODAL_POP_DURATION; 1.0 afterwards. */
float modal_pop_scale(float age);

/* Eased fade factor (0 → 1) over MODAL_POP_DURATION; multiply any colour
 * alpha by this for a uniform fade-in. */
float modal_pop_alpha(float age);

/* Soft drop shadow under a panel that floats over the live world with no
 * dimmed backdrop behind it — without it the card has no edge against a busy
 * scene. Draw before the panel fill. */
#define MODAL_SHADOW_LAYERS 3
void modal_draw_float_shadow(Rectangle rect, float age);

/* Panel alpha for an anchored card: it floats over the live world with no
 * dimmed backdrop, so it fills more opaquely than a centred card. */
#define MODAL_ANCHOR_PANEL_ALPHA 236.0f

/* Clearance the header title keeps from the close button. */
#define MODAL_HEADER_TITLE_GAP 8.0f

/* Standard card chrome, in draw order: the float shadow when `anchored`, else
 * the dim overlay down to `overlay_h`; the panel fill; a 1 px `border`; and
 * the header strip of `header_h`. Every alpha is eased by `age`. */
void modal_draw_card(Rectangle card, bool anchored, float age, float overlay_h,
                     Color border, float header_h);

/* Header title, `pad` from the card's left edge but never left of the
 * toolbar's pinned toggle, clipped before `close_x`. */
void modal_draw_title(const char* text, Rectangle card, float pad, float header_h,
                      int font, Color color, float close_x);

/* Draw a single line at (x, y), truncated with an ellipsis to fit `max_w`
 * pixels. Used for panel header titles, which share their strip with a close
 * button and must never run under it. */
void modal_draw_clipped_text(const char* text, int x, int y, int max_w,
                             int font_size, Color color);

/* Landscape breakpoint: the UI area is significantly wider than tall, so
 * modals prefer two-column layouts over vertical stacking. */
bool modal_wide_layout(void);


#endif // MODAL_H
