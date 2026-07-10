#include "ui_scroll.h"

#include <math.h>

#define UI_SCROLL_DRAG_SLOP       8.0f
#define UI_SCROLL_WHEEL_PIXELS   56.0f
#define UI_SCROLL_INERTIA_DECAY   8.5f
#define UI_SCROLL_MIN_VELOCITY    8.0f
#define UI_SCROLL_BAR_HOLD_S      0.45f
#define UI_SCROLL_BAR_FADE_S      0.25f
#define UI_SCROLL_BAR_MIN_H      18.0f
#define UI_SCROLL_BAR_W           3.0f
#define UI_SCROLL_BAR_MARGIN      3.0f

static float scroll_max_offset(const UIScroll* s) {
    float max_offset = s->content_h - s->view.height;
    return max_offset > 0.0f ? max_offset : 0.0f;
}

static float scroll_clamp(float value, float min_value, float max_value) {
    if (value < min_value) return min_value;
    if (value > max_value) return max_value;
    return value;
}

static bool scroll_contains(const Rectangle view, Vector2 point) {
    return point.x >= view.x && point.x < view.x + view.width &&
           point.y >= view.y && point.y < view.y + view.height;
}

static bool scroll_input_contains(const UIScroll* s, Vector2 point) {
    Rectangle bounds = s->has_input_bounds ? s->input_bounds : s->view;
    return scroll_contains(bounds, point);
}

static Vector2 scroll_pointer_position(void) {
    if (GetTouchPointCount() > 0) return GetTouchPosition(0);
    return GetMousePosition();
}

static bool scroll_pointer_down(void) {
    return GetTouchPointCount() > 0 || IsMouseButtonDown(MOUSE_BUTTON_LEFT);
}

static void scroll_capture_bounds(UIScroll* s, Rectangle view, float content_h) {
    s->view = view;
    s->content_h = content_h > 0.0f ? content_h : 0.0f;
    s->offset = scroll_clamp(s->offset, 0.0f, scroll_max_offset(s));
}

static float scroll_move(UIScroll* s, float delta) {
    float before = s->offset;
    s->offset = scroll_clamp(before + delta, 0.0f, scroll_max_offset(s));
    return s->offset - before;
}

static void scroll_show_bar(UIScroll* s) {
    if (!s->scrollbar_enabled) return;
    s->bar_hold_s = UI_SCROLL_BAR_HOLD_S;
    s->bar_alpha = 1.0f;
}

void ui_scroll_reset(UIScroll* s) {
    if (!s) return;
    *s = (UIScroll){ 0 };
    s->scrollbar_enabled = true;
}

void ui_scroll_set_scrollbar(UIScroll* s, bool enabled) {
    if (!s) return;
    s->scrollbar_enabled = enabled;
    if (!enabled) {
        s->bar_hold_s = 0.0f;
        s->bar_alpha = 0.0f;
    }
}

void ui_scroll_set_input_bounds(UIScroll* s, Rectangle bounds) {
    if (!s) return;
    s->input_bounds = bounds;
    s->has_input_bounds = bounds.width > 0.0f && bounds.height > 0.0f;
}

void ui_scroll_set_scrollbar_bounds(UIScroll* s, Rectangle bounds) {
    if (!s) return;
    s->scrollbar_bounds = bounds;
    s->has_scrollbar_bounds = bounds.width > 0.0f && bounds.height > 0.0f;
}

void ui_scroll_on_press(UIScroll* s, int mx, int my) {
    if (!s) return;
    s->pressed = true;
    s->dragging = false;
    s->press_pos = (Vector2){ (float)mx, (float)my };
    s->last_pointer_y = (float)my;
    s->vel = 0.0f;
    s->click_pending = false;
}

bool ui_scroll_on_wheel(UIScroll* s, Rectangle view, float content_h, float wheel_delta) {
    if (!s || wheel_delta == 0.0f) return false;

    scroll_capture_bounds(s, view, content_h);
    if (!scroll_input_contains(s, scroll_pointer_position())) return false;

    s->vel = 0.0f;
    if (fabsf(scroll_move(s, -wheel_delta * UI_SCROLL_WHEEL_PIXELS)) > 0.0f)
        scroll_show_bar(s);
    return true;
}

