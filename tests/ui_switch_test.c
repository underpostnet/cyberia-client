#include "ui/ui_switch.h"

#include <assert.h>
#include <math.h>
#include <stdbool.h>
#include <stdio.h>

int main(void) {
    UISwitch control;
    ui_switch_init(&control, false);
    assert(!control.on && 0.0f == control.anim_t);
    ui_switch_update(&control, true, 0.016f);
    assert(control.on && 0.0f < control.anim_t && 1.0f > control.anim_t);
    float first = control.anim_t;
    ui_switch_update(&control, true, 0.016f);
    assert(first < control.anim_t && 1.0f > control.anim_t);
    float before_reverse = control.anim_t;
    ui_switch_update(&control, false, 0.016f);
    assert(!control.on && before_reverse > control.anim_t && 0.0f < control.anim_t);
    float before_pause = control.anim_t;
    ui_switch_update(&control, false, 0.0f);
    assert(before_pause == control.anim_t);
    ui_switch_update(&control, false, -1.0f);
    assert(before_pause == control.anim_t);
    ui_switch_update(&control, false, 1.0f);
    assert(0.0f == control.anim_t);

    UISwitch half_steps;
    ui_switch_init(&half_steps, true);
    ui_switch_init(&control, true);
    ui_switch_update(&control, false, 0.032f);
    ui_switch_update(&half_steps, false, 0.016f);
    ui_switch_update(&half_steps, false, 0.016f);
    assert(0.0001f > fabsf(control.anim_t - half_steps.anim_t));
    ui_switch_update(&control, true, 1.0f);
    assert(control.on && 1.0f == control.anim_t);

    puts("ui_switch_test OK");
    return 0;
}
