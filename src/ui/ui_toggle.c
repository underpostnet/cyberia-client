#include "ui_toggle.h"
#include "ui_button.h"
#include "text.h"

#define UI_TOGGLE_ANIM_SPEED 6.667f /* ~150 ms 0..1 */

/* Header row geometry — uniform across every collapsible list. */
#define UI_TOGGLE_HDR_CHEVRON 22.0f
#define UI_TOGGLE_HDR_GAP      6.0f
#define UI_TOGGLE_HDR_VPAD     6.0f

void ui_toggle_init(UIToggle* t, Rectangle anchor, bool initial_expanded,
                    UIToggleChevron chevron) {
    t->expanded = initial_expanded;
    t->anchor   = anchor;
    t->anim_t   = initial_expanded ? 1.0f : 0.0f;
    t->chevron  = chevron;
}

void ui_toggle_set_anchor(UIToggle* t, Rectangle anchor) {
    t->anchor = anchor;
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
    UIButtonStyle style = { .icon_id = chevron_icon_id(resolve_chevron(t)) };
    ui_button_draw(t->anchor, &style, UI_BUTTON_NORMAL);
}

bool ui_toggle_handle_click(UIToggle* t, int mx, int my) {
    if ((float)mx < t->anchor.x || (float)mx >= t->anchor.x + t->anchor.width ||
        (float)my < t->anchor.y || (float)my >= t->anchor.y + t->anchor.height) {
        return false;
    }
    t->expanded = !t->expanded;
    return true;
}

float ui_toggle_header(UIToggle* t, float x, float y, float width,
                       const char* label, int font, Color text_col,
                       UIToggleHeaderSide side, float reserve_left, float reserve_right, bool draw) {
    const float chev = UI_TOGGLE_HDR_CHEVRON;

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
