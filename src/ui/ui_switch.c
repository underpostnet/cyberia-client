#include "ui_switch.h"

#include "ease.h"
#include "text.h"
#include "ui_button.h"
#include "ui_icon.h"
#include "ui_rect.h"

#include <math.h>
#include <raylib.h>

void ui_switch_init(UISwitch* control, bool on) {
    *control = (UISwitch){ .on = on, .anim_t = on ? 1.0f : 0.0f };
}

void ui_switch_update(UISwitch* control, bool on, float dt) {
    control->on = on;
    float target = on ? 1.0f : 0.0f;
    control->anim_t = ease_approach(control->anim_t, target, ease_exp_factor(22.0f, fmaxf(0.0f, dt)));
    if (0.001f > fabsf(target - control->anim_t)) control->anim_t = target;
}

void ui_switch_draw(const UISwitch* control, Rectangle bounds, const UISwitchStyle* style, bool hovered) {
    Color text = { 220, 230, 240, 255 };
    draw_pixel_bevel(bounds, 0.0f, 2.0f, (Color){ 24, 30, 48, 255 },
                     (Color){ 45, 55, 76, 255 }, (Color){ 10, 14, 24, 255 });
    if (hovered) ui_button_draw_ring(bounds, text);

    float cy = bounds.y + bounds.height * 0.5f;
    float icon = fminf(24.0f, bounds.height - 12.0f);
    ui_icon_draw(style->icon_id, bounds.x + 12.0f + icon * 0.5f, cy, (int)icon, false, 0.0f);

    Rectangle track = { roundf(bounds.x + bounds.width - 76.0f), roundf(cy - 14.0f), 64.0f, 28.0f };
    float x = bounds.x + 20.0f + icon;
    int width = (int)(track.x - x - 10.0f);
    int height = text_wrap(style->label, 0, 0, width, style->font_size, text, false, false);
    text_wrap(style->label, (int)x, (int)(cy - (float)height * 0.5f), width,
              style->font_size, text, false, true);

    Color fill = ColorLerp((Color){ 48, 55, 72, 255 }, (Color){ 35, 105, 77, 255 }, control->anim_t);
    draw_pixel_bevel(track, 0.0f, 2.0f, fill, ColorBrightness(fill, 0.25f), ColorBrightness(fill, -0.4f));
    ui_button_draw_ring(track, ColorLerp(text, (Color){ 110, 235, 170, 255 }, control->anim_t));
    const char* state = control->on ? "ON" : "OFF";
    float state_x = control->on ? track.x + 4.0f : track.x + 26.0f;
    DrawText(state, (int)(state_x + (34.0f - (float)MeasureText(state, 11)) * 0.5f),
             (int)(cy - (float)text_line_height(11) * 0.5f), 11, text);
    Rectangle thumb = { roundf(track.x + 4.0f + (track.width - 28.0f) * control->anim_t),
                        track.y + 4.0f, 20.0f, 20.0f };
    draw_pixel_bevel(thumb, 0.0f, 2.0f, text, WHITE, (Color){ 100, 120, 145, 255 });
    DrawRectangle((int)thumb.x + 8, (int)thumb.y + 6, 2, 8, fill);
}
