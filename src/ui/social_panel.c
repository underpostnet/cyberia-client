#include "social_panel.h"

#include "hud_panel.h"
#include "hud_side_stack.h"
#include "player_panel.h"
#include "sim/local_player.h"
#include "ui_button.h"

#include <raylib.h>

typedef enum { SP_TAB_FRIENDS, SP_TAB_REQUESTS, SP_TAB_PARTY, SP_TAB_COUNT } SocialTab;

static const PlayerView SP_VIEW[SP_TAB_COUNT]  = { PLAYER_VIEW_FRIENDS, PLAYER_VIEW_REQUESTS, PLAYER_VIEW_PARTY };
static const char*      SP_LABEL[SP_TAB_COUNT] = { "Friends", "Requests", "Party" };
static const char*      SP_ICON[SP_TAB_COUNT]  = { "friends", "sign-up", "party" };

static bool      s_visible = false;
static SocialTab s_tab = SP_TAB_FRIENDS;

static Rectangle panel_rect(void) { return hud_side_stack_social_slot(); }

static int tab_news(SocialTab tab) {
    switch (tab) {
        case SP_TAB_REQUESTS: return local_player_incoming_requests();
        case SP_TAB_PARTY:    return g_local_player.invite_count;
        default:              return 0;
    }
}

/* A guest has no friends: it opens on the party. */
static void show_news(void) {
    if (!g_local_player.account) s_tab = SP_TAB_PARTY;
    else if (0 < tab_news(SP_TAB_REQUESTS)) s_tab = SP_TAB_REQUESTS;
    else if (0 < tab_news(SP_TAB_PARTY)) s_tab = SP_TAB_PARTY;
}

void social_panel_toggle(void) {
    if (s_visible) {
        social_panel_close();
        return;
    }
    s_visible = true;
    show_news();
}

void social_panel_close(void) {
    if (!s_visible) return;
    s_visible = false;
    player_panel_hide_input();
}

bool social_panel_is_visible(void) { return s_visible; }

void social_panel_update(float dt) {
    if (!s_visible) return;
    Rectangle panel = panel_rect();
    if (HUD_PANEL_HEADER_H > panel.height) return;
    player_panel_update(SP_VIEW[s_tab], hud_panel_content_rect(panel), true, dt);
}

void social_panel_draw(void) {
    if (!s_visible) return;
    Rectangle panel = panel_rect();
    if (HUD_PANEL_HEADER_H > panel.height) return;
    hud_panel_draw_frame(panel);
    HudPanelTab tabs[SP_TAB_COUNT];
    for (SocialTab tab = 0; SP_TAB_COUNT > tab; tab++)
        tabs[tab] = (HudPanelTab){ SP_LABEL[tab], SP_ICON[tab], true, tab_news(tab) };
    hud_panel_draw_tabs(panel, tabs, SP_TAB_COUNT, s_tab);
    hud_panel_draw_close(panel);
    player_panel_draw(SP_VIEW[s_tab], hud_panel_content_rect(panel), g_local_player.id);
}

bool social_panel_covers_point(int mx, int my) {
    return s_visible && CheckCollisionPointRec((Vector2){ (float)mx, (float)my }, panel_rect());
}

bool social_panel_handle_click(int mx, int my) {
    if (!social_panel_covers_point(mx, my)) return false;
    Rectangle panel = panel_rect();
    if (ui_button_hit(hud_panel_close_rect(panel), mx, my)) {
        social_panel_close();
        return true;
    }
    for (SocialTab tab = 0; SP_TAB_COUNT > tab; tab++) {
        if (!ui_button_hit(hud_panel_tab_rect(panel, tab, SP_TAB_COUNT), mx, my)) continue;
        if (tab != s_tab) player_panel_hide_input();
        s_tab = tab;
        return true;
    }
    player_panel_press(SP_VIEW[s_tab], hud_panel_content_rect(panel), mx, my);
    return true;
}

bool social_panel_handle_wheel(float wheel_delta) {
    if (!social_panel_covers_point(GetMouseX(), GetMouseY())) return false;
    player_panel_wheel(SP_VIEW[s_tab], hud_panel_content_rect(panel_rect()), wheel_delta);
    return true;
}
