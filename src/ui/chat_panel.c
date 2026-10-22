#include "chat_panel.h"

#include "chat_log.h"
#include "chat_pane.h"
#include "emoji.h"
#include "hud_panel.h"
#include "hud_side_stack.h"
#include "identity_icon.h"
#include "modal.h"
#include "modal_instance_map.h"
#include "sim/local_player.h"
#include "social_panel.h"
#include "text.h"
#include "ui_button.h"
#include "ui_scroll.h"
#include "ui_tabs.h"
#include "utils.h"
#include "view/viewport.h"

#include <limits.h>
#include <raylib.h>
#include <stdio.h>
#include <string.h>

#define CP_PAD       HUD_PANEL_PAD
#define CP_BACK_H    30.0f
#define CP_ROWS_MAX  (CHAT_LOG_CONVERSATIONS + FRIENDS_LIST_MAX)

typedef enum { CP_TAB_WORLD, CP_TAB_PARTY, CP_TAB_DIRECT, CP_TAB_COUNT } ChatTab;

static const Color C_BADGE  = { 210,  60,  60, 245 };
static const Color C_ROW    = {  14,  20,  36, 220 };
static const Color C_NOTE   = { 150, 160, 185, 255 };

/* One row of the direct list: a conversation, or a friend online here. */
typedef struct {
    EntityID    peer;
    const char* name;
    const char* note;
    int         unread;
} DirectRow;

static bool     s_visible = false;
static ChatTab  s_tab = CP_TAB_WORLD;
static EntityID s_peer = 0;      /* the open direct conversation, 0 for the list */
static ChatPane s_pane;
static UIScroll s_list_scroll;
static float    s_list_h;
static DirectRow s_rows[CP_ROWS_MAX];
static char      s_notes[CP_ROWS_MAX][CYBERIA_STATUS_NOTE_MAX_RUNES * 4 + 1];
static int       s_row_count;

static bool in_party(void) { return 0 < g_local_player.party.member_count; }

/* The held pointer of the press that shows the list never opens a row of it. */
static void show_direct_list(void) {
    s_peer = 0;
    ui_scroll_reset(&s_list_scroll);
    ui_scroll_cancel_press(&s_list_scroll);
}

static int tab_unread(ChatTab tab) {
    switch (tab) {
        case CP_TAB_WORLD: return chat_log_unread(chat_channel(CHAT_CHANNEL_WORLD));
        case CP_TAB_PARTY: return in_party() ? chat_log_unread(chat_channel(CHAT_CHANNEL_PARTY)) : 0;
        default:           return chat_log_unread_direct();
    }
}

/* Shows where unread lines wait: the shown tab when it holds some, else the
 * first tab that does; on Direct, the list when another conversation holds them. */
static void show_unread(void) {
    for (int tab = 0; 0 == tab_unread(s_tab) && CP_TAB_COUNT > tab; tab++) {
        if (0 < tab_unread((ChatTab)tab)) s_tab = (ChatTab)tab;
    }
    if (CP_TAB_DIRECT == s_tab && 0 != s_peer && 0 == chat_log_unread(chat_direct(s_peer))) s_peer = 0;
}

void chat_panel_toggle(void) {
    s_visible = !s_visible;
    if (s_visible) show_unread();
    else chat_pane_hide(&s_pane);
}

bool chat_panel_is_visible(void) { return s_visible; }

void chat_panel_open_direct(EntityID peer, const char* name) {
    chat_log_open_direct(peer, name);
    social_panel_close();
    s_visible = true;
    s_tab = CP_TAB_DIRECT;
    s_peer = peer;
}

static Rectangle panel_rect(void) { return hud_side_stack_chat_slot(); }

/* The back row above an open direct conversation, and the pane under it. */
static Rectangle back_rect(Rectangle content) { return (Rectangle){ content.x, content.y, content.width, CP_BACK_H }; }

static Rectangle pane_rect(Rectangle content) {
    float top = content.y + CP_BACK_H + CP_PAD;
    return (Rectangle){ content.x, top, content.width, content.y + content.height - top };
}

static inline float row_h(void) { return viewport_is_mobile() ? 40.0f : 44.0f; }

