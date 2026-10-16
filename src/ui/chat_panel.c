#include "chat_panel.h"

#include "chat_log.h"
#include "chat_pane.h"
#include "emoji.h"
#include "hud_side_stack.h"
#include "identity_icon.h"
#include "modal.h"
#include "modal_instance_map.h"
#include "sim/local_player.h"
#include "text.h"
#include "ui_button.h"
#include "ui_scroll.h"
#include "utils.h"
#include "view/viewport.h"

#include <limits.h>
#include <raylib.h>
#include <stdio.h>
#include <string.h>

#define CP_HEADER_H  34.0f
#define CP_PAD        6.0f
#define CP_BACK_H    30.0f
#define CP_ROWS_MAX  (CHAT_LOG_CONVERSATIONS + FRIENDS_LIST_MAX)

typedef enum { CP_TAB_WORLD, CP_TAB_PARTY, CP_TAB_DIRECT, CP_TAB_COUNT } ChatTab;

static const Color C_PANEL  = {   3,   7,  16, 170 };
static const Color C_TAB    = {  24,  30,  48, 255 };
static const Color C_TAB_ON = {  70, 110, 175, 230 };
static const Color C_BADGE  = { 210,  60,  60, 245 };
static const Color C_ROW    = {  14,  20,  36, 220 };
static const Color C_NOTE   = { 150, 160, 185, 255 };

