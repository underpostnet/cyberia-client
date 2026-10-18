#include "hud_panel.h"

#include "modal.h"
#include "toolbar.h"
#include "ui_button.h"
#include "view/viewport.h"

#include <stddef.h>

static const Color C_PANEL  = {  3,   7,  16, 170 };
static const Color C_TAB    = { 24,  30,  48, 255 };
static const Color C_TAB_ON = { 70, 110, 175, 230 };

static float control_side(void) { return HUD_PANEL_HEADER_H - 2.0f * HUD_PANEL_PAD; }

void hud_panel_draw_frame(Rectangle panel) {
    DrawRectangleRec(panel, C_PANEL);
    DrawRectangleLinesEx(panel, 1.0f, MODAL_PANEL_BORDER);
}

Rectangle hud_panel_tab_rect(Rectangle panel, int index, int count) {
    float side = control_side();
    float w = (panel.width - (float)(count + 2) * HUD_PANEL_PAD - side) / (float)count;
    return (Rectangle){ panel.x + HUD_PANEL_PAD + (float)index * (w + HUD_PANEL_PAD), panel.y + HUD_PANEL_PAD, w,
                        side };
}

Rectangle hud_panel_close_rect(Rectangle panel) {
    float side = control_side();
    return (Rectangle){ panel.x + panel.width - HUD_PANEL_PAD - side, panel.y + HUD_PANEL_PAD, side, side };
}

Rectangle hud_panel_content_rect(Rectangle panel) {
    float top = panel.y + HUD_PANEL_HEADER_H + HUD_PANEL_PAD;
    return (Rectangle){ panel.x + HUD_PANEL_PAD, top, panel.width - 2.0f * HUD_PANEL_PAD,
                        panel.y + panel.height - HUD_PANEL_PAD - top };
}

void hud_panel_draw_tabs(Rectangle panel, const HudPanelTab* tabs, int count, int active) {
    int font = viewport_is_mobile() ? 12 : 13;
    bool icons = true;
    for (int i = 0; count > i; i++) {
        UIButtonPixelRetroStyle probe = { .icon_id = tabs[i].icon, .label = tabs[i].label, .font_size = font };
        icons = icons && ui_button_pixel_retro_fits(hud_panel_tab_rect(panel, i, count), &probe);
    }
    for (int i = 0; count > i; i++) {
        Rectangle r = hud_panel_tab_rect(panel, i, count);
        UIButtonPixelRetroStyle style = {
            .bg = i == active ? C_TAB_ON : C_TAB, .icon_id = icons ? tabs[i].icon : NULL, .label = tabs[i].label,
            .font_size = font, .text_color = tabs[i].enabled ? MODAL_TEXT : MODAL_LABEL,
            .selected = i == active, .enabled = tabs[i].enabled,
        };
        ui_button_pixel_retro_draw(r, &style, tabs[i].enabled && ui_button_hit(r, GetMouseX(), GetMouseY()));
        if (i != active) toolbar_badge_draw(r, tabs[i].count);
    }
}

void hud_panel_draw_close(Rectangle panel) {
    Rectangle r = hud_panel_close_rect(panel);
    UIButtonStyle style = { .icon_id = "close-yellow", .no_fill = true };
    ui_button_draw(r, &style, ui_button_resolve_state(true, false, ui_button_hit(r, GetMouseX(), GetMouseY())));
}