void ui_scroll_update(UIScroll* s, Rectangle view, float content_h, float dt) {
    if (!s) return;

    scroll_capture_bounds(s, view, content_h);
    Vector2 pointer = scroll_pointer_position();
    bool pointer_down = scroll_pointer_down();

    /* Touch can be independent of mouse press events on some platforms. */
    if (!s->pressed && pointer_down && !s->pointer_was_down &&
        scroll_input_contains(s, pointer)) {
        ui_scroll_on_press(s, (int)pointer.x, (int)pointer.y);
    }

    if (s->pressed) {
        if (pointer_down) {
            float pointer_delta = pointer.y - s->last_pointer_y;
            float total_delta = pointer.y - s->press_pos.y;
            if (!s->dragging && fabsf(total_delta) >= UI_SCROLL_DRAG_SLOP) {
                s->dragging = true;
                s->click_pending = false;
            }
            if (s->dragging && fabsf(pointer_delta) > 0.0f) {
                float moved = scroll_move(s, -pointer_delta);
                if (dt > 0.0f) s->vel = moved / dt;
                if (fabsf(moved) > 0.0f) scroll_show_bar(s);
            }
            s->last_pointer_y = pointer.y;
        } else {
            if (!s->dragging) {
                s->click_pending = true;
                s->click_pos = s->press_pos;
            }
            s->pressed = false;
        }
    }

    if (!s->pressed && fabsf(s->vel) >= UI_SCROLL_MIN_VELOCITY && dt > 0.0f) {
        float next_velocity = s->vel * expf(-UI_SCROLL_INERTIA_DECAY * dt);
        float requested = (s->vel - next_velocity) / UI_SCROLL_INERTIA_DECAY;
        float moved = scroll_move(s, requested);
        if (fabsf(moved - requested) > 0.01f) {
            s->vel = 0.0f;
        } else {
            s->vel = next_velocity;
            if (fabsf(s->vel) < UI_SCROLL_MIN_VELOCITY) s->vel = 0.0f;
            if (fabsf(moved) > 0.0f) scroll_show_bar(s);
        }
    }

    if (scroll_max_offset(s) <= 0.0f) {
        s->vel = 0.0f;
        s->bar_hold_s = 0.0f;
        s->bar_alpha = 0.0f;
        s->pointer_was_down = pointer_down;
        return;
    }

    if (s->pressed && s->dragging) {
        scroll_show_bar(s);
    } else if (s->bar_hold_s > 0.0f) {
        s->bar_hold_s -= dt;
        if (s->bar_hold_s < 0.0f) s->bar_hold_s = 0.0f;
    } else if (dt > 0.0f) {
        s->bar_alpha -= dt / UI_SCROLL_BAR_FADE_S;
        if (s->bar_alpha < 0.0f) s->bar_alpha = 0.0f;
    }

    s->pointer_was_down = pointer_down;
}

bool ui_scroll_take_click(UIScroll* s, int* out_x, int* out_y) {
    if (!s || !s->click_pending) return false;
    if (out_x) *out_x = (int)s->click_pos.x;
    if (out_y) *out_y = (int)s->click_pos.y;
    s->click_pending = false;
    return true;
}

float ui_scroll_offset(const UIScroll* s) {
    return s ? s->offset : 0.0f;
}

void ui_scroll_begin(const UIScroll* s) {
    if (!s) return;
    BeginScissorMode((int)s->view.x, (int)s->view.y,
                     (int)s->view.width, (int)s->view.height);
}

void ui_scroll_end(const UIScroll* s) {
    if (!s) return;

    EndScissorMode();
    float max_offset = scroll_max_offset(s);
    if (max_offset <= 0.0f || s->bar_alpha <= 0.0f) return;

    Rectangle bounds = s->has_scrollbar_bounds ? s->scrollbar_bounds : s->view;
    float thumb_h = bounds.height * (s->view.height / s->content_h);
    float min_thumb_h = bounds.height < UI_SCROLL_BAR_MIN_H
                      ? bounds.height : UI_SCROLL_BAR_MIN_H;
    thumb_h = scroll_clamp(thumb_h, min_thumb_h, bounds.height);
    float travel = bounds.height - thumb_h;
    float progress = s->offset / max_offset;
    float thumb_y = bounds.y + travel * progress;
    float thumb_x = bounds.x + bounds.width - UI_SCROLL_BAR_MARGIN - UI_SCROLL_BAR_W;
    Color thumb = { 220, 230, 245, (unsigned char)(190.0f * s->bar_alpha) };
    DrawRectangleRounded((Rectangle){ thumb_x, thumb_y, UI_SCROLL_BAR_W, thumb_h },
                         1.0f, 4, thumb);
}