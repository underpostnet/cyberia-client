#ifndef CYBERIA_UI_HUD_PANEL_H
#define CYBERIA_UI_HUD_PANEL_H

#include "ui_tabs.h"

#include <raylib.h>

/* hud_panel — the chrome the panels of the right-hand column share (chat,
 * social): the fill and border, one header row of tabs with the close button
 * at its right end, and the content area under it. */

#define HUD_PANEL_HEADER_H 40.0f
#define HUD_PANEL_PAD       6.0f

void      hud_panel_draw_frame(Rectangle panel);
/* The header row of `count` tabs. */
UITabs    hud_panel_tabs(Rectangle panel, int count);
Rectangle hud_panel_close_rect(Rectangle panel);
Rectangle hud_panel_content_rect(Rectangle panel);
void      hud_panel_draw_close(Rectangle panel);

#endif /* CYBERIA_UI_HUD_PANEL_H */
