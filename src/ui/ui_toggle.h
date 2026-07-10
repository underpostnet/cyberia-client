/**
 * ui_toggle — reusable collapse/expand tap target.
 *
 * A standalone widget with no external state dependencies. Each consumer
 * (interaction bubble column, Quest Journal panel, journal sections) owns
 * its own UIToggle instance and drives it through update/draw/click.
 *
 * `anim_t` lerps 0..1 toward the current `expanded` state so consumers can
 * drive slide/rotation animation off a single eased value.
 */

#ifndef UI_TOGGLE_H
#define UI_TOGGLE_H

#include <raylib.h>
#include <stdbool.h>

/* Direction the chevron points to when expanded; the collapsed chevron
 * points the opposite way. Picks the glyph orientation per consumer. */
typedef enum {
    UI_TOGGLE_CHEVRON_LEFT,
    UI_TOGGLE_CHEVRON_RIGHT,
    UI_TOGGLE_CHEVRON_UP,
    UI_TOGGLE_CHEVRON_DOWN,
} UIToggleChevron;

typedef struct {
    bool            expanded;
    Rectangle       anchor;   /* screen-space hit area */
    float           anim_t;   /* 0..1 eased toward expanded */
    UIToggleChevron chevron;  /* glyph orientation when expanded */
} UIToggle;

void ui_toggle_init(UIToggle* t, Rectangle anchor, bool initial_expanded,
                    UIToggleChevron chevron);
void ui_toggle_set_anchor(UIToggle* t, Rectangle anchor);
void ui_toggle_update(UIToggle* t, float dt);
void ui_toggle_draw(const UIToggle* t);

/* Returns true when the tap landed on the toggle and flipped its state. */
bool ui_toggle_handle_click(UIToggle* t, int mx, int my);

/* Side the chevron sits on within a header row built by ui_toggle_header. */
typedef enum { UI_TOGGLE_HEADER_LEFT, UI_TOGGLE_HEADER_RIGHT } UIToggleHeaderSide;

/* Default chevron glyph size (px) for ui_toggle_header — used by every
 * collapsible list in the client except one that opts into a larger glyph
 * (via the `chevron_size` param) for a more prominent header. */
#define UI_TOGGLE_HDR_CHEVRON 22.0f

/* Lay out a collapsible header row of `width` at (x, y): a chevron on `side`
 * (vertically centred) and `label` word-wrapped in the remaining width. The row
 * height grows to fit the wrapped label (never less than the chevron), so text
 * never overflows when the font size / family changes. `reserve_left` /
 * `reserve_right` carve out space next to the label for caller decorations (a
 * status dot, a trailing badge). `chevron_size` sets the chevron glyph's edge
 * length (pass UI_TOGGLE_HDR_CHEVRON for the standard size). When `draw` is
 * true the chevron + label render and the toggle's chevron anchor updates;
 * otherwise it only measures. Returns the row height — the caller advances its
 * cursor and hit-tests the full [x, y, width, height] row to flip the toggle.
 * Centralized header geometry shared by every collapsible list in the client. */
float ui_toggle_header(UIToggle* t, float x, float y, float width,
                       const char* label, int font, Color text_col,
                       UIToggleHeaderSide side, float reserve_left, float reserve_right,
                       float chevron_size, bool draw);

#endif /* UI_TOGGLE_H */
