#include "toolbar.h"

#include "modal_instance_map.h"
#include "quest_journal.h"
#include "ui_button.h"

#include "js/fullscreen_bridge.h"

#include <raylib.h>

static const Color TB_BG   = { 8, 12, 24, 150 };
static const Color TB_LINE = { 70, 190, 240, 60 };

float toolbar_height(void) { return TOOLBAR_H; }

/* Toggle row, right-aligned: index 0 is the rightmost button. */
static Rectangle btn_rect(int index_from_right) {
    float x = (float)GetScreenWidth()
              - (float)(index_from_right + 1) * (TOOLBAR_BTN_SIZE + TOOLBAR_BTN_MARGIN);
    float y = (TOOLBAR_H - TOOLBAR_BTN_SIZE) * 0.5f;
    return (Rectangle){ x, y, TOOLBAR_BTN_SIZE, TOOLBAR_BTN_SIZE };
}

static void draw_btn(Rectangle r, const char* icon, bool selected, Vector2 mouse) {
    UIButtonStyle style = {
        .icon_id    = icon,
        .icon_size  = (int)TOOLBAR_BTN_SIZE - 12,
        .bg         = { 20, 20, 35, 200 },
        .bg_hover   = { 50, 50, 70, 220 },
        .border     = { 80, 80, 120, 180 },
        .border_selected = { 90, 210, 250, 220 },
    };
    ui_button_draw(r, &style,
                   ui_button_resolve_state(true, selected, CheckCollisionPointRec(mouse, r)));
}

void toolbar_draw(int screen_width) {
    DrawRectangle(0, 0, screen_width, (int)TOOLBAR_H, TB_BG);
    DrawLine(0, (int)TOOLBAR_H, screen_width, (int)TOOLBAR_H, TB_LINE);

    Vector2 mp = GetMousePosition();
    bool fs_active = fullscreen_bridge_is_active();
    bool map_open  = modal_instance_map_is_open();
    draw_btn(btn_rect(0), fs_active ? "shrink" : "fullscreen", fs_active, mp);
    draw_btn(btn_rect(1), map_open ? "close-yellow" : "map", map_open, mp);
    draw_btn(btn_rect(2), "quest", quest_journal_is_visible(), mp);
}

bool toolbar_handle_click(int mx, int my) {
    if (ui_button_hit(btn_rect(0), mx, my)) { fullscreen_bridge_toggle(); return true; }
    if (ui_button_hit(btn_rect(1), mx, my)) { modal_instance_map_toggle(); return true; }
    if (ui_button_hit(btn_rect(2), mx, my)) { quest_journal_toggle(); return true; }
    return false;
}

bool toolbar_covers_point(int mx, int my) {
    return 0 <= my && (float)my < TOOLBAR_H && 0 <= mx && mx < GetScreenWidth();
}
