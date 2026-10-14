#include "player_panel.h"

#include "content/stat_contract_generated.h"
#include "modal.h"
#include "ol_stack_ico.h"
#include "sim/game_state.h"
#include "sim/local_player.h"
#include "text.h"
#include "ui_button.h"
#include "ui_icon.h"
#include "ui_rect.h"
#include "ui_scroll.h"
#include "utils.h"
#include "view/viewport.h"
#include "world/object_layers_management.h"

#include <raylib.h>
#include <stdio.h>
#include <string.h>

#define PP_BUTTONS_MAX 16
/* Seconds a first Disband tap waits for the second. */
#define PP_DISBAND_ARM_S 3.0

typedef enum { PP_INVITE, PP_ACCEPT, PP_DECLINE, PP_KICK, PP_LEAVE, PP_DISBAND } PlayerAction;

typedef struct {
    Rectangle    rect;
    PlayerAction action;
    EntityID     target;
} PlayerButton;

static struct {
    UIScroll     scroll;
    float        content_h;
    PlayerButton buttons[PP_BUTTONS_MAX];
    int          button_count;
} s_views[PLAYER_VIEW_COUNT];

static double s_disband_armed_at = -1.0;

static const Color C_BTN      = {  24,  30,  48, 255 };
static const Color C_ACCEPT   = {  38, 138,  76, 255 };
static const Color C_DANGER   = { 130,  44,  44, 255 };
static const Color C_DISABLED = {  44,  46,  58, 255 };
static const Color C_ROW      = {  30,  34,  52, 210 };
static const Color C_HP       = { 200,  70,  70, 255 };
static const Color C_BAR_BG   = {  20,  20,  30, 220 };
static const Color C_XP       = { 230, 190,  60, 255 };
static const Color C_NOTE     = { 230, 170, 110, 235 };

static int   font_body(void)  { return viewport_is_mobile() ? 12 : 15; }
static int   font_title(void) { return viewport_is_mobile() ? 16 : 20; }
static float button_h(void)   { return viewport_is_mobile() ? 32.0f : 36.0f; }
static float gap(void)        { return viewport_is_mobile() ? 6.0f : 8.0f; }
static float avatar_size(void){ return viewport_is_mobile() ? 56.0f : 72.0f; }

void player_panel_reset(void) {
    for (int v = 0; PLAYER_VIEW_COUNT > v; v++) {
        ui_scroll_reset(&s_views[v].scroll);
        s_views[v].content_h = 0.0f;
        s_views[v].button_count = 0;
    }
    s_disband_armed_at = -1.0;
}

/* Draw a button and record it as a tap target when it is enabled. */
static void button(PlayerView view, Rectangle r, const char* icon, const char* label, Color bg,
                   bool enabled, PlayerAction action, EntityID target) {
    UIButtonPixelRetroStyle style = {
        .bg = enabled ? bg : C_DISABLED, .icon_id = icon, .label = label,
        .font_size = font_body(), .text_color = enabled ? MODAL_TEXT : MODAL_LABEL, .enabled = enabled,
    };
    ui_button_pixel_retro_draw(r, &style, enabled && ui_button_hit(r, GetMouseX(), GetMouseY()));
    if (enabled && PP_BUTTONS_MAX > s_views[view].button_count) {
        s_views[view].buttons[s_views[view].button_count++] = (PlayerButton){ r, action, target };
    }
}

static void perform(const PlayerButton* b) {
    switch (b->action) {
        case PP_INVITE:  local_player_request_party_invite(b->target); return;
        case PP_ACCEPT:  local_player_request_party_reply(b->target, true); return;
        case PP_DECLINE: local_player_request_party_reply(b->target, false); return;
        case PP_KICK:    local_player_request_party_kick(b->target); return;
        case PP_LEAVE:   local_player_request_party_leave(); return;
        case PP_DISBAND:
            if (0.0 <= s_disband_armed_at && GetTime() - s_disband_armed_at < PP_DISBAND_ARM_S) {
                s_disband_armed_at = -1.0;
                local_player_request_party_disband();
            } else {
                s_disband_armed_at = GetTime();
            }
            return;
    }
}

/* A bar of `fill` 0..1 with a centred label. */
static void bar(Rectangle r, float fill, Color color, const char* label) {
    DrawRectangleRec(r, C_BAR_BG);
    DrawRectangleRec((Rectangle){ r.x, r.y, r.width * (0.0f > fill ? 0.0f : 1.0f < fill ? 1.0f : fill), r.height },
                     color);
    DrawRectangleLinesEx(r, 1.0f, BLACK);
    if (NULL == label) return;
    int font = font_body() - 2;
    text_draw_outlined(label, (int)(r.x + (r.width - (float)MeasureText(label, font)) * 0.5f),
                       (int)(r.y + (r.height - (float)font) * 0.5f), font, MODAL_TEXT, BLACK, 1);
}

