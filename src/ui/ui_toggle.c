#include "ui_toggle.h"
#include "ui_button.h"
#include "ui_icon.h"
#include "text.h"

#include <math.h>
#include <stddef.h>

#define UI_TOGGLE_ANIM_SPEED 6.667f /* ~150 ms 0..1 */

/* Swipe control. The icon leaves its seat almost at once (DRAG_SLOP) and then
 * tracks the pointer, so the gesture always shows it took. Direction carries no
 * meaning: one press is one flip, tripped at COMMIT_PX of travel whichever way
 * it runs, or on release when the gesture stays shorter than that. */
#define UI_TOGGLE_DRAG_SLOP         4.0f
#define UI_TOGGLE_DRAG_COMMIT_PX   14.0f
#define UI_TOGGLE_DRAG_ICON_SCALE   1.18f
/* Follow travel saturates at this multiple of the button's edge, so the icon
 * stays near its seat however far the gesture runs. */
#define UI_TOGGLE_DRAG_FOLLOW_MAX   1.1f
/* Exponential rates (per second) for the release spring-back and the swell. */
#define UI_TOGGLE_DRAG_RETURN_RATE 16.0f
#define UI_TOGGLE_ICON_SCALE_RATE  20.0f

/* Header row geometry — uniform across every collapsible list except the
 * chevron glyph size, which callers may override (UI_TOGGLE_HDR_CHEVRON is
 * the standard, declared in ui_toggle.h). */
#define UI_TOGGLE_HDR_GAP      6.0f
#define UI_TOGGLE_HDR_VPAD     6.0f

void ui_toggle_init(UIToggle* t, Rectangle anchor, bool initial_expanded,
                    UIToggleChevron chevron) {
    t->expanded = initial_expanded;
    t->anchor   = anchor;
    t->anim_t   = initial_expanded ? 1.0f : 0.0f;
    t->chevron  = chevron;
    t->icon_expanded  = NULL;
    t->icon_collapsed = NULL;
    t->drag_enabled  = false;
    t->input_enabled = true;
    t->press_armed = false;
    t->dragging    = false;
    t->committed   = false;
    t->changed     = false;
    t->pointer_was_down = false;
    t->press_pos   = (Vector2){ 0.0f, 0.0f };
    t->drag_offset = (Vector2){ 0.0f, 0.0f };
    t->icon_scale  = 1.0f;
}

void ui_toggle_set_anchor(UIToggle* t, Rectangle anchor) {
    t->anchor = anchor;
}

void ui_toggle_set_drag(UIToggle* t) {
    t->drag_enabled = true;
}

void ui_toggle_set_input_enabled(UIToggle* t, bool enabled) {
    t->input_enabled = enabled;
}

bool ui_toggle_take_changed(UIToggle* t) {
    bool changed = t->changed;
    t->changed = false;
    return changed;
}

static void toggle_flip(UIToggle* t) {
    t->expanded = !t->expanded;
    t->changed  = true;
}

/* Touch drives the gesture when present; mouse otherwise. */
static Vector2 toggle_pointer_position(void) {
    if (GetTouchPointCount() > 0) return GetTouchPosition(0);
    return GetMousePosition();
}

static bool toggle_pointer_down(void) {
    return GetTouchPointCount() > 0 || IsMouseButtonDown(MOUSE_BUTTON_LEFT);
}

/* Follow offset for the icon: 1:1 with the pointer near the seat, easing into a
 * radial ceiling so a long drag never carries the glyph off its corner. */
static Vector2 follow_offset(const UIToggle* t, float dx, float dy) {
    float edge = t->anchor.width < t->anchor.height ? t->anchor.width : t->anchor.height;
    float max  = edge * UI_TOGGLE_DRAG_FOLLOW_MAX;
    float len  = sqrtf(dx * dx + dy * dy);
    if (max <= 0.0f || len <= 0.0f) return (Vector2){ 0.0f, 0.0f };
    float k = max * tanhf(len / max) / len;
    return (Vector2){ dx * k, dy * k };
}

