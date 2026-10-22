#include "ui/ui_tabs.h"
#include "ui/ui_button.h"

#include <assert.h>
#include <math.h>
#include <raylib.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

const Color MODAL_TEXT = { 0 }, MODAL_LABEL = { 0 }, MODAL_BUTTON = { 0 }, MODAL_SELECTED = { 0 };
static int icons, selected, badges;

bool viewport_is_mobile(void) { return false; }
int GetMouseX(void) { return 0; }
int GetMouseY(void) { return 0; }
bool ui_button_hit(Rectangle b, int mx, int my) {
    return mx >= b.x && mx < b.x + b.width && my >= b.y && my < b.y + b.height;
}
/* A tab fits 10 px per label letter and 40 px for its icon and padding. */
bool ui_button_pixel_retro_fits(Rectangle r, const UIButtonPixelRetroStyle* style) {
    return (float)(strlen(style->label) * 10 + 40) <= r.width;
}
void ui_button_pixel_retro_draw(Rectangle r, const UIButtonPixelRetroStyle* style, bool hovered) {
    icons += NULL != style->icon_id;
    selected += style->selected;
}
void toolbar_badge_draw(Rectangle r, int count) { badges += count; }

static const UITab TABS[] = {
    { "Shop", "home-red", true, 0 }, { "Assembly", "engine", true, 0 }, { "Buyback", "buyback", true, 0 },
    { "Salvage", "burn", true, 0 },  { "Market", "market", true, 0 },   { "Storage", "grid", true, 0 },
    { "Quest", "quest", true, 2 },   { "Stack", "stack", true, 0 },     { "Stats", "stats", true, 0 },
};

/* Nine tabs make three rows of three: four to a row would clip "Assembly". */
static void test_rows_share_the_tabs(void) {
    UITabs group = ui_tabs_wrap(TABS, 9, 10.0f, 20.0f, 400.0f, 40.0f);
    assert(3 == group.columns);
    assert(3.0f * 40.0f + 2.0f * UI_TABS_GAP == ui_tabs_height(&group));
    Rectangle last = ui_tabs_rect(&group, 8);
    assert(10.0f + 400.0f == roundf(last.x + last.width) && 20.0f + 2.0f * (40.0f + UI_TABS_GAP) == last.y);
    assert(4 == ui_tabs_hit(&group, (int)(last.x - group.tab_w * 0.5f), (int)(20.0f + 40.0f + UI_TABS_GAP + 1.0f)));
    assert(-1 == ui_tabs_hit(&group, 10, (int)(20.0f + 40.0f + 1.0f)));

    group = ui_tabs_wrap(TABS, 4, 0.0f, 0.0f, 600.0f, 40.0f);
    assert(4 == group.columns && 40.0f == ui_tabs_height(&group));
    group = ui_tabs_wrap(TABS, 5, 0.0f, 0.0f, 600.0f, 40.0f);
    assert(3 == group.columns && 2.0f * 40.0f + UI_TABS_GAP == ui_tabs_height(&group));
}

/* A narrow strip takes fewer tabs to a row; one tab to a row is the floor. */
static void test_narrow_strips_take_fewer_columns(void) {
    UITabs group = ui_tabs_wrap(TABS, 9, 0.0f, 0.0f, 260.0f, 32.0f);
    assert(2 == group.columns && 5.0f * 32.0f + 4.0f * UI_TABS_GAP == ui_tabs_height(&group));
    group = ui_tabs_wrap(TABS, 9, 0.0f, 0.0f, 100.0f, 32.0f);
    assert(1 == group.columns);
    group = ui_tabs_row(3, 0.0f, 0.0f, 100.0f, 28.0f);
    assert(3 == group.columns && 28.0f == ui_tabs_height(&group));
}

/* Icons show only while every tab fits them; the active tab shows no badge. */
static void test_draw(void) {
    UITabs group = ui_tabs_wrap(TABS, 9, 0.0f, 0.0f, 400.0f, 40.0f);
    ui_tabs_draw(&group, TABS, 1);
    assert(9 == icons && 1 == selected && 2 == badges);
    icons = selected = badges = 0;
    ui_tabs_draw(&group, TABS, 6);
    assert(0 == badges);
    icons = 0;
    group = ui_tabs_row(9, 0.0f, 0.0f, 400.0f, 40.0f);
    ui_tabs_draw(&group, TABS, -1);
    assert(0 == icons);
}

int main(void) {
    test_rows_share_the_tabs();
    test_narrow_strips_take_fewer_columns();
    test_draw();
    puts("ui_tabs_test OK");
    return 0;
}