/* The entity as it looks in the world, in a bevelled frame. */
static void avatar(Rectangle r, EntityID id) {
    Rectangle inner = draw_pixel_bevel(r, 0.0f, 3.0f, C_ROW, ColorBrightness(C_ROW, 0.45f),
                                       ColorBrightness(C_ROW, -0.45f));
    if (!game_state_has(id, COMP_APPEARANCE)) return;
    const Appearance* ap = game_state_appearance(id);
    Rectangle art = ui_rect_inset(inner, 2.0f);
    ol_stack_ico_draw(obj_layers_mgr_get(), OBJ_LAYERS(ap), ap->layer_count,
                      (int)art.x, (int)art.y, (int)art.width, "down_idle", 0, WHITE);
}

/* The avatar with the name and the level line beside it; returns its height. */
static float identity(Rectangle c, float y, EntityID id, const char* level_line) {
    float size = avatar_size();
    avatar((Rectangle){ c.x, y, size, size }, id);
    float tx = c.x + size + gap() * 1.5f;
    const char* name = game_state_has(id, COMP_PLAYER) ? game_state_player(id)->name : "";
    text_draw_outlined(name, (int)tx, (int)(y + 4.0f), font_title(), MODAL_TEXT, BLACK, 1);
    DrawText(level_line, (int)tx, (int)(y + 8.0f + (float)font_title()), font_body(), GOLD);
    return size + gap();
}

/* One labelled value with an icon, and a note under the value. */
static float info_row(Rectangle c, float y, const char* icon, const char* label, const char* value,
                      const char* note) {
    int font = font_body();
    float h = (float)font * 2.0f + (NULL != note ? (float)text_line_height(font - 1) : 0.0f);
    DrawRectangleRec((Rectangle){ c.x, y, c.width, h }, C_ROW);
    float icon_sz = (float)font * 1.5f;
    ui_icon_draw(icon, c.x + gap() + icon_sz * 0.5f, y + (float)font, (int)icon_sz, false, 0.0f);
    float tx = c.x + gap() * 2.0f + icon_sz;
    DrawText(label, (int)tx, (int)(y + (float)font * 0.5f), font, MODAL_LABEL);
    DrawText(value, (int)(c.x + c.width - gap() - (float)MeasureText(value, font)), (int)(y + (float)font * 0.5f),
             font, MODAL_TEXT);
    if (NULL != note) DrawText(note, (int)tx, (int)(y + (float)font * 1.6f), font - 1, C_NOTE);
    return h + gap() * 0.5f;
}

/* "Next slot at level N", or why no more slots open. */
static void next_slot_note(CyberiaCapacityRule rule, char* out, size_t cap) {
    int open = local_player_open_slots(rule);
    if (open >= rule.max) {
        copy_str(out, cap, "Every slot is open.");
        return;
    }
    int level = local_player_unlock_level(rule, open);
    if (0 < level) snprintf(out, cap, "Next slot at level %d.", level);
    else           copy_str(out, cap, "No more slots open in this world.");
}

static float draw_own_profile(Rectangle c, float y, EntityID id) {
    int level = game_state_has(id, COMP_PROGRESS) ? game_state_progress(id)->level : 1;
    char line[96];
    snprintf(line, sizeof(line), "Level %d", level);
    float top = y;
    y += identity(c, y, id, line);

    char xp[48];
    snprintf(xp, sizeof(xp), "XP %.0f / %.0f", g_local_player.xp, g_local_player.next_level_xp);
    bar((Rectangle){ c.x, y, c.width, (float)font_body() + 6.0f }, local_player_xp_ratio(), C_XP, xp);
    y += (float)font_body() + 6.0f + gap();

    char value[48], note[64];
    snprintf(value, sizeof(value), "%d", local_player_coins());
    y += info_row(c, y, "wallet", "Coins", value, NULL);

    snprintf(value, sizeof(value), "%d / %d", local_player_inventory_used(),
             local_player_open_slots(CYBERIA_INVENTORY_CAPACITY));
    next_slot_note(CYBERIA_INVENTORY_CAPACITY, note, sizeof(note));
    y += info_row(c, y, "bag", "Bag slots", value, note);

    snprintf(value, sizeof(value), "%d / %d", g_local_player.storage.count,
             local_player_open_slots(CYBERIA_STORAGE_CAPACITY));
    next_slot_note(CYBERIA_STORAGE_CAPACITY, note, sizeof(note));
    y += info_row(c, y, "grid", "Storage slots", value, note);

    const Party* party = &g_local_player.party;
    if (0 < party->member_count) {
        snprintf(value, sizeof(value), "%s %d / %d", local_player_leads_party() ? "Leader" : "Member",
                 party->member_count, CYBERIA_PARTY_MAX_MEMBERS);
    } else {
        copy_str(value, sizeof(value), "None");
    }
    y += info_row(c, y, "users", "Party", value, NULL);

    if (game_state_has(id, COMP_PROGRESS)) {
        snprintf(value, sizeof(value), "%+d", game_state_progress(id)->stats_sum);
        y += info_row(c, y, "stats", "Stats sum", value, NULL);
    }
    y += text_wrap("Guest session: your bag, storage and party end when you leave.",
                   (int)c.x, (int)y, (int)c.width, font_body() - 1, MODAL_LABEL, false, true);
    return y - top;
}