/* The open conversations, then each friend online here without one. */
static void collect_rows(void) {
    s_row_count = 0;
    for (int i = 0; chat_log_count() > i; i++) {
        const ChatHistory* h = chat_log_at(i);
        if (CHAT_CHANNEL_DIRECT != h->conversation.channel) continue;
        DirectRow* row = &s_rows[s_row_count];
        const ChatLine* last = 0 < h->count ? &h->lines[h->count - 1] : NULL;
        copy_str(s_notes[s_row_count], sizeof s_notes[0], NULL != last ? last->text : "No messages yet");
        *row = (DirectRow){ h->conversation.peer, h->peer_name, s_notes[s_row_count], h->unread };
        s_row_count++;
    }
    for (int i = 0; g_local_player.friend_count > i && CP_ROWS_MAX > s_row_count; i++) {
        const Friend* f = &g_local_player.friends[i];
        if (FRIEND_ACCEPTED != f->status || 0 == f->entity_id || NULL != chat_log_get(chat_direct(f->entity_id)))
            continue;
        int n = s_row_count++;
        copy_str(s_notes[n], sizeof s_notes[0], '\0' != f->note[0] ? f->note : "Online");
        s_rows[n] = (DirectRow){ f->entity_id, f->name, s_notes[n], 0 };
    }
}

static Rectangle row_rect(Rectangle content, int index) {
    return (Rectangle){ content.x, content.y + (float)index * (row_h() + CP_PAD) - ui_scroll_offset(&s_list_scroll),
                        content.width, row_h() };
}

int chat_panel_unread(void) {
    return tab_unread(CP_TAB_WORLD) + tab_unread(CP_TAB_PARTY) + tab_unread(CP_TAB_DIRECT);
}

static ChatConversation shown_conversation(void) {
    return CP_TAB_DIRECT == s_tab ? chat_direct(s_peer)
                                  : chat_channel(CP_TAB_PARTY == s_tab ? CHAT_CHANNEL_PARTY : CHAT_CHANNEL_WORLD);
}

void chat_panel_update(float dt) {
    /* The party channel ends with the party. */
    if (CP_TAB_PARTY == s_tab && !in_party()) s_tab = CP_TAB_WORLD;
    if (!s_visible) return;
    Rectangle content = hud_panel_content_rect(panel_rect());
    if (CP_TAB_DIRECT == s_tab && 0 == s_peer) {
        chat_pane_hide(&s_pane);
        collect_rows();
        s_list_h = (float)s_row_count * (row_h() + CP_PAD);
        ui_scroll_update(&s_list_scroll, content, s_list_h, dt);
        int x, y;
        if (!ui_scroll_take_click(&s_list_scroll, &x, &y)) return;
        for (int i = 0; s_row_count > i; i++) {
            if (!ui_button_hit(row_rect(content, i), x, y)) continue;
            chat_panel_open_direct(s_rows[i].peer, s_rows[i].name);
            return;
        }
        return;
    }
    Rectangle pane = CP_TAB_DIRECT == s_tab ? pane_rect(content) : content;
    chat_pane_bind(&s_pane, shown_conversation());
    bool input_visible = !modal_any_open() && !modal_instance_map_is_open() && 0.0f < pane.height;
    chat_pane_update(&s_pane, pane, input_visible, dt);
}

static void draw_count(float cx, float cy, int count) {
    char text[8];
    snprintf(text, sizeof text, "%d", 99 < count ? 99 : count);
    DrawCircle((int)cx, (int)cy, 8.0f, C_BADGE);
    DrawText(text, (int)(cx - (float)MeasureText(text, 10) * 0.5f), (int)(cy - 5.0f), 10, WHITE);
}

static void draw_list(Rectangle content) {
    int font = viewport_is_mobile() ? 12 : 14;
    if (0 == s_row_count) {
        text_wrap("Open a player's profile and tap Chat, or add friends to write to them here.", (int)content.x,
                  (int)content.y, (int)content.width, font, C_NOTE, false, true);
        return;
    }
    ui_scroll_begin(&s_list_scroll);
    for (int i = 0; s_row_count > i; i++) {
        const DirectRow* row = &s_rows[i];
        Rectangle r = row_rect(content, i);
        DrawRectangleRec(r, C_ROW);
        if (ui_button_hit(r, GetMouseX(), GetMouseY())) DrawRectangleLinesEx(r, 1.0f, MODAL_PANEL_BORDER);
        float face = r.height - CP_PAD;
        identity_preview_draw(row->name, row->note,
                               (Rectangle){ r.x + CP_PAD * 0.5f, r.y + CP_PAD * 0.5f,
                                            r.width - CP_PAD - 24.0f, face }, font, MODAL_TEXT, C_NOTE);
        if (0 < row->unread) draw_count(r.x + r.width - 14.0f, r.y + r.height * 0.5f, row->unread);
    }
    ui_scroll_end(&s_list_scroll);
}