static bool anchor_contains(const UIToggle* t, float x, float y) {
    return x >= t->anchor.x && x < t->anchor.x + t->anchor.width &&
           y >= t->anchor.y && y < t->anchor.y + t->anchor.height;
}

static void arm_gesture(UIToggle* t, Vector2 press) {
    t->press_armed = true;
    t->dragging    = false;
    t->committed   = false;
    t->press_pos   = press;
}

/* Resolve the armed gesture. A touch press reaches ui_toggle_handle_click up to
 * one confirmation window late (input.c holds it to disambiguate a pinch), so
 * the pointer may already have travelled — or lifted — by the first pass here;
 * both are read from the recorded press position, never assumed fresh. */
static void update_gesture(UIToggle* t, float dt) {
    Vector2 p    = toggle_pointer_position();
    bool    down = toggle_pointer_down();

    /* Touch arms on the press itself rather than waiting out that window,
     * which is what keeps the icon under the finger from the first frame.
     * Mouse presses arrive through the dispatcher with no such delay, so they
     * stay on it and keep its z-order gating. */
    if (t->input_enabled && !t->press_armed && down && !t->pointer_was_down &&
        GetTouchPointCount() > 0 && anchor_contains(t, p.x, p.y)) {
        arm_gesture(t, p);
    }
    t->pointer_was_down = down;

    if (t->press_armed) {
        float dx   = p.x - t->press_pos.x;
        float dy   = p.y - t->press_pos.y;
        float dist = sqrtf(dx * dx + dy * dy);

        if (!t->dragging && dist >= UI_TOGGLE_DRAG_SLOP) t->dragging = true;
        if (t->dragging) {
            /* One press, one flip: the drag trips it here, the release below
             * trips it otherwise, and `committed` keeps the two from both
             * firing on the same gesture. */
            if (!t->committed && dist >= UI_TOGGLE_DRAG_COMMIT_PX) {
                toggle_flip(t);
                t->committed = true;
            }
            t->drag_offset = follow_offset(t, dx, dy);
        }
        if (!down) {
            if (!t->committed) toggle_flip(t);
            t->press_armed = false;
            t->dragging    = false;
            t->committed   = false;
        }
    }

    if (!t->dragging && (0.0f != t->drag_offset.x || 0.0f != t->drag_offset.y)) {
        float decay = expf(-UI_TOGGLE_DRAG_RETURN_RATE * dt);
        t->drag_offset.x *= decay;
        t->drag_offset.y *= decay;
        if (fabsf(t->drag_offset.x) < 0.25f) t->drag_offset.x = 0.0f;
        if (fabsf(t->drag_offset.y) < 0.25f) t->drag_offset.y = 0.0f;
    }
    float scale_target = t->dragging ? UI_TOGGLE_DRAG_ICON_SCALE : 1.0f;
    t->icon_scale += (scale_target - t->icon_scale) *
                     (1.0f - expf(-UI_TOGGLE_ICON_SCALE_RATE * dt));
}

void ui_toggle_update(UIToggle* t, float dt) {
    float target = t->expanded ? 1.0f : 0.0f;
    float step   = UI_TOGGLE_ANIM_SPEED * dt;
    if (t->anim_t < target) {
        t->anim_t += step;
        if (t->anim_t > target) t->anim_t = target;
    } else if (t->anim_t > target) {
        t->anim_t -= step;
        if (t->anim_t < target) t->anim_t = target;
    }
    if (t->drag_enabled) update_gesture(t, dt);
}

/* Resolve the glyph orientation, flipping to the opposite side when
 * collapsed so the arrow always points toward the reveal direction. */
static UIToggleChevron resolve_chevron(const UIToggle* t) {
    if (t->expanded) return t->chevron;
    switch (t->chevron) {
        case UI_TOGGLE_CHEVRON_LEFT:  return UI_TOGGLE_CHEVRON_RIGHT;
        case UI_TOGGLE_CHEVRON_RIGHT: return UI_TOGGLE_CHEVRON_LEFT;
        case UI_TOGGLE_CHEVRON_UP:    return UI_TOGGLE_CHEVRON_DOWN;
        case UI_TOGGLE_CHEVRON_DOWN:  return UI_TOGGLE_CHEVRON_UP;
    }
    return t->chevron;
}

