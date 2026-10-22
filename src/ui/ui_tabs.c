#include "ui_tabs.h"

#include "modal.h"
#include "toolbar.h"
#include "ui_button.h"
#include "view/viewport.h"

#include <assert.h>
#include <raylib.h>
#include <stdbool.h>
#include <stddef.h>

int ui_tabs_font(void) { return viewport_is_mobile() ? 12 : 14; }

static UITabs grid(int count, int columns, float x, float y, float width, float tab_h) {
    float tab_w = (width - (float)(columns - 1) * UI_TABS_GAP) / (float)columns;
    return (UITabs){ x, y, tab_w, tab_h, count, columns };
}

static bool every_tab_fits(const UITabs* group, const UITab* tabs) {
    for (int i = 0; group->count > i; i++) {
        UIButtonPixelRetroStyle probe = { .icon_id = tabs[i].icon, .label = tabs[i].label,
                                          .font_size = ui_tabs_font() };
        if (!ui_button_pixel_retro_fits(ui_tabs_rect(group, i), &probe)) return false;
    }
    return true;
}

UITabs ui_tabs_wrap(const UITab* tabs, int count, float x, float y, float width, float tab_h) {
    assert(0 < count);
    for (int per_row = UI_TABS_ROW_MAX < count ? UI_TABS_ROW_MAX : count;; per_row--) {
        int rows = (count + per_row - 1) / per_row;
        UITabs group = grid(count, (count + rows - 1) / rows, x, y, width, tab_h);
        if (1 == per_row || every_tab_fits(&group, tabs)) return group;
    }
}

UITabs ui_tabs_row(int count, float x, float y, float width, float tab_h) {
    assert(0 < count);
    return grid(count, count, x, y, width, tab_h);
}

float ui_tabs_height(const UITabs* group) {
    int rows = (group->count + group->columns - 1) / group->columns;
    return (float)rows * group->tab_h + (float)(rows - 1) * UI_TABS_GAP;
}

Rectangle ui_tabs_rect(const UITabs* group, int index) {
    int column = index % group->columns, row = index / group->columns;
    return (Rectangle){ group->x + (float)column * (group->tab_w + UI_TABS_GAP),
                        group->y + (float)row * (group->tab_h + UI_TABS_GAP), group->tab_w, group->tab_h };
}

int ui_tabs_hit(const UITabs* group, int mx, int my) {
    for (int i = 0; group->count > i; i++)
        if (ui_button_hit(ui_tabs_rect(group, i), mx, my)) return i;
    return -1;
}

void ui_tabs_draw(const UITabs* group, const UITab* tabs, int active) {
    bool icons = every_tab_fits(group, tabs);
    int mx = GetMouseX(), my = GetMouseY();
    for (int i = 0; group->count > i; i++) {
        Rectangle r = ui_tabs_rect(group, i);
        UIButtonPixelRetroStyle style = {
            .bg = i == active ? MODAL_SELECTED : MODAL_BUTTON, .icon_id = icons ? tabs[i].icon : NULL,
            .label = tabs[i].label, .font_size = ui_tabs_font(),
            .text_color = tabs[i].enabled ? MODAL_TEXT : MODAL_LABEL, .selected = i == active,
            .enabled = tabs[i].enabled,
        };
        ui_button_pixel_retro_draw(r, &style, tabs[i].enabled && ui_button_hit(r, mx, my));
        if (i != active) toolbar_badge_draw(r, tabs[i].badge);
    }
}