static void draw_conversation_header(Rectangle content) {
    Rectangle back = back_rect(content);
    const ChatHistory* h = chat_log_get(chat_direct(s_peer));
    const Friend* friend = local_player_friend_of(s_peer);
    UIButtonPixelRetroStyle style = {
        .bg = MODAL_BUTTON, .icon_id = "arrow-left", .font_size = 14, .text_color = MODAL_TEXT, .enabled = true,
    };
    Rectangle arrow = { back.x, back.y, back.height, back.height };
    ui_button_pixel_retro_draw(arrow, &style, ui_button_hit(back, GetMouseX(), GetMouseY()));
    if (NULL != h)
        identity_preview_draw(h->peer_name,
                               NULL != friend && FRIEND_ACCEPTED == friend->status ? friend->note : "",
                               (Rectangle){ arrow.x + arrow.width + 4.0f, back.y,
                                            back.width - arrow.width - 4.0f, back.height },
                               viewport_is_mobile() ? 12 : 14, MODAL_TEXT, C_NOTE);
}

void chat_panel_draw(void) {
    if (!s_visible) return;
    Rectangle panel = panel_rect();
    if (HUD_PANEL_HEADER_H > panel.height) return;
    hud_panel_draw_frame(panel);
    UITab tabs[CP_TAB_COUNT] = {
        [CP_TAB_WORLD]  = { "World", "chat", true, tab_unread(CP_TAB_WORLD) },
        [CP_TAB_PARTY]  = { "Party", "party", in_party(), tab_unread(CP_TAB_PARTY) },
        [CP_TAB_DIRECT] = { "Direct", "character", true, tab_unread(CP_TAB_DIRECT) },
    };
    UITabs group = hud_panel_tabs(panel, CP_TAB_COUNT);
    ui_tabs_draw(&group, tabs, s_tab);
    hud_panel_draw_close(panel);
    Rectangle content = hud_panel_content_rect(panel);
    if (CP_TAB_DIRECT != s_tab) {
        chat_pane_draw(&s_pane, content);
    } else if (0 == s_peer) {
        draw_list(content);
    } else {
        draw_conversation_header(content);
        chat_pane_draw(&s_pane, pane_rect(content));
    }
}

bool chat_panel_covers_point(int mx, int my) {
    return s_visible && CheckCollisionPointRec((Vector2){ (float)mx, (float)my }, panel_rect());
}

bool chat_panel_handle_click(int mx, int my) {
    if (!chat_panel_covers_point(mx, my)) return false;
    Rectangle panel = panel_rect();
    Rectangle content = hud_panel_content_rect(panel);
    if (ui_button_hit(hud_panel_close_rect(panel), mx, my)) {
        chat_panel_toggle();
        return true;
    }
    UITabs group = hud_panel_tabs(panel, CP_TAB_COUNT);
    int tab = ui_tabs_hit(&group, mx, my);
    if (0 <= tab) {
        if (CP_TAB_PARTY != tab || in_party()) {
            if (CP_TAB_DIRECT == tab && CP_TAB_DIRECT == s_tab) show_direct_list();
            s_tab = (ChatTab)tab;
        }
        return true;
    }
    if (CP_TAB_DIRECT == s_tab && 0 == s_peer) {
        ui_scroll_on_press(&s_list_scroll, mx, my);
        return true;
    }
    if (CP_TAB_DIRECT == s_tab && ui_button_hit(back_rect(content), mx, my)) {
        show_direct_list();
        return true;
    }
    chat_pane_press(&s_pane, CP_TAB_DIRECT == s_tab ? pane_rect(content) : content, mx, my);
    return true;
}

bool chat_panel_handle_wheel(float wheel_delta) {
    if (!chat_panel_covers_point(GetMouseX(), GetMouseY())) return false;
    Rectangle content = hud_panel_content_rect(panel_rect());
    if (CP_TAB_DIRECT == s_tab && 0 == s_peer)
        return ui_scroll_on_wheel(&s_list_scroll, content, s_list_h, wheel_delta);
    chat_pane_wheel(&s_pane, CP_TAB_DIRECT == s_tab ? pane_rect(content) : content, wheel_delta);
    return true;
}
