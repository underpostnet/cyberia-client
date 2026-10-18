#ifndef CYBERIA_UI_HUD_PANEL_H
#define CYBERIA_UI_HUD_PANEL_H

#include <raylib.h>
#include <stdbool.h>

/* hud_panel — the chrome the panels of the right-hand column share (chat,
 * social): the fill and border, one header row of tabs with the close button
 * at its right end, and the content area under it. */

#define HUD_PANEL_HEADER_H 40.0f
#define HUD_PANEL_PAD       6.0f

void      hud_panel_draw_frame(Rectangle panel);
Rectangle hud_panel_tab_rect(Rectangle panel, int index, int count);
Rectangle hud_panel_close_rect(Rectangle panel);
Rectangle hud_panel_content_rect(Rectangle panel);

/* One header tab. `count` shows as a badge while the tab is not active. */
typedef struct {
    const char* label;
    const char* icon;
    bool        enabled;
    int         count;
} HudPanelTab;

/* The header tabs. Their icons show only while every tab fits its icon and
 * its label. */
void      hud_panel_draw_tabs(Rectangle panel, const HudPanelTab* tabs, int count, int active);
void      hud_panel_draw_close(Rectangle panel);

#endif /* CYBERIA_UI_HUD_PANEL_H */
