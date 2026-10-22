#include "player_panel.h"

#include "chat_panel.h"
#include "content/stat_contract_generated.h"
#include "identity_icon.h"
#include "js/text_input_bridge.h"
#include "modal.h"
#include "net/client_event.h"
#include "ol_as_animated_ico.h"
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

#include <math.h>
#include <raylib.h>
#include <stdio.h>
#include <string.h>

#define PP_BUTTONS_MAX 64
/* Seconds a first Disband or Remove tap waits for the second. */
#define PP_CONFIRM_ARM_S 3.0
/* Seconds before the Friends view asks the server for a fresh list again. */
#define PP_FRIENDS_REFRESH_S 10.0

typedef enum {
    PP_INVITE, PP_ACCEPT, PP_DECLINE, PP_KICK, PP_LEAVE, PP_DISBAND,
    PP_FRIEND_ADD, PP_FRIEND_ACCEPT, PP_FRIEND_DECLINE, PP_FRIEND_REMOVE, PP_FRIEND_CHAT, PP_FRIEND_SELECT,
    PP_NOTE_EDIT, PP_NOTE_SAVE, PP_NOTE_CLEAR, PP_NOTE_CANCEL,
} PlayerAction;

typedef struct {
    Rectangle    rect;
    PlayerAction action;
    PlayerView   view;
    EntityID     target;
    char         friendship[RECORD_ID_BYTES];  /* friend actions */
    char         name[PLAYER_NAME_BYTES];      /* friend chat */
} PlayerButton;

static struct {
    UIScroll     scroll;
    float        content_h;
    PlayerButton buttons[PP_BUTTONS_MAX];
    int          button_count;
} s_views[PLAYER_VIEW_COUNT];

static double s_disband_armed_at = -1.0;
/* The friendship a first Remove tap armed, and when. */
static char   s_remove_armed[RECORD_ID_BYTES];
static double s_remove_armed_at = -1.0;
static double s_friends_asked_at = -PP_FRIENDS_REFRESH_S;
/* The friend row a tap opened: its actions show under it. */
static char   s_selected[RECORD_ID_BYTES];
static bool s_note_editing;
static bool s_note_pending;
static Rectangle s_note_input;
static char s_note_error[64];

static const Color C_ACCEPT   = {  38, 138,  76, 255 };
static const Color C_DANGER   = { 130,  44,  44, 255 };
static const Color C_DISABLED = {  44,  46,  58, 255 };
static const Color C_ROW      = {  30,  34,  52, 210 };
static const Color C_HP       = { 200,  70,  70, 255 };
static const Color C_BAR_BG   = {  20,  20,  30, 220 };
static const Color C_XP       = { 230, 190,  60, 255 };
static const Color C_NOTE     = { 230, 170, 110, 235 };
static const Color C_ONLINE   = {  90, 200, 120, 255 };
static const Color C_OFFLINE  = { 110, 116, 134, 255 };
static const Color C_SELECTED = { 255, 210,  90, 255 };

static int   font_body(void)  { return viewport_is_mobile() ? 12 : 15; }
static int   font_title(void) { return viewport_is_mobile() ? 16 : 20; }
static float button_h(void)   { return viewport_is_mobile() ? 32.0f : 36.0f; }
static float gap(void)        { return viewport_is_mobile() ? 6.0f : 8.0f; }
static float portrait_size(void){ return viewport_is_mobile() ? 56.0f : 72.0f; }
static float face_size(void)    { return viewport_is_mobile() ? 34.0f : 40.0f; }

void player_panel_reset(void) {
    player_panel_hide_input();
    for (int v = 0; PLAYER_VIEW_COUNT > v; v++) {
        ui_scroll_reset(&s_views[v].scroll);
        s_views[v].content_h = 0.0f;
        s_views[v].button_count = 0;
    }
    s_disband_armed_at = -1.0;
    s_remove_armed_at = -1.0;
    s_note_pending = false;
    s_selected[0] = '\0';
}

void player_panel_hide_input(void) {
    if (!s_note_editing) return;
    text_input_bridge_hide();
    text_input_bridge_set("");
    s_note_editing = false;
    s_note_error[0] = '\0';
}

