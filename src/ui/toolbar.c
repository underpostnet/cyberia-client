#include "toolbar.h"

#include "inventory_modal.h"
#include "modal_instance_map.h"
#include "modal_interact.h"
#include "modal_map.h"
#include "quest_journal.h"
#include "ui_button.h"

#include "js/fullscreen_bridge.h"

#include <math.h>
#include <raylib.h>

#define TOOLBAR_SLIDE_DURATION 0.22f
#define TOGGLE_EDGE_MARGIN     3.0f   /* minimal margin around the toggle icon */

static const Color TB_BG   = { 8, 12, 24, 150 };
static const Color TB_LINE = { 70, 190, 240, 60 };

/* Slide parameter: 0 = strip fully visible, 1 = fully hidden. */
static bool  s_hidden  = false;
static float s_slide_t = 0.0f;

static float slide_eased(void) {
    float t = s_slide_t;
    return t * t * (3.0f - 2.0f * t); /* smoothstep */
}

float toolbar_height(void)   { return TOOLBAR_H * (1.0f - slide_eased()); }
float toolbar_offset_y(void) { return -TOOLBAR_H * slide_eased(); }
bool  toolbar_is_hidden(void) { return s_hidden; }

/* Toggle row, right-aligned: index 0 is the rightmost button (fullscreen).
 * All ride the strip's slide. */
static Rectangle btn_rect(int index_from_right) {
    float x = (float)GetScreenWidth()
              - (float)(index_from_right + 1) * (TOOLBAR_BTN_SIZE + TOOLBAR_BTN_MARGIN);
    float y = (TOOLBAR_H - TOOLBAR_BTN_SIZE) * 0.5f + toolbar_offset_y();
    return (Rectangle){ x, y, TOOLBAR_BTN_SIZE, TOOLBAR_BTN_SIZE };
}

/* Hide/show toggle — top-left corner, pinned to the screen so the strip can
 * always be brought back.  Nearly full toolbar height with minimal edge
 * margin so the main logo icon has maximum presence. */
static Rectangle toggle_rect(void) {
    float sz = TOOLBAR_H - TOGGLE_EDGE_MARGIN * 2.0f;
    return (Rectangle){ TOGGLE_EDGE_MARGIN,
                        TOGGLE_EDGE_MARGIN,
                        sz, sz };
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

/* Draw the main-logo toggle button — large icon with a subtle background. */
static void draw_toggle_btn(Rectangle r, const char* icon, Vector2 mouse) {
    float sz = r.height - 4.0f;
    int icon_sz = (int)sz;
    if (icon_sz < 8) icon_sz = 8;

    UIButtonStyle style = {
        .icon_id    = icon,
        .icon_size  = icon_sz,
        .bg         = { 20, 20, 35, 200 },
        .bg_hover   = { 50, 50, 70, 220 },
    };
    ui_button_draw(r, &style,
                   ui_button_resolve_state(true, false, CheckCollisionPointRec(mouse, r)));
}

void toolbar_draw(int screen_width) {
    /* Advance the slide (draw runs once per frame). */
    float dt = GetFrameTime();
    float step = dt / TOOLBAR_SLIDE_DURATION;
    s_slide_t += s_hidden ? step : -step;
    if (s_slide_t < 0.0f) s_slide_t = 0.0f;
    if (s_slide_t > 1.0f) s_slide_t = 1.0f;

    float oy = toolbar_offset_y();
    Vector2 mp = GetMousePosition();

    if (s_slide_t < 1.0f) {
        DrawRectangle(0, (int)oy, screen_width, (int)TOOLBAR_H, TB_BG);
        DrawLine(0, (int)(oy + TOOLBAR_H), screen_width, (int)(oy + TOOLBAR_H), TB_LINE);

        bool fs_active = fullscreen_bridge_is_active();
        bool map_open  = modal_instance_map_is_open();
        draw_btn(btn_rect(0), fs_active ? "shrink" : "fullscreen", fs_active, mp);
        draw_btn(btn_rect(1), map_open ? "close-yellow" : "map", map_open, mp);
        draw_btn(btn_rect(2), "quest", quest_journal_is_visible(), mp);
    }

    draw_toggle_btn(toggle_rect(), s_hidden ? "cyberia-white-0" : "cyberia-yellow-1", mp);
}

bool toolbar_handle_click(int mx, int my) {
    if (ui_button_hit(toggle_rect(), mx, my)) {
        s_hidden = !s_hidden;
        return true;
    }
    if (s_hidden) return false;
    if (ui_button_hit(btn_rect(0), mx, my)) { fullscreen_bridge_toggle(); return true; }
    if (ui_button_hit(btn_rect(1), mx, my)) { modal_instance_map_toggle(); return true; }
    if (ui_button_hit(btn_rect(2), mx, my)) {
        /* The quest button returns to the grid: if any world-covering modal is
         * up, dismiss them all and surface the Quest Journal instead of
         * toggling it. Otherwise it toggles the journal as usual. */
        if (modal_interact_is_open() || inventory_modal_is_open() || modal_instance_map_is_open()) {
            modal_interact_close();
            inventory_modal_close();
            modal_instance_map_close();
            if (!quest_journal_is_visible()) quest_journal_toggle();
        } else {
            quest_journal_toggle();
        }
        return true;
    }
    /* Tapping the map readout expands/retracts the Instance Map too. */
    if (ui_button_hit(modal_map_bounds(), mx, my)) {
        modal_instance_map_toggle();
        return true;
    }
    return false;
}

bool toolbar_covers_point(int mx, int my) {
    if (ui_button_hit(toggle_rect(), mx, my)) return true;
    return !s_hidden && 0 <= my && (float)my < toolbar_height() &&
           0 <= mx && mx < GetScreenWidth();
}
