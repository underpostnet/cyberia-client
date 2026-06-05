#include "ui_toggle.h"
#include "ui_icon.h"

#include <math.h>

#define UI_TOGGLE_ANIM_SPEED 6.667f /* ~150 ms 0..1 */

static const Color C_TOGGLE_BG     = {  20,  24,  40, 220 };
static const Color C_TOGGLE_BORDER = {  80, 160, 220, 240 };

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
    DrawRectangleRec(t->anchor, C_TOGGLE_BG);
    DrawRectangleLinesEx(t->anchor, 1.0f, C_TOGGLE_BORDER);

    float cx = t->anchor.x + t->anchor.width  * 0.5f;
    float cy = t->anchor.y + t->anchor.height * 0.5f;
    int   sz = (int)((t->anchor.width < t->anchor.height
                      ? t->anchor.width : t->anchor.height) * 0.75f);
    if (sz < 6) sz = 6;
    ui_icon_draw(chevron_icon_id(resolve_chevron(t)), cx, cy, sz, false, 0.0f);
}

bool ui_toggle_handle_click(UIToggle* t, int mx, int my) {
    if ((float)mx < t->anchor.x || (float)mx >= t->anchor.x + t->anchor.width ||
        (float)my < t->anchor.y || (float)my >= t->anchor.y + t->anchor.height) {
        return false;
    }
    t->expanded = !t->expanded;
    return true;
}
