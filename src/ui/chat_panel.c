#include "chat_panel.h"

#include "chat_log.h"
#include "chat_pane.h"
#include "hud_side_stack.h"
#include "modal.h"
#include "modal_instance_map.h"
#include "sim/local_player.h"
#include "text.h"
#include "ui_button.h"
#include "view/viewport.h"

#include <raylib.h>
#include <stdio.h>

#define CP_HEADER_H 34.0f
#define CP_PAD       6.0f
#define CP_TAB_W    92.0f

static const Color C_PANEL  = {   3,   7,  16, 170 };
static const Color C_TAB    = {  24,  30,  48, 255 };
static const Color C_TAB_ON = {  70, 110, 175, 230 };
static const Color C_BADGE  = { 210,  60,  60, 245 };

static bool           s_visible = false;
static chat_channel_t s_channel = CHAT_CHANNEL_WORLD;
static ChatPane       s_pane;

static bool in_party(void) { return 0 < g_local_player.party.member_count; }

void chat_panel_toggle(void) {
    s_visible = !s_visible;
    if (!s_visible) chat_pane_hide(&s_pane);
}

bool chat_panel_is_visible(void) { return s_visible; }

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

static Rectangle tab_rect(Rectangle panel, chat_channel_t channel) {
    float h = CP_HEADER_H - 2.0f * CP_PAD;
    return (Rectangle){ panel.x + CP_PAD + (float)channel * (CP_TAB_W + CP_PAD), panel.y + CP_PAD, CP_TAB_W, h };
}

static Rectangle content_rect(Rectangle panel) {
    float top = panel.y + CP_HEADER_H + CP_PAD;
    return (Rectangle){ panel.x + CP_PAD, top, panel.width - 2.0f * CP_PAD,
                        panel.y + panel.height - CP_PAD - top };
}

int chat_panel_unread(void) {
    return chat_log_unread(chat_channel(CHAT_CHANNEL_WORLD)) + chat_log_unread(chat_channel(CHAT_CHANNEL_PARTY));
}

void chat_panel_update(float dt) {
    /* The party channel ends with the party. */
    if (CHAT_CHANNEL_PARTY == s_channel && !in_party()) s_channel = CHAT_CHANNEL_WORLD;
    if (!s_visible) return;
    chat_pane_bind(&s_pane, chat_channel(s_channel));
    Rectangle panel = panel_rect();
    Rectangle content = content_rect(panel);
    bool input_visible = !modal_any_open() && !modal_instance_map_is_open() && 0.0f < content.height;
    chat_pane_update(&s_pane, content, input_visible, dt);
}

static void draw_tab(Rectangle panel, chat_channel_t channel, const char* label, bool enabled) {
    Rectangle r = tab_rect(panel, channel);
    bool active = channel == s_channel;
    UIButtonPixelRetroStyle style = {
        .bg = active ? C_TAB_ON : C_TAB, .icon_id = CHAT_CHANNEL_PARTY == channel ? "users" : "chat",
        .label = label, .font_size = viewport_is_mobile() ? 12 : 13,
        .text_color = enabled ? MODAL_TEXT : MODAL_LABEL, .selected = active, .enabled = enabled,
    };
    ui_button_pixel_retro_draw(r, &style, enabled && ui_button_hit(r, GetMouseX(), GetMouseY()));
    int unread = active ? 0 : chat_log_unread(chat_channel(channel));
    if (0 >= unread) return;
    char count[8];
    snprintf(count, sizeof count, "%d", 99 < unread ? 99 : unread);
    DrawCircle((int)(r.x + r.width - 4.0f), (int)(r.y + 4.0f), 8.0f, C_BADGE);
    DrawText(count, (int)(r.x + r.width - 4.0f - (float)MeasureText(count, 10) * 0.5f), (int)(r.y - 1.0f), 10, WHITE);
}

void chat_panel_draw(void) {
    if (!s_visible) return;
    Rectangle panel = panel_rect();
    if (CP_HEADER_H > panel.height) return;
    DrawRectangleRec(panel, C_PANEL);
    DrawRectangleLinesEx(panel, 1.0f, MODAL_PANEL_BORDER);
    draw_tab(panel, CHAT_CHANNEL_WORLD, "World", true);
    draw_tab(panel, CHAT_CHANNEL_PARTY, "Party", in_party());
    Rectangle xr = close_rect(panel);
    UIButtonStyle close_btn = { .icon_id = "close-yellow", .no_fill = true };
    ui_button_draw(xr, &close_btn, ui_button_resolve_state(true, false, ui_button_hit(xr, GetMouseX(), GetMouseY())));
    chat_pane_draw(&s_pane, content_rect(panel));
}

bool chat_panel_covers_point(int mx, int my) {
    return s_visible && CheckCollisionPointRec((Vector2){ (float)mx, (float)my }, panel_rect());
}

bool chat_panel_handle_click(int mx, int my) {
    if (!chat_panel_covers_point(mx, my)) return false;
    Rectangle panel = panel_rect();
    if (ui_button_hit(close_rect(panel), mx, my)) {
        chat_panel_toggle();
        return true;
    }
    if (ui_button_hit(tab_rect(panel, CHAT_CHANNEL_WORLD), mx, my)) {
        s_channel = CHAT_CHANNEL_WORLD;
        return true;
    }
    if (ui_button_hit(tab_rect(panel, CHAT_CHANNEL_PARTY), mx, my)) {
        if (in_party()) s_channel = CHAT_CHANNEL_PARTY;
        return true;
    }
    chat_pane_press(&s_pane, content_rect(panel), mx, my);
    return true;
}

bool chat_panel_handle_wheel(float wheel_delta) {
    if (!chat_panel_covers_point(GetMouseX(), GetMouseY())) return false;
    chat_pane_wheel(&s_pane, content_rect(panel_rect()), wheel_delta);
    return true;
}