void player_panel_note_result(bool accepted) {
    if (!s_note_pending) return;
    s_note_pending = false;
    if (accepted) player_panel_hide_input();
}

static void send_note(bool clear) {
    if (s_note_pending || GetTime() < g_local_player.note_ready_at) return;
    client_event_payload_t payload = {0};
    if (!clear) text_input_bridge_read(payload.text, sizeof payload.text);
    if (CYBERIA_STATUS_NOTE_MAX_RUNES < GetCodepointCount(payload.text)) {
        copy_str(s_note_error, sizeof s_note_error, "Use 80 Unicode code points or fewer.");
        return;
    }
    s_note_error[0] = '\0';
    s_note_pending = true;
    client_event_push(CLIENT_EVENT_STATUS_NOTE, payload);
}

static bool armed(double at) { return 0.0 <= at && GetTime() - at < PP_CONFIRM_ARM_S; }

/* Record a tap target while it shows in the view. The first target a tap
 * hits takes it. */
static PlayerButton* tap_target(PlayerView view, Rectangle r, PlayerAction action, EntityID target) {
    if (!CheckCollisionRecs(r, s_views[view].scroll.view) || PP_BUTTONS_MAX <= s_views[view].button_count) return NULL;
    PlayerButton* b = &s_views[view].buttons[s_views[view].button_count++];
    *b = (PlayerButton){ .rect = r, .action = action, .target = target, .view = view };
    return b;
}

/* Draw a button and record it as a tap target when it is enabled. */
static PlayerButton* button(PlayerView view, Rectangle r, const char* icon, const char* label, Color bg,
                            bool enabled, PlayerAction action, EntityID target) {
    UIButtonPixelRetroStyle style = {
        .bg = enabled ? bg : C_DISABLED, .icon_id = icon, .label = label,
        .font_size = font_body(), .text_color = enabled ? MODAL_TEXT : MODAL_LABEL, .enabled = enabled,
    };
    ui_button_pixel_retro_draw(r, &style, enabled && ui_button_hit(r, GetMouseX(), GetMouseY()));
    return enabled ? tap_target(view, r, action, target) : NULL;
}

static void bind_friend(PlayerButton* b, const Friend* f) {
    if (NULL == b) return;
    copy_str(b->friendship, sizeof(b->friendship), f->id);
    copy_str(b->name, sizeof(b->name), f->name);
}

/* A button that acts on one friendship. */
static void friend_button(PlayerView view, Rectangle r, const char* icon, const char* label, Color bg,
                          PlayerAction action, const Friend* f) {
    bind_friend(button(view, r, icon, label, bg, true, action, f->entity_id), f);
}

static void perform(const PlayerButton* b) {
    switch (b->action) {
        case PP_NOTE_EDIT:
            s_note_editing = true;
            text_input_bridge_set(g_local_player.status_note);
            return;
        case PP_NOTE_SAVE: send_note(false); return;
        case PP_NOTE_CLEAR: send_note(true); return;
        case PP_NOTE_CANCEL: player_panel_hide_input(); return;
        case PP_INVITE:  local_player_request_party_invite(b->target); return;
        case PP_ACCEPT:  local_player_request_party_reply(b->target, true); return;
        case PP_DECLINE: local_player_request_party_reply(b->target, false); return;
        case PP_KICK:    local_player_request_party_kick(b->target); return;
        case PP_LEAVE:   local_player_request_party_leave(); return;
        case PP_DISBAND:
            if (armed(s_disband_armed_at)) {
                s_disband_armed_at = -1.0;
                local_player_request_party_disband();
            } else {
                s_disband_armed_at = GetTime();
            }
            return;
        case PP_FRIEND_ADD:     local_player_request_friend(b->target); return;
        case PP_FRIEND_ACCEPT:  local_player_request_friend_reply(b->friendship, true); return;
        case PP_FRIEND_DECLINE: local_player_request_friend_reply(b->friendship, false); return;
        case PP_FRIEND_CHAT:
            chat_panel_open_direct(b->target, b->name);
            return;
        case PP_FRIEND_SELECT:
            copy_str(s_selected, sizeof s_selected, 0 == strcmp(s_selected, b->friendship) ? "" : b->friendship);
            return;
        case PP_FRIEND_REMOVE:
            if (armed(s_remove_armed_at) && 0 == strcmp(s_remove_armed, b->friendship)) {
                s_remove_armed_at = -1.0;
                local_player_request_friend_remove(b->friendship);
            } else {
                copy_str(s_remove_armed, sizeof(s_remove_armed), b->friendship);
                s_remove_armed_at = GetTime();
            }
            return;
    }
}