/* Why the local player cannot invite now, when the server offers no invite. */
static const char* invite_refusal(void) {
    if (0 < g_local_player.party.member_count && !local_player_leads_party()) return "Only the leader invites";
    if (CYBERIA_PARTY_MAX_MEMBERS <= g_local_player.party.member_count) return "Party is full";
    return "In another party";
}

static float draw_other_profile(Rectangle c, float y, EntityID id) {
    float top = y;
    if (!game_state_has(id, COMP_PLAYER | COMP_PROGRESS)) {
        y += text_wrap("Out of sight. A profile shows while the player is in view.",
                       (int)c.x, (int)y, (int)c.width, font_body(), MODAL_LABEL, false, true);
        return y - top;
    }
    const Progress* pr = game_state_progress(id);
    char line[64];
    snprintf(line, sizeof(line), "Level %d   Stats %+d", pr->level, pr->stats_sum);
    y += identity(c, y, id, line);

    const PartyMember* member = local_player_party_member(id);
    const PartyInvite* invite = local_player_invite_from(id);
    uint8_t flags = local_player_interaction_flags(id);
    const char* relation = NULL;
    if (NULL != member) relation = id == g_local_player.party.leader_id ? "Leads your party" : "In your party";
    else if (NULL != invite) relation = "Invited you to a party";
    else if (0 != (flags & INTERACTION_FLAG_PARTY_INVITED)) relation = "Your invite waits for an answer";
    if (NULL != relation) {
        DrawText(relation, (int)c.x, (int)y, font_body(), MODAL_POSITIVE);
        y += (float)text_line_height(font_body()) + gap();
    }

    float bw = (c.width - gap()) * 0.5f;
    Rectangle left = { c.x, y, bw, button_h() };
    Rectangle right = { c.x + bw + gap(), y, bw, button_h() };
    Rectangle full = { c.x, y, c.width, button_h() };
    if (NULL != invite) {
        button(PLAYER_VIEW_PROFILE, left, "close", "Decline", C_BTN, true, PP_DECLINE, id);
        button(PLAYER_VIEW_PROFILE, right, "check", "Accept", C_ACCEPT, true, PP_ACCEPT, id);
    } else if (NULL != member) {
        if (local_player_leads_party())
            button(PLAYER_VIEW_PROFILE, full, "close", "Remove from party", C_DANGER, true, PP_KICK, id);
        else
            y -= button_h() + gap();
    } else if (0 != (flags & INTERACTION_FLAG_PARTY_INVITED)) {
        button(PLAYER_VIEW_PROFILE, full, "clock", "Invite sent", C_BTN, false, PP_INVITE, id);
    } else if (0 != (flags & INTERACTION_FLAG_PARTY_INVITE)) {
        button(PLAYER_VIEW_PROFILE, full, "users", "Invite to party", C_ACCEPT, true, PP_INVITE, id);
    } else {
        button(PLAYER_VIEW_PROFILE, full, "lock", invite_refusal(), C_BTN, false, PP_INVITE, id);
    }
    y += button_h() + gap();
    return y - top;
}