/* One row of the direct list: a conversation, or a friend online here. */
typedef struct {
    EntityID    peer;
    const char* name;
    const char* skin;
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
static char      s_notes[CP_ROWS_MAX][48];
static int       s_row_count;

static bool in_party(void) { return 0 < g_local_player.party.member_count; }

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

void chat_panel_open_direct(EntityID peer, const char* name, const char* skin) {
    chat_log_open_direct(peer, name, skin);
    s_visible = true;
    s_tab = CP_TAB_DIRECT;
    s_peer = peer;
}

float chat_panel_desired_height(void) {
    return CP_HEADER_H + 2.0f * CP_PAD + CHAT_PANE_HEIGHT;
}

static Rectangle panel_rect(void) {
    return hud_side_stack_chat_slot(chat_panel_desired_height());
}

static Rectangle close_rect(Rectangle panel) {
    float size = CP_HEADER_H - 2.0f * CP_PAD;
    return (Rectangle){ panel.x + panel.width - CP_PAD - size, panel.y + CP_PAD, size, size };
}

static Rectangle tab_rect(Rectangle panel, ChatTab tab) {
    float h = CP_HEADER_H - 2.0f * CP_PAD;
    float w = (panel.width - (float)(CP_TAB_COUNT + 2) * CP_PAD - h) / (float)CP_TAB_COUNT;
    return (Rectangle){ panel.x + CP_PAD + (float)tab * (w + CP_PAD), panel.y + CP_PAD, w, h };
}

static Rectangle content_rect(Rectangle panel) {
    float top = panel.y + CP_HEADER_H + CP_PAD;
    return (Rectangle){ panel.x + CP_PAD, top, panel.width - 2.0f * CP_PAD,
                        panel.y + panel.height - CP_PAD - top };
}

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
        *row = (DirectRow){ h->conversation.peer, h->peer_name, h->peer_skin, s_notes[s_row_count], h->unread };
        s_row_count++;
    }
    for (int i = 0; g_local_player.friend_count > i && CP_ROWS_MAX > s_row_count; i++) {
        const Friend* f = &g_local_player.friends[i];
        if (FRIEND_ACCEPTED != f->status || 0 == f->entity_id || NULL != chat_log_get(chat_direct(f->entity_id)))
            continue;
        int n = s_row_count++;
        snprintf(s_notes[n], sizeof s_notes[0], "Online  %s", f->map_code);
        s_rows[n] = (DirectRow){ f->entity_id, f->name, f->skin, s_notes[n], 0 };
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
    Rectangle content = content_rect(panel_rect());
    if (CP_TAB_DIRECT == s_tab && 0 == s_peer) {
        chat_pane_hide(&s_pane);
        collect_rows();
        s_list_h = (float)s_row_count * (row_h() + CP_PAD);
        ui_scroll_update(&s_list_scroll, content, s_list_h, dt);
        int x, y;
        if (!ui_scroll_take_click(&s_list_scroll, &x, &y)) return;
        for (int i = 0; s_row_count > i; i++) {
            if (!ui_button_hit(row_rect(content, i), x, y)) continue;
            chat_panel_open_direct(s_rows[i].peer, s_rows[i].name, s_rows[i].skin);
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

static void draw_tab(Rectangle panel, ChatTab tab, const char* label, const char* icon, bool enabled, int unread) {
    Rectangle r = tab_rect(panel, tab);
    bool active = tab == s_tab;
    UIButtonPixelRetroStyle style = {
        .bg = active ? C_TAB_ON : C_TAB, .icon_id = icon, .label = label,
        .font_size = viewport_is_mobile() ? 12 : 13, .text_color = enabled ? MODAL_TEXT : MODAL_LABEL,
        .selected = active, .enabled = enabled,
    };
    ui_button_pixel_retro_draw(r, &style, enabled && ui_button_hit(r, GetMouseX(), GetMouseY()));
    if (!active && 0 < unread) draw_count(r.x + r.width - 4.0f, r.y + 4.0f, unread);
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
        identity_icon_draw(row->skin, (Rectangle){ r.x + CP_PAD * 0.5f, r.y + CP_PAD * 0.5f, face, face });
        float tx = r.x + face + CP_PAD * 2.0f;
        DrawText(row->name, (int)tx, (int)(r.y + 4.0f), font, MODAL_TEXT);
        emoji_text_wrap(row->note, (int)tx, (int)(r.y + 6.0f + (float)font), INT_MAX, font - 2, C_NOTE, true);
        if (0 < row->unread) draw_count(r.x + r.width - 14.0f, r.y + r.height * 0.5f, row->unread);
    }
    ui_scroll_end(&s_list_scroll);
}

static void draw_conversation_header(Rectangle content) {
    Rectangle back = back_rect(content);
    const ChatHistory* h = chat_log_get(chat_direct(s_peer));
    UIButtonPixelRetroStyle style = {
        .bg = C_TAB, .icon_id = "arrow-left", .label = NULL != h ? h->peer_name : "",
        .font_size = viewport_is_mobile() ? 12 : 14, .text_color = MODAL_TEXT, .enabled = true,
    };
    ui_button_pixel_retro_draw(back, &style, ui_button_hit(back, GetMouseX(), GetMouseY()));
}

void chat_panel_draw(void) {
    if (!s_visible) return;
    Rectangle panel = panel_rect();
    if (CP_HEADER_H > panel.height) return;
    DrawRectangleRec(panel, C_PANEL);
    DrawRectangleLinesEx(panel, 1.0f, MODAL_PANEL_BORDER);
    draw_tab(panel, CP_TAB_WORLD, "World", "chat", true, tab_unread(CP_TAB_WORLD));
    draw_tab(panel, CP_TAB_PARTY, "Party", "users", in_party(), tab_unread(CP_TAB_PARTY));
    draw_tab(panel, CP_TAB_DIRECT, "Direct", "character", true, tab_unread(CP_TAB_DIRECT));
    Rectangle xr = close_rect(panel);
    UIButtonStyle close_btn = { .icon_id = "close-yellow", .no_fill = true };
    ui_button_draw(xr, &close_btn, ui_button_resolve_state(true, false, ui_button_hit(xr, GetMouseX(), GetMouseY())));
    Rectangle content = content_rect(panel);
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
    Rectangle content = content_rect(panel);
    if (ui_button_hit(close_rect(panel), mx, my)) {
        chat_panel_toggle();
        return true;
    }
    for (int tab = 0; CP_TAB_COUNT > tab; tab++) {
        if (!ui_button_hit(tab_rect(panel, (ChatTab)tab), mx, my)) continue;
        if (CP_TAB_PARTY != tab || in_party()) {
            if (CP_TAB_DIRECT == tab && CP_TAB_DIRECT == s_tab) s_peer = 0;
            s_tab = (ChatTab)tab;
        }
        return true;
    }
    if (CP_TAB_DIRECT == s_tab && 0 == s_peer) {
        ui_scroll_on_press(&s_list_scroll, mx, my);
        return true;
    }
    if (CP_TAB_DIRECT == s_tab && ui_button_hit(back_rect(content), mx, my)) {
        s_peer = 0;
        ui_scroll_reset(&s_list_scroll);
        return true;
    }
    chat_pane_press(&s_pane, CP_TAB_DIRECT == s_tab ? pane_rect(content) : content, mx, my);
    return true;
}

bool chat_panel_handle_wheel(float wheel_delta) {
    if (!chat_panel_covers_point(GetMouseX(), GetMouseY())) return false;
    Rectangle content = content_rect(panel_rect());
    if (CP_TAB_DIRECT == s_tab && 0 == s_peer)
        return ui_scroll_on_wheel(&s_list_scroll, content, s_list_h, wheel_delta);
    chat_pane_wheel(&s_pane, CP_TAB_DIRECT == s_tab ? pane_rect(content) : content, wheel_delta);
    return true;
}