/* The own note: what friends of the account read under its name. */
static float status_note(Rectangle c, float y) {
    float top = y;
    int font = font_body();
    bool ready = !s_note_pending && GetTime() >= g_local_player.note_ready_at;
    float edit_w = 86.0f;
    DrawText("Your note", (int)c.x, (int)(y + (button_h() - (float)font) * 0.5f), font, MODAL_LABEL);
    if (!s_note_editing)
        button(PLAYER_VIEW_FRIENDS, (Rectangle){ c.x + c.width - edit_w, y, edit_w, button_h() }, "chat", "Edit",
               MODAL_BUTTON, ready, PP_NOTE_EDIT, 0);
    y += button_h() + gap() * 0.5f;
    bool empty = '\0' == g_local_player.status_note[0];
    y += (float)text_wrap(empty ? "No note yet." : g_local_player.status_note, (int)c.x, (int)y, (int)c.width, font,
                          empty ? MODAL_LABEL : MODAL_TEXT, false, true) + gap();
    if (s_note_editing) {
        s_note_input = (Rectangle){ c.x, y, c.width, button_h() };
        y += button_h() + gap();
        float width = (c.width - 2.0f * gap()) / 3.0f;
        button(PLAYER_VIEW_FRIENDS, (Rectangle){ c.x, y, width, button_h() }, "check", "Save", C_ACCEPT,
               ready, PP_NOTE_SAVE, 0);
        button(PLAYER_VIEW_FRIENDS, (Rectangle){ c.x + width + gap(), y, width, button_h() }, "close", "Clear",
               MODAL_BUTTON, ready, PP_NOTE_CLEAR, 0);
        button(PLAYER_VIEW_FRIENDS, (Rectangle){ c.x + 2.0f * (width + gap()), y, width, button_h() }, "close",
               "Cancel", MODAL_BUTTON, true, PP_NOTE_CANCEL, 0);
        y += button_h() + gap();
    }
    if (!s_note_editing && ready && '\0' == s_note_error[0]) return y - top;
    char hint[80];
    if (s_note_pending) copy_str(hint, sizeof hint, "Saving...");
    else if (!ready) snprintf(hint, sizeof hint, "You can change it in %.0f seconds.",
                              g_local_player.note_ready_at - GetTime() + 1.0);
    else copy_str(hint, sizeof hint, '\0' != s_note_error[0] ? s_note_error : "80 Unicode code points. Empty clears the note.");
    y += (float)text_wrap(hint, (int)c.x, (int)y, (int)c.width, font - 1, MODAL_LABEL, false, true) + gap();
    return y - top;
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
static void portrait(Rectangle r, EntityID id) {
    Rectangle inner = draw_pixel_bevel(r, 0.0f, 3.0f, C_ROW, ColorBrightness(C_ROW, 0.45f),
                                       ColorBrightness(C_ROW, -0.45f));
    if (!game_state_has(id, COMP_APPEARANCE)) return;
    const Appearance* ap = game_state_appearance(id);
    Rectangle art = ui_rect_inset(inner, 2.0f);
    ol_stack_ico_draw(obj_layers_mgr_get(), OBJ_LAYERS(ap), ap->layer_count,
                      (int)art.x, (int)art.y, (int)art.width, "down_idle", 0, WHITE);
}

/* The in-world look with the name and the level line beside it; returns its
 * height. */
static float identity(Rectangle c, float y, EntityID id, const char* level_line) {
    float size = portrait_size();
    portrait((Rectangle){ c.x, y, size, size }, id);
    float tx = c.x + size + gap() * 1.5f;
    const Player* player = game_state_has(id, COMP_PLAYER) ? game_state_player(id) : NULL;
    text_draw_outlined(NULL != player ? player->name : "", (int)tx, (int)(y + 4.0f), font_title(), MODAL_TEXT,
                       BLACK, 1);
    DrawText(level_line, (int)(c.x + size + gap() * 1.5f), (int)(y + 8.0f + (float)font_title()), font_body(), GOLD);
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

static const char* held_reason_text(HeldReason reason) {
    switch (reason) {
        case HELD_REBOUND: return "A content update changed it here.";
        case HELD_NO_ROOM: return "No free slot. It returns on a visit with room.";
        case HELD_UNSUPPORTED: return "Its skill does not work in this world.";
        case HELD_INCOMPATIBLE: return "This world is updating. It returns after the update.";
        case HELD_UNKNOWN: break;
    }
    return "From another world. It works there, not here.";
}

/* One held stack: its own picture, its label and count, and why it waits. */
static float held_row(Rectangle c, float y, const HeldStack* stack) {
    int font = font_body();
    float h = (float)font * 2.0f + (float)text_line_height(font - 1);
    DrawRectangleRec((Rectangle){ c.x, y, c.width, h }, C_ROW);
    float icon_sz = h - gap();
    ol_as_ico_draw(obj_layers_mgr_get(), stack->cid, (int)(c.x + gap() * 0.5f), (int)(y + gap() * 0.5f),
                   (int)icon_sz, OL_ICO_DEFAULT_DIR, 0, (Color){ 200, 200, 210, 200 });
    float tx = c.x + gap() * 1.5f + icon_sz;
    char label[MAX_ITEM_ID_LENGTH + 16];
    snprintf(label, sizeof(label), "%s x%d", stack->item_id, stack->quantity);
    DrawText(label, (int)tx, (int)(y + (float)font * 0.5f), font, MODAL_TEXT);
    DrawText(held_reason_text(stack->reason), (int)tx, (int)(y + (float)font * 1.6f), font - 1, C_NOTE);
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
    y += info_row(c, y, "party", "Party", value, NULL);

    if (game_state_has(id, COMP_PROGRESS)) {
        snprintf(value, sizeof(value), "%+d", game_state_progress(id)->stats_sum);
        y += info_row(c, y, "stats", "Stats sum", value, NULL);
    }
    if (0 < g_local_player.held_count) {
        snprintf(value, sizeof(value), "%d", g_local_player.held_count);
        y += info_row(c, y, "lock", "Held items", value, "They stay safe in your account. They cannot be used here.");
        int shown = g_local_player.held_count < HELD_STACKS_SHOWN ? g_local_player.held_count : HELD_STACKS_SHOWN;
        for (int i = 0; i < shown; i++) y += held_row(c, y, &g_local_player.held[i]);
        if (g_local_player.held_count > shown) {
            snprintf(value, sizeof(value), "And %d more.", g_local_player.held_count - shown);
            y += text_wrap(value, (int)c.x, (int)y, (int)c.width, font_body() - 1, C_NOTE, false, true);
        }
    }
    const char* saving = DURABILITY_RECOVERY == g_local_player.durability
                             ? "Your latest changes are not saved yet. The world keeps them and retries; trades wait. A world restart before they save would undo only those changes."
                         : DURABILITY_DEGRADED == g_local_player.durability
                             ? "Saving is delayed. The world keeps your latest changes and retries; trades wait until they save."
                             : NULL;
    if (saving) y += text_wrap(saving, (int)c.x, (int)y, (int)c.width, font_body() - 1, C_NOTE, false, true);
    y += text_wrap(g_local_player.account ? "Your bag, storage, vaults, coins, XP and quests are saved to your account."
                                          : "Guest session: your bag, storage and party end when you leave.",
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
    const SentInvite* sent = local_player_invite_to(id);
    const Friend* friendship = local_player_friend_of(id);
    if (NULL != friendship && FRIEND_ACCEPTED == friendship->status && '\0' != friendship->note[0]) {
        y += (float)text_wrap(friendship->note, (int)c.x, (int)y, (int)c.width, font_body(), MODAL_TEXT,
                              false, true) + gap();
    }
    uint8_t flags = local_player_interaction_flags(id);
    const char* relation = NULL;
    if (NULL != member) relation = id == g_local_player.party.leader_id ? "Leads your party" : "In your party";
    else if (NULL != invite) relation = "Invited you to a party";
    else if (NULL != sent) relation = "Your invite waits for an answer";
    else if (0 != (flags & INTERACTION_FLAG_GROUPED)) relation = "In a party";
    if (NULL != relation) {
        DrawText(relation, (int)c.x, (int)y, font_body(), MODAL_POSITIVE);
        y += (float)text_line_height(font_body()) + gap();
    }
    if (0 != (flags & INTERACTION_FLAG_FRIEND)) {
        DrawText("Your friend", (int)c.x, (int)y, font_body(), C_ONLINE);
        y += (float)text_line_height(font_body()) + gap();
    }

    float bw = (c.width - gap()) * 0.5f;
    Rectangle left = { c.x, y, bw, button_h() };
    Rectangle right = { c.x + bw + gap(), y, bw, button_h() };
    Rectangle full = { c.x, y, c.width, button_h() };
    if (NULL != invite) {
        button(PLAYER_VIEW_PROFILE, left, "close", "Decline", MODAL_BUTTON, true, PP_DECLINE, id);
        button(PLAYER_VIEW_PROFILE, right, "check", "Accept", C_ACCEPT, true, PP_ACCEPT, id);
    } else if (NULL != member) {
        if (local_player_leads_party())
            button(PLAYER_VIEW_PROFILE, full, "close", "Remove from party", C_DANGER, true, PP_KICK, id);
        else
            y -= button_h() + gap();
    } else if (NULL != sent) {
        button(PLAYER_VIEW_PROFILE, full, "clock", "Invite sent", MODAL_BUTTON, false, PP_INVITE, id);
    } else if (0 != (flags & INTERACTION_FLAG_PARTY_INVITE)) {
        button(PLAYER_VIEW_PROFILE, full, "party", "Invite to party", C_ACCEPT, true, PP_INVITE, id);
    } else {
        button(PLAYER_VIEW_PROFILE, full, "lock", invite_refusal(), MODAL_BUTTON, false, PP_INVITE, id);
    }
    y += button_h() + gap();

    /* A friendship needs an account on both sides; a request waits in the list. */
    if (g_local_player.account && 0 == (flags & INTERACTION_FLAG_FRIEND)) {
        bool pending = NULL != friendship;
        if (pending && FRIEND_INCOMING == friendship->status) {
            friend_button(PLAYER_VIEW_PROFILE, (Rectangle){ c.x, y, bw, button_h() }, "close", "Decline", MODAL_BUTTON,
                          PP_FRIEND_DECLINE, friendship);
            friend_button(PLAYER_VIEW_PROFILE, (Rectangle){ c.x + bw + gap(), y, bw, button_h() }, "check",
                          "Accept friend", C_ACCEPT, PP_FRIEND_ACCEPT, friendship);
        } else {
            button(PLAYER_VIEW_PROFILE, (Rectangle){ c.x, y, c.width, button_h() }, pending ? "clock" : "friends",
                   pending ? "Friend request sent" : "Add friend", MODAL_BUTTON, !pending, PP_FRIEND_ADD, id);
        }
        y += button_h() + gap();
    }
    return y - top;
}

/* A label at the left and a count at the right end; returns its height. */
static float section(Rectangle c, float y, const char* label, const char* count) {
    int font = font_body();
    DrawText(label, (int)c.x, (int)y, font, MODAL_LABEL);
    if (NULL != count)
        DrawText(count, (int)(c.x + c.width - (float)MeasureText(count, font)), (int)y, font, MODAL_LABEL);
    return (float)text_line_height(font) + gap() * 0.5f;
}

static float message(Rectangle c, float y, const char* text) {
    return (float)text_wrap(text, (int)c.x, (int)y, (int)c.width, font_body(), MODAL_LABEL, false, true) + gap();
}

static float draw_party(Rectangle c, float y) {
    const Party* party = &g_local_player.party;
    float top = y;
    int font = font_body();
    char line[96];

    if (0 < g_local_player.invite_count) {
        snprintf(line, sizeof line, "%d", g_local_player.invite_count);
        y += section(c, y, "Invites", line);
        float bw = (c.width - gap()) * 0.5f;
        for (int i = 0; i < g_local_player.invite_count; i++) {
            const PartyInvite* in = &g_local_player.invites[i];
            int left_s = (int)(in->expires_at - GetTime());
            Rectangle row = { c.x, y, c.width, (float)text_line_height(font) + button_h() + gap() * 2.0f };
            DrawRectangleRec(row, C_ROW);
            snprintf(line, sizeof(line), "%s invites you (%ds)", in->from_name, 0 < left_s ? left_s : 0);
            modal_draw_clipped_text(line, (int)(c.x + gap()), (int)(y + gap() * 0.5f), (int)(c.width - gap() * 2.0f),
                                    font, MODAL_TEXT);
            float by = row.y + row.height - gap() * 0.5f - button_h();
            button(PLAYER_VIEW_PARTY, (Rectangle){ c.x + gap() * 0.5f, by, bw - gap() * 0.5f, button_h() }, "check",
                   "Accept", C_ACCEPT, true, PP_ACCEPT, in->from_id);
            button(PLAYER_VIEW_PARTY, (Rectangle){ c.x + bw + gap(), by, bw - gap() * 1.5f, button_h() }, "close",
                   "Decline", MODAL_BUTTON, true, PP_DECLINE, in->from_id);
            y += row.height + gap() * 0.5f;
        }
        y += gap();
    }

    if (0 == party->member_count) {
        y += section(c, y, "No party", NULL);
        y += text_wrap("Invite a player from their profile, or a friend from Friends.",
                       (int)c.x, (int)y, (int)c.width, font, MODAL_LABEL, false, true);
        return y - top;
    }

    snprintf(line, sizeof(line), "%d / %d", party->member_count, CYBERIA_PARTY_MAX_MEMBERS);
    y += section(c, y, "Party", line);

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
    button(PLAYER_VIEW_PARTY, (Rectangle){ c.x, y, bw, button_h() }, "log-out", "Leave party", MODAL_BUTTON, true,
           PP_LEAVE, 0);
    if (leads) {
        button(PLAYER_VIEW_PARTY, (Rectangle){ c.x + bw + gap(), y, bw, button_h() }, "close",
               armed(s_disband_armed_at) ? "Tap to confirm" : "Disband", C_DANGER, true, PP_DISBAND, 0);
    }
    y += button_h() + gap();
    return y - top;
}

/* Where a friend is, in the words of the list. */
static void presence_text(const Friend* f, char* out, size_t cap) {
    if (!f->online)             copy_str(out, cap, "Offline");
    else if (0 != f->entity_id) snprintf(out, cap, "Online in %s", f->map_code);
    else                        copy_str(out, cap, "Online in another world");
}

/* A request waits until its day count ends. */
static bool request_open(const Friend* f) { return FRIEND_ACCEPTED == f->status || GetTime() < f->expires_at; }

static void request_days(const Friend* f, char* out, size_t cap) {
    int days = (int)((f->expires_at - GetTime()) / 86400.0) + 1;
    snprintf(out, cap, "%d day%s left", days, 1 == days ? "" : "s");
}

/* Whether the local player may invite a player to its party now: it leads
 * one that has room, or is in none, and the player is not in it. */
static bool invitable(EntityID id) {
    const Party* party = &g_local_player.party;
    bool room = 0 == party->member_count ||
                (local_player_leads_party() && CYBERIA_PARTY_MAX_MEMBERS > party->member_count);
    return room && NULL == local_player_party_member(id);
}

/* One row of the social lists: the identity icon, the name, a status line and
 * a detail line, then `actions` buttons along its bottom. A friend carries its
 * presence as a dot on the icon too. Returns the row. */
static Rectangle social_row(Rectangle c, float y, const Friend* f, const char* status, Color status_color,
                            const char* detail, int actions, bool selected) {
    int font = font_body();
    float face = face_size();
    float lines = (float)(text_line_height(font) * 2 + ('\0' != detail[0] ? text_line_height(font - 2) : 0));
    float head = fmaxf(face, lines) + gap();
    Rectangle row = { c.x, y, c.width, head + (0 < actions ? button_h() + gap() * 0.5f : 0.0f) };
    DrawRectangleRec(row, C_ROW);
    Rectangle id = { c.x + gap() * 0.5f, y + gap() * 0.5f, c.width - gap(), face };
    identity_preview_draw(f->name, status, id, font, MODAL_TEXT, status_color);
    if (FRIEND_ACCEPTED == f->status) {
        Vector2 dot = { id.x + face - 5.0f, id.y + face - 5.0f };
        DrawCircleV(dot, 5.0f, BLACK);
        DrawCircleV(dot, 3.5f, f->online ? C_ONLINE : C_OFFLINE);
    }
    if ('\0' != detail[0])
        modal_draw_clipped_text(detail, (int)(id.x + face + 8.0f), (int)(id.y + (float)text_line_height(font) * 2.0f),
                                (int)(id.width - face - 8.0f), font - 2, MODAL_LABEL);
    if (selected) DrawRectangleLinesEx(row, 1.0f, C_SELECTED);
    else if (ui_button_hit(row, GetMouseX(), GetMouseY())) DrawRectangleLinesEx(row, 1.0f, MODAL_PANEL_BORDER);
    return row;
}

/* One action of a social row. */
typedef struct {
    const char*  icon;
    const char*  label;
    Color        bg;
    bool         enabled;
    PlayerAction action;
} RowAction;

/* The actions along the bottom of a row, side by side. Their icons show only
 * while every action fits its icon and its label. */
static void row_actions(PlayerView view, Rectangle row, const RowAction* actions, int count, const Friend* f) {
    if (0 == count) return;
    float w = (row.width - gap() * (float)(count + 1)) / (float)count;
    bool icons = true;
    for (int i = 0; count > i; i++) {
        UIButtonPixelRetroStyle probe = { .icon_id = actions[i].icon, .label = actions[i].label,
                                          .font_size = font_body() };
        icons = icons && ui_button_pixel_retro_fits((Rectangle){ 0, 0, w, button_h() }, &probe);
    }
    for (int i = 0; count > i; i++) {
        Rectangle r = { row.x + gap() + (float)i * (w + gap()), row.y + row.height - gap() * 0.5f - button_h(), w,
                        button_h() };
        bind_friend(button(view, r, icons ? actions[i].icon : NULL, actions[i].label, actions[i].bg, actions[i].enabled,
                           actions[i].action, f->entity_id), f);
    }
}

static float draw_requests(Rectangle c, float y, const int count[3]) {
    float top = y;
    char line[64];
    for (FriendStatus pass = FRIEND_INCOMING; FRIEND_OUTGOING >= pass; pass++) {
        bool incoming = FRIEND_INCOMING == pass;
        snprintf(line, sizeof line, "%d", count[pass]);
        y += section(c, y, incoming ? "Received" : "Sent", line);
        if (0 == count[pass]) y += message(c, y, incoming ? "No requests to answer." : "No requests sent.");
        for (int i = 0; g_local_player.friend_count > i; i++) {
            const Friend* f = &g_local_player.friends[i];
            if (pass != f->status || !request_open(f)) continue;
            request_days(f, line, sizeof line);
            const RowAction answer[] = { { "check", "Accept", C_ACCEPT, true, PP_FRIEND_ACCEPT },
                                         { "close", "Decline", MODAL_BUTTON, true, PP_FRIEND_DECLINE } };
            const RowAction cancel = { "close", "Cancel request", MODAL_BUTTON, true, PP_FRIEND_DECLINE };
            int actions = incoming ? 2 : 1;
            Rectangle row = social_row(c, y, f, incoming ? "Wants to be your friend" : "Waiting for an answer",
                                       incoming ? C_NOTE : MODAL_LABEL, line, actions, false);
            row_actions(PLAYER_VIEW_REQUESTS, row, incoming ? answer : &cancel, actions, f);
            y += row.height + gap() * 0.5f;
        }
        y += gap();
    }
    return y - top;
}

/* Online friends first. A tap on a row shows its actions: chat and invite
 * while the friend plays on this server, and remove. */
static float draw_friend_list(Rectangle c, float y, int accepted) {
    float top = y;
    char line[96];
    int online = 0;
    for (int i = 0; g_local_player.friend_count > i; i++)
        online += FRIEND_ACCEPTED == g_local_player.friends[i].status && g_local_player.friends[i].online;
    snprintf(line, sizeof line, "%d online   %d / %d", online, accepted, CYBERIA_FRIENDS_MAX);
    y += section(c, y, "Friends", line);
    if (0 == accepted)
        y += message(c, y, "No friends yet. Tap a player's bubble, open the profile and tap Add friend.");
    for (int pass = 0; 2 > pass; pass++) {
        for (int i = 0; g_local_player.friend_count > i; i++) {
            const Friend* f = &g_local_player.friends[i];
            if (FRIEND_ACCEPTED != f->status || f->online != (0 == pass)) continue;
            bool selected = 0 == strcmp(s_selected, f->id);
            bool here = 0 != f->entity_id;
            bool sent = here && NULL != local_player_invite_to(f->entity_id);
            bool invite = here && (sent || invitable(f->entity_id));
            RowAction actions[3];
            int count = 0;
            if (selected) {
                if (here) actions[count++] = (RowAction){ "chat", "Chat", MODAL_BUTTON, true, PP_FRIEND_CHAT };
                if (sent) actions[count++] = (RowAction){ "clock", "Invited", MODAL_BUTTON, false, PP_INVITE };
                else if (invite) actions[count++] = (RowAction){ "party", "Invite", C_ACCEPT, true, PP_INVITE };
                bool confirm = armed(s_remove_armed_at) && 0 == strcmp(s_remove_armed, f->id);
                actions[count++] = (RowAction){ "close", confirm ? "Confirm" : "Remove", C_DANGER, true,
                                                PP_FRIEND_REMOVE };
            }
            presence_text(f, line, sizeof line);
            Rectangle row = social_row(c, y, f, line, f->online ? C_ONLINE : C_OFFLINE, f->note, count, selected);
            row_actions(PLAYER_VIEW_FRIENDS, row, actions, count, f);
            bind_friend(tap_target(PLAYER_VIEW_FRIENDS, row, PP_FRIEND_SELECT, f->entity_id), f);
            y += row.height + gap() * 0.5f;
        }
    }
    return y - top;
}

static float draw_friends(Rectangle c, float y, PlayerView view) {
    float top = y;
    if (!g_local_player.account)
        return message(c, y, "Sign in at the portal to make friends. Friends stay with your account.");
    if (!g_local_player.friends_loaded) return message(c, y, "Loading friends...");
    if (g_local_player.friends_unavailable) y += message(c, y, "Friends are unavailable right now. Trying again...");
    int count[3] = { 0 };
    for (int i = 0; g_local_player.friend_count > i; i++)
        if (request_open(&g_local_player.friends[i])) count[g_local_player.friends[i].status]++;
    if (PLAYER_VIEW_REQUESTS == view) return y - top + draw_requests(c, y, count);
    y += status_note(c, y);
    return y - top + draw_friend_list(c, y, count[FRIEND_ACCEPTED]);
}

void player_panel_update(PlayerView view, Rectangle content, bool ready, float dt) {
    if ((PLAYER_VIEW_FRIENDS == view || PLAYER_VIEW_REQUESTS == view) && g_local_player.account &&
        PP_FRIENDS_REFRESH_S < GetTime() - s_friends_asked_at) {
        s_friends_asked_at = GetTime();
        local_player_request_friends();
    }
    if (s_note_editing && PLAYER_VIEW_FRIENDS == view) {
        if (s_note_input.y >= content.y && s_note_input.y + s_note_input.height <= content.y + content.height) {
            text_input_bridge_show((int)s_note_input.x, (int)s_note_input.y, (int)s_note_input.width,
                                    (int)s_note_input.height, 2 * CYBERIA_STATUS_NOTE_MAX_RUNES, "Status note");
            if (text_input_bridge_submitted()) send_note(false);
        } else text_input_bridge_hide();
    }
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
            : (PLAYER_VIEW_FRIENDS == view || PLAYER_VIEW_REQUESTS == view) ? draw_friends(content, top, view)
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