static float draw_party(Rectangle c, float y) {
    const Party* party = &g_local_player.party;
    float top = y;
    int font = font_body();
    char line[96];

    if (0 < g_local_player.invite_count) {
        DrawText("Invites", (int)c.x, (int)y, font, MODAL_LABEL);
        y += (float)text_line_height(font) + gap() * 0.5f;
        for (int i = 0; i < g_local_player.invite_count; i++) {
            const PartyInvite* in = &g_local_player.invites[i];
            int left_s = (int)(in->expires_at - GetTime());
            snprintf(line, sizeof(line), "%s  (%ds)", in->from_name, 0 < left_s ? left_s : 0);
            DrawText(line, (int)c.x, (int)(y + (button_h() - (float)font) * 0.5f), font, MODAL_TEXT);
            float bw = c.width * 0.26f;
            Rectangle accept = { c.x + c.width - bw, y, bw, button_h() };
            Rectangle decline = { accept.x - gap() - bw, y, bw, button_h() };
            button(PLAYER_VIEW_PARTY, decline, "close", "Decline", C_BTN, true, PP_DECLINE, in->from_id);
            button(PLAYER_VIEW_PARTY, accept, "check", "Accept", C_ACCEPT, true, PP_ACCEPT, in->from_id);
            y += button_h() + gap();
        }
        y += gap();
    }

    if (0 == party->member_count) {
        text_draw_outlined("No party", (int)c.x, (int)y, font_title(), MODAL_TEXT, BLACK, 1);
        y += (float)font_title() + gap();
        y += text_wrap("Tap a player's bubble to open their profile, then invite them.",
                       (int)c.x, (int)y, (int)c.width, font, MODAL_LABEL, false, true);
        return y - top;
    }

    snprintf(line, sizeof(line), "Party %d / %d", party->member_count, CYBERIA_PARTY_MAX_MEMBERS);
    text_draw_outlined(line, (int)c.x, (int)y, font_title(), MODAL_TEXT, BLACK, 1);
    y += (float)font_title() + gap();

    bool leads = local_player_leads_party();
    for (int i = 0; i < party->member_count; i++) {
        const PartyMember* m = &party->members[i];
        bool own = m->id == g_local_player.id;
        float h = (float)font * 2.0f + gap() * 2.0f;
        DrawRectangleRec((Rectangle){ c.x, y, c.width, h }, C_ROW);
        float x = c.x + gap();
        if (m->id == party->leader_id) {
            ui_icon_draw("star", x + (float)font * 0.6f, y + gap() + (float)font * 0.5f, font + 2, false, 0.0f);
            x += (float)font * 1.4f;
        }
        snprintf(line, sizeof(line), "%s%s  Lv %d", m->name, own ? " (You)" : "", m->level);
        DrawText(line, (int)x, (int)(y + gap()), font, MODAL_TEXT);
        float kick_w = leads && !own ? c.width * 0.28f : 0.0f;
        float bar_w = c.width - (x - c.x) - kick_w - gap() * 2.0f;
        char hp[32];
        snprintf(hp, sizeof(hp), "%d / %d  %s", m->life, m->max_life, m->map_code);
        bar((Rectangle){ x, y + gap() + (float)font + 2.0f, bar_w, (float)font }, 0 < m->max_life ?
            (float)m->life / (float)m->max_life : 0.0f, C_HP, hp);
        if (0.0f < kick_w) {
            Rectangle kick = { c.x + c.width - kick_w - gap(), y + (h - button_h()) * 0.5f, kick_w, button_h() };
            button(PLAYER_VIEW_PARTY, kick, "close", "Remove", C_DANGER, true, PP_KICK, m->id);
        }
        y += h + gap() * 0.5f;
    }
    y += gap();

    float bw = leads ? (c.width - gap()) * 0.5f : c.width;
    button(PLAYER_VIEW_PARTY, (Rectangle){ c.x, y, bw, button_h() }, "log-out", "Leave party", C_BTN, true,
           PP_LEAVE, 0);
    if (leads) {
        bool armed = 0.0 <= s_disband_armed_at && GetTime() - s_disband_armed_at < PP_DISBAND_ARM_S;
        button(PLAYER_VIEW_PARTY, (Rectangle){ c.x + bw + gap(), y, bw, button_h() }, "close",
               armed ? "Tap to confirm" : "Disband", C_DANGER, true, PP_DISBAND, 0);
    }
    y += button_h() + gap();
    return y - top;
}

void player_panel_update(PlayerView view, Rectangle content, bool ready, float dt) {
    ui_scroll_update(&s_views[view].scroll, content, s_views[view].content_h, dt);
    int x, y;
    if (!ui_scroll_take_click(&s_views[view].scroll, &x, &y) || !ready) return;
    for (int i = 0; i < s_views[view].button_count; i++) {
        if (ui_button_hit(s_views[view].buttons[i].rect, x, y)) {
            perform(&s_views[view].buttons[i]);
            return;
        }
    }
}

float player_panel_draw(PlayerView view, Rectangle content, EntityID player_id) {
    s_views[view].button_count = 0;
    float top = content.y - ui_scroll_offset(&s_views[view].scroll);
    ui_scroll_begin(&s_views[view].scroll);
    float h = PLAYER_VIEW_PARTY == view       ? draw_party(content, top)
            : player_id == g_local_player.id ? draw_own_profile(content, top, player_id)
                                              : draw_other_profile(content, top, player_id);
    ui_scroll_end(&s_views[view].scroll);
    s_views[view].content_h = h;
    return h;
}

void player_panel_press(PlayerView view, Rectangle content, int mx, int my) {
    if (CheckCollisionPointRec((Vector2){ (float)mx, (float)my }, content)) {
        ui_scroll_on_press(&s_views[view].scroll, mx, my);
    }
}

bool player_panel_wheel(PlayerView view, Rectangle content, float wheel_delta) {
    return ui_scroll_on_wheel(&s_views[view].scroll, content, s_views[view].content_h, wheel_delta);
}
