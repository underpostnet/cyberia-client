#ifndef CYBERIA_UI_SCROLL_H
#define CYBERIA_UI_SCROLL_H

#include <raylib.h>
#include <stdbool.h>

/* ui_scroll — reusable invisible scroll container for UI panels.
 *
 * Interaction model (cross-platform):
 *   - mouse wheel scrolls while the pointer hovers the view (desktop);
 *   - press-drag scrolls 1:1 once the gesture exceeds a small slop
 *     (touch and mouse), then glides with inertial velocity after release;
 *   - a thin scrollbar appears only while scrolling and fades out when idle;
 *   - content is clipped with raylib scissor mode;
 *   - all motion is frame-rate independent (dt-driven exponential decay).
 *
 * Tap-vs-drag contract: the input pipeline dispatches INPUT_TAP on pointer
 * PRESS, so a host panel forwards presses inside the view to
 * ui_scroll_on_press() and activates content only from ui_scroll_take_click(),
 * which fires on release when the gesture never became a drag — activation on
 * touch-up, never on touch-down, so a scroll drag can never press content.
 *
 * Per-frame host contract:
 *   ui_scroll_update(&s, view, content_h, dt);
 *   int cx, cy;
 *   if (ui_scroll_take_click(&s, &cx, &cy)) { ...hit-test content at cx,cy... }
 *   ui_scroll_begin(&s);
 *   ...draw content shifted up by ui_scroll_offset(&s)...
 *   ui_scroll_end(&s);
 * and in the panel's tap handler, for presses inside the view:
 *   ui_scroll_on_press(&s, mx, my);
 * Route wheel deltas from the UI input dispatcher through ui_scroll_on_wheel().
 */

typedef struct {
    Rectangle view;           /* viewport captured by the last update          */
    Rectangle input_bounds;   /* optional pointer hit area                     */
    Rectangle scrollbar_bounds; /* optional transient thumb track               */
    float     content_h;      /* content height captured by the last update    */
    float     offset;         /* scroll offset px, 0..(content_h - view.height)*/
    float     vel;            /* px/s — inertial glide velocity                */
    bool      pressed;        /* pointer down began inside the view            */
    bool      pointer_was_down;
    bool      dragging;       /* gesture exceeded the slop — scrolling         */
    bool      has_input_bounds;
    bool      has_scrollbar_bounds;
    Vector2   press_pos;
    float     last_pointer_y;
    bool      click_pending;  /* clean release — deferred activation           */
    Vector2   click_pos;
    bool      scrollbar_enabled;
    float     bar_hold_s;     /* seconds of scrollbar visibility remaining     */
    float     bar_alpha;
} UIScroll;

/* Zero the container (fresh panel session: offset, glide, gesture, bar). */
void ui_scroll_reset(UIScroll* s);

/* Enable or disable the transient scrollbar. It is enabled after reset. */
void ui_scroll_set_scrollbar(UIScroll* s, bool enabled);

/* Override the pointer hit area or transient thumb track. Bounds with no
 * positive area restore the clipping viewport as the default. */
void ui_scroll_set_input_bounds(UIScroll* s, Rectangle bounds);
void ui_scroll_set_scrollbar_bounds(UIScroll* s, Rectangle bounds);

/* Route a pointer press that landed inside the view. Catches a gliding list
 * (kills velocity) and arms tap-vs-drag disambiguation. */
void ui_scroll_on_press(UIScroll* s, int mx, int my);

/* Forward a wheel delta from the UI input dispatcher. Returns true when the
 * pointer is inside `view`, so callers can suppress world zoom behind UI.
 * `view` is the on-screen viewport and `content_h` the full unscrolled height. */
bool ui_scroll_on_wheel(UIScroll* s, Rectangle view, float content_h, float wheel_delta);

/* Advance the container one frame: drag tracking, inertial glide, clamping,
 * and scrollbar fade. `view` is the on-screen viewport, `content_h`
 * the full unscrolled content height (last frame's measure is fine). */
void ui_scroll_update(UIScroll* s, Rectangle view, float content_h, float dt);

/* One-shot: the deferred click from a clean (drag-less) release. Fills the
 * press position and clears the pending state. */
bool ui_scroll_take_click(UIScroll* s, int* out_x, int* out_y);

/* Current scroll offset in pixels — subtract from content y while drawing. */
float ui_scroll_offset(const UIScroll* s);

/* Scissor-clip drawing to the view. Pair every begin with an end. */
void ui_scroll_begin(const UIScroll* s);

/* End the scissor clip and overlay the fading scrollbar. */
void ui_scroll_end(const UIScroll* s);

#endif /* CYBERIA_UI_SCROLL_H */