static const char* chevron_icon_id(UIToggleChevron ch) {
    switch (ch) {
        case UI_TOGGLE_CHEVRON_LEFT:  return "arrow-left";
        case UI_TOGGLE_CHEVRON_RIGHT: return "arrow-right";
        case UI_TOGGLE_CHEVRON_UP:    return "arrow-up";
        case UI_TOGGLE_CHEVRON_DOWN:  return "arrow-down";
    }
    return "arrow-down";
}

void ui_toggle_draw(const UIToggle* t) {
    /* Clean icon-only toggle: draw the icon directly with no background
     * fill, no black border, no bevel edges, no icon shadow. White rounded
     * rect outline on hover for the pixel-retro feel.
     *
     * When icon_expanded / icon_collapsed are set they override the chevron
     * glyph — used by the interaction column and inventory bar to show
     * close-yellow / stack / bag icons instead of arrows. */
    const char* icon = NULL;
    if (t->expanded && t->icon_expanded) {
        icon = t->icon_expanded;
    } else if (!t->expanded && t->icon_collapsed) {
        icon = t->icon_collapsed;
    } else {
        icon = chevron_icon_id(resolve_chevron(t));
    }
    float sz = t->anchor.width < t->anchor.height ? t->anchor.width : t->anchor.height;
    float cx = t->anchor.x + t->anchor.width * 0.5f;
    float cy = t->anchor.y + t->anchor.height * 0.5f;
    /* The hit area stays put — only the glyph rides the drag and swells. */
    cx += t->drag_offset.x;
    cy += t->drag_offset.y;
    ui_icon_draw(icon, cx, cy, (int)(sz * 0.75f * t->icon_scale + 0.5f), false, 0.0f);
    if (CheckCollisionPointRec(GetMousePosition(), t->anchor))
        DrawRectangleRoundedLinesEx(t->anchor, 0.18f, 6, 1.0f, WHITE);
}

bool ui_toggle_handle_click(UIToggle* t, int mx, int my) {
    if (!anchor_contains(t, (float)mx, (float)my)) return false;
    if (t->drag_enabled) {
        /* Arm only — the press may still become a swipe. Re-arming an active
         * gesture would move its origin, so a repeated press is ignored. */
        if (!t->press_armed) arm_gesture(t, (Vector2){ (float)mx, (float)my });
        return true;
    }
    toggle_flip(t);
    return true;
}

float ui_toggle_header(UIToggle* t, float x, float y, float width,
                       const char* label, int font, Color text_col,
                       UIToggleHeaderSide side, float reserve_left, float reserve_right,
                       float chevron_size, bool draw) {
    const float chev = chevron_size;

    float text_w = width - chev - UI_TOGGLE_HDR_GAP - reserve_left - reserve_right;
    if (text_w < 8.0f) text_w = 8.0f;

    float text_h = (float)text_wrap(label, 0, 0, (int)text_w, font, text_col, false, false);
    float content_h = text_h > chev ? text_h : chev;
    float row_h = content_h + 2.0f * UI_TOGGLE_HDR_VPAD;

    if (draw) {
        float chev_x = (UI_TOGGLE_HEADER_LEFT == side) ? x : (x + width - chev);
        ui_toggle_set_anchor(t, (Rectangle){ chev_x, y + (row_h - chev) / 2.0f, chev, chev });
        ui_toggle_draw(t);

        float text_x = (UI_TOGGLE_HEADER_LEFT == side)
                     ? (x + chev + UI_TOGGLE_HDR_GAP + reserve_left)
                     : (x + reserve_left);
        float text_y = y + (row_h - text_h) / 2.0f;
        text_wrap(label, (int)text_x, (int)text_y, (int)text_w, font, text_col, false, true);
    }
    return row_h;
}
