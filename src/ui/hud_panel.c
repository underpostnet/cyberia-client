#include "hud_panel.h"

#include "modal.h"
#include "ui_button.h"
#include "ui_tabs.h"

#include <raylib.h>

static const Color C_PANEL = { 3, 7, 16, 170 };

static float control_side(void) { return HUD_PANEL_HEADER_H - 2.0f * HUD_PANEL_PAD; }

void hud_panel_draw_frame(Rectangle panel) {
    DrawRectangleRec(panel, C_PANEL);
    DrawRectangleLinesEx(panel, 1.0f, MODAL_PANEL_BORDER);
}

UITabs hud_panel_tabs(Rectangle panel, int count) {
    return ui_tabs_row(count, panel.x + HUD_PANEL_PAD, panel.y + HUD_PANEL_PAD,
                       panel.width - 3.0f * HUD_PANEL_PAD - control_side(), control_side());
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

void hud_panel_draw_close(Rectangle panel) { ui_button_draw_close(hud_panel_close_rect(panel)); }
