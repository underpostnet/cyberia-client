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

#endif /* UI_TOGGLE_H */
