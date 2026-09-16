/**
 * ui_toggle — reusable collapse/expand tap target.
 *
 * A standalone widget with no external state dependencies. Each consumer
 * (interaction bubble column, Quest Journal panel, journal sections) owns
 * its own UIToggle instance and drives it through update/draw/click.
 *
 * `anim_t` lerps 0..1 toward the current `expanded` state so consumers can
 * drive slide/rotation animation off a single eased value.
 *
 * A consumer may opt into swipe control with ui_toggle_set_drag(): the press
 * then arms a gesture instead of flipping on touch-down, and either a drag in
 * any direction or a drag-less release (the plain tap) flips the state.
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
    const char*     icon_expanded;   /* overrides chevron when expanded (NULL = use chevron) */
    const char*     icon_collapsed;  /* overrides chevron when collapsed (NULL = use chevron) */
    bool            drag_enabled;    /* swipe control armed by ui_toggle_set_drag  */
    bool            input_enabled;   /* consumer gate for the touch press fallback */
    bool            press_armed;     /* press landed on the toggle, gesture pending */
    bool            dragging;        /* gesture passed the slop — icon follows      */
    bool            committed;       /* the drag already flipped this gesture       */
    bool            changed;         /* state flipped since the last take_changed   */
    bool            pointer_was_down;
    Vector2         press_pos;       /* gesture origin — icon follow and flip test  */
    Vector2         drag_offset;     /* px the glyph trails the pointer, springs to 0 */
    float           icon_scale;      /* 1.0, eased up while dragging                */
} UIToggle;

void ui_toggle_init(UIToggle* t, Rectangle anchor, bool initial_expanded,
                    UIToggleChevron chevron);
void ui_toggle_set_anchor(UIToggle* t, Rectangle anchor);
void ui_toggle_update(UIToggle* t, float dt);
void ui_toggle_draw(const UIToggle* t);

/* Opt into swipe control: dragging the button flips it, whichever way the drag
 * runs — one press is one flip, so the state always lands opposite whatever it
 * held. The icon tracks the pointer while dragging and springs back on release.
 * State resolves in ui_toggle_update — on the commit distance while dragging,
 * else on release. Toggles that skip this call keep flipping on press. */
void ui_toggle_set_drag(UIToggle* t);

/* Gate the drag-enabled toggle's own press detection, which exists because a
 * touch press reaches the dispatcher late. Consumers whose button can end up
 * under a modal clear it for as long as that modal is up, so the covered
 * button cannot be pressed through. Enabled after init. */
void ui_toggle_set_input_enabled(UIToggle* t, bool enabled);

/* One-shot: true when `expanded` flipped since the last query. Consumers that
 * react to the transition (resetting a scroll on expand, say) read it after
 * ui_toggle_update, because a swipe resolves there rather than on the press. */
bool ui_toggle_take_changed(UIToggle* t);

/* Returns true when the tap landed on the toggle. Flips the state, unless the
 * toggle is drag-enabled — then it only arms the gesture. */
bool ui_toggle_handle_click(UIToggle* t, int mx, int my);

/* Lay out a collapsible header row of `width` at (x, y): a chevron on the left
 * (vertically centred) and `label` word-wrapped in the remaining width. The row
 * height grows to fit the wrapped label (never less than the chevron), so text
 * never overflows when the font size / family changes. When `draw` is true the
 * chevron + label render and the toggle's chevron anchor updates; otherwise it
 * only measures. Returns the row height — the caller advances its cursor and
 * hit-tests the full [x, y, width, height] row to flip the toggle. */
float ui_toggle_header(UIToggle* t, float x, float y, float width,
                       const char* label, int font, Color text_col, bool draw);

#endif /* UI_TOGGLE_H */
