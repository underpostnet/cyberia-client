#ifndef CYBERIA_UI_SWITCH_H
#define CYBERIA_UI_SWITCH_H

#include <raylib.h>
#include <stdbool.h>

typedef struct {
    const char* label;
    const char* icon_id;
    int font_size;
} UISwitchStyle;

typedef struct {
    bool on;
    float anim_t;
} UISwitch;

void ui_switch_init(UISwitch* control, bool on);
void ui_switch_update(UISwitch* control, bool on, float dt);

/* The caller owns the value and uses the full row as the tap target. */
void ui_switch_draw(const UISwitch* control, Rectangle bounds, const UISwitchStyle* style, bool hovered);

#endif
