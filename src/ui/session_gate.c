#include "session_gate.h"

#include "config.h"
#include "modal.h"
#include "audio/audio.h"
#include "js/fullscreen_bridge.h"
#include "utils.h"
#include "net/client_event.h"
#include "net/game_client.h"
#include "text.h"
#include "ui_button.h"
#include "ui_switch.h"
#include "view/viewport.h"
#include "world/world_types.h"

#include <raylib.h>
#include <math.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#define GATE_ACTIONS_MAX 3


typedef enum { WAY_GUEST, WAY_ACCOUNT, WAY_PORTAL, WAY_SIGN_OUT, WAY_RECONNECT, WAY_CONTINUE, WAY_TRAVEL } GateWay;

typedef struct {
    GateWay     way;
    const char* label;
    const char* icon;
} GateAction;

typedef struct {
    const char* reason;
    const char* message;
    GateAction  actions[GATE_ACTIONS_MAX];
} GateText;

#define GUEST          { WAY_GUEST, "CONTINUE AS GUEST", "anon" }
#define SIGN_IN        { WAY_ACCOUNT, "SIGN IN", "log-in" }
#define PORTAL         { WAY_PORTAL, "CYBERIA PORTAL", "cyberia-yellow-1" }
#define PLAY_ACCOUNT   { WAY_ACCOUNT, "PLAY AGAIN", "reload" }
#define PLAY_RECONNECT { WAY_RECONNECT, "PLAY AGAIN", "reload" }

/* Every reason a session ends or a launch is refused, in the words the player
 * reads, with its ways forward. */
static const GateText GATE_TEXTS[] = {
    { "auth_required", "Play now as a guest, or sign in to keep your items, coins and progress.", { GUEST, SIGN_IN } },
    { "revoked", "You signed out. Sign in to play your character, or continue as a guest.", { SIGN_IN, GUEST } },
    { "expired", "This game link expired.", { SIGN_IN, GUEST } },
    { "replayed", "This game link was already used.", { SIGN_IN, GUEST } },
    { "unknown", "This game link is not valid.", { SIGN_IN, GUEST } },
    { "wrong_instance", "This game link opens another world.", { SIGN_IN, GUEST } },
    { "invalid", "The game could not start this session.", { SIGN_IN, GUEST } },
    { "signed_in_elsewhere", "Your account is playing in another window.",
      { { WAY_ACCOUNT, "PLAY HERE", "log-in" }, PORTAL } },
    { "unavailable", "The world cannot open your session now.", { { WAY_ACCOUNT, "TRY AGAIN", "reload" }, PORTAL } },
    { "saving", "Your progress is still saving in the world you left. Try again in a moment.",
      { { WAY_ACCOUNT, "TRY AGAIN", "reload" }, PORTAL } },
    { "ended", "The game team ended this session. Play again to continue.", { PLAY_ACCOUNT, PORTAL } },
    { "invalid_state", "Your progress could not be saved.", { PLAY_ACCOUNT, PORTAL } },
    { "left_saving", "Your last changes are still saving. The world keeps them and retries for up to 10 minutes.",
      { { WAY_CONTINUE, "CONTINUE", "forward" } } },
};

static const GateText GATE_LOST_ACCOUNT = { "", "The connection to the world was lost.", { PLAY_ACCOUNT, PORTAL } };
static const GateText GATE_LOST_GUEST   = { "", "The connection to the world was lost.", { PLAY_RECONNECT, PORTAL } };
static const GateText GATE_RESTART_ACCOUNT = { "", "The world restarted.", { PLAY_ACCOUNT, PORTAL } };
static const GateText GATE_RESTART_GUEST   = { "", "The world restarted.", { PLAY_RECONNECT, PORTAL } };
static const GateText GATE_SETTINGS_ACCOUNT = {
    "", "", { { WAY_GUEST, "PLAY AS GUEST", "anon" }, { WAY_SIGN_OUT, "SIGN OUT", "log-out" } },
};
static const GateText GATE_SETTINGS_GUEST = {
    "", "",
    { { WAY_ACCOUNT, "SIGN IN TO KEEP PROGRESS", "log-in" } },
};
static const GateText GATE_LEAVING = { "", "Saving your progress...", { { 0 } } };

/* A travel shows what happens now: the save of this world, then the entry of the next one. */
static struct {
    char     world[MAX_ID_LENGTH];       /* instance code of the target */
    char     path[MAX_ID_LENGTH];        /* its path on the client */
    char     name[MAX_ID_LENGTH];
    char     ticket[64];                 /* the ticket the end of the session carried */
    bool     in_place;                   /* this client plays the target: no page change */
    bool     entering;                   /* the target world is opening in this page */
    char     message[MAX_ID_LENGTH + 48];
    GateText text;
} s_travel;

static struct {
    bool            active;
    bool            account;
    bool            settings; /* the card is the settings menu: it closes */
    bool            leaving;  /* a leave waits for the end of its session */
    GateWay         pending;  /* the way a leave opens */
    const GateText* text;
    float           age;
    Rectangle       buttons[GATE_ACTIONS_MAX];
    Rectangle       portal, sound, fullscreen;
    Rectangle       close;
    Rectangle       card;
    UISwitch        sound_switch, fullscreen_switch;
} s;

static void show(const GateText* text) {
    s.active = true;
    s.settings = false;
    s.text = text;
    s.age = 0.0f;
}

/* The travel card: its message names the target world. */
static void show_travel(const char* lead, const char* tail) {
    snprintf(s_travel.message, sizeof s_travel.message, "%s%s%s", lead, s_travel.name, tail);
    s_travel.text = (GateText){ "", s_travel.message, { { 0 } } };
    show(&s_travel.text);
}

/* Opens the target world through the launch bridge: the way every travel can take. */
static void travel_by_bridge(void) {
    s_travel.entering = false;
    config_open_launch(s_travel.world, s.account);
}

/* Opens the target world in this page with the ticket of the travel. */
static void enter_world(void) {
    s_travel.entering = true;
    show_travel("Entering ", "...");
    config_enter_world(s_travel.path, s_travel.ticket);
    s_travel.ticket[0] = '\0';
    game_client_enter_world();
}

/* Takes a way: the portal for a launch, the world selection or the sign-out,
 * or a new connection. */
static void open_way(GateWay way) {
    switch (way) {
        case WAY_GUEST:     config_open_launch(config_instance_code(), false); return;
        case WAY_ACCOUNT:   config_open_launch(config_instance_code(), true); return;
        case WAY_TRAVEL:
            if (s_travel.in_place && '\0' != s_travel.ticket[0]) enter_world();
            else travel_by_bridge();
            return;
        case WAY_PORTAL:    config_open_portal(PORTAL_WORLDS); return;
        case WAY_SIGN_OUT:  config_open_portal(PORTAL_SIGN_OUT); return;
        case WAY_CONTINUE:  open_way(s.pending); return;
        case WAY_RECONNECT:
            s.active = false;
            connection_open();
            return;
    }
}

static void finish_leave(void) {
    s.leaving = false;
    open_way(s.pending);
}

void session_gate_end(const char* reason, const char* travel_ticket) {
    if (s_travel.entering) {
        travel_by_bridge();
        return;
    }
    /* A travel goes on with its ticket even while the last changes save: the
     * target holds the session back until they did. */
    if (s.leaving && (0 == strcmp(reason, "left") || '\0' != travel_ticket[0])) {
        copy_str(s_travel.ticket, sizeof s_travel.ticket, travel_ticket);
        finish_leave();
        return;
    }
    if (s.leaving && 0 == strcmp(reason, "left_saving")) s.leaving = false;
    if (0 == strcmp(reason, "server_restart")) {
        show(s.account ? &GATE_RESTART_ACCOUNT : &GATE_RESTART_GUEST);
        return;
    }
    for (size_t i = 0; i < sizeof(GATE_TEXTS) / sizeof(GATE_TEXTS[0]); i++) {
        if (0 == strcmp(GATE_TEXTS[i].reason, reason)) {
            show(&GATE_TEXTS[i]);
            return;
        }
    }
    session_gate_lost();
}

void session_gate_lost(void) {
    if (s_travel.entering) travel_by_bridge();
    else if (s.leaving) finish_leave();
    else show(s.account ? &GATE_LOST_ACCOUNT : &GATE_LOST_GUEST);
}

void session_gate_arrived(void) {
    if (!s_travel.entering) return;
    s_travel.entering = false;
    s.active = false;
}

void session_gate_settings(void) {
    show(s.account ? &GATE_SETTINGS_ACCOUNT : &GATE_SETTINGS_GUEST);
    s.settings = true;
    ui_switch_init(&s.sound_switch, !audio_is_muted());
    ui_switch_init(&s.fullscreen_switch, fullscreen_bridge_is_active());
}

bool session_gate_settings_open(void) { return s.active && s.settings; }

bool session_gate_handle_escape(void) {
    if (!s.active) return false;
    if (s.settings) s.active = false;
    return true;
}

void session_gate_set_account(bool account) { s.account = account; }
bool session_gate_account(void) { return s.account; }
bool session_gate_active(void) { return s.active; }
bool session_gate_leaving(void) { return s.leaving; }

/* A way chosen in the settings: the session ends first, with its final save.
 * A travel in this page asks the server for the ticket of its target. */
static void leave_for(GateWay way) {
    s.pending = way;
    s.leaving = true;
    client_event_payload_t leave = { 0 };
    if (WAY_TRAVEL == way) {
        show_travel("Saving your progress before the journey to ", "...");
        if (s_travel.in_place) copy_str(leave.code, sizeof leave.code, s_travel.world);
    } else {
        show(&GATE_LEAVING);
    }
    client_event_push(CLIENT_EVENT_SESSION_LEAVE, leave);
}

void session_gate_travel(const char* instance_code, const char* path, const char* name, bool in_place) {
    copy_str(s_travel.world, sizeof s_travel.world, instance_code);
    copy_str(s_travel.path, sizeof s_travel.path, path);
    copy_str(s_travel.name, sizeof s_travel.name, name);
    s_travel.ticket[0] = '\0';
    s_travel.in_place = in_place && s.account;
    leave_for(WAY_TRAVEL);
}

static int action_count(void) {
    int n = 0;
    while (GATE_ACTIONS_MAX > n && NULL != s.text->actions[n].label) n++;
    return n;
}

void session_gate_draw(Rectangle area) {
    if (!s.active) return;
    s.age += GetFrameTime();
    bool mobile = viewport_is_mobile();
    int title_font = mobile ? 18 : 22;
    int body_font = mobile ? 14 : 17;
    float pad = mobile ? 14.0f : 20.0f;
    float w = area.width - 2.0f * pad < 440.0f ? area.width - 2.0f * pad : 440.0f;
    float button_h = s.settings || !mobile ? 44.0f : 40.0f;
    int count = action_count();
    int rows = s.settings ? 3 : 0;
    if (s.settings)
        pad = fminf(pad, fmaxf(6.0f, (area.height - 16.0f - 44.0f - (float)(rows + count) * button_h)
                                    / (float)(rows + count + 1)));
    bool message = '\0' != s.text->message[0];
    float body_h = message ? (float)text_wrap(s.text->message, 0, 0, (int)(w - 2.0f * pad), body_font, MODAL_TEXT,
                                              true, false) + pad
                           : 0.0f;
    float header_h = s.settings ? 44.0f : (float)title_font + pad;
    float h = header_h + pad + (float)(rows + count) * (button_h + pad) + body_h;
    Rectangle card = { area.x + (area.width - w) * 0.5f, area.y + (area.height - h) * 0.5f, w, h };
    s.card = card;
    s.close = s.settings
        ? (Rectangle){ card.x + card.width - pad * 0.5f - 44.0f, card.y, 44.0f, 44.0f }
        : (Rectangle){ 0 };

    modal_draw_card(card, false, s.age, area.y + area.height, MODAL_PANEL_BORDER, header_h);
    modal_draw_title(s.settings ? "SETTINGS" : "SESSION", card, pad, header_h, title_font, MODAL_TEXT,
                     s.settings ? s.close.x : card.x + card.width);
    if (s.settings) ui_button_draw_close(s.close);
    float y = card.y + header_h + pad;
    s.portal = s.sound = s.fullscreen = (Rectangle){ 0 };
    if (s.settings) {
        bool muted = audio_is_muted();
        bool full = fullscreen_bridge_is_active();
        const GateAction way = PORTAL;
        s.sound = (Rectangle){ card.x + pad, y, w - 2.0f * pad, button_h };
        s.fullscreen = (Rectangle){ card.x + pad, y + button_h + pad, w - 2.0f * pad, button_h };
        s.portal = (Rectangle){ card.x + pad, y + 2.0f * (button_h + pad), w - 2.0f * pad, button_h };
        UIButtonPixelRetroStyle portal = {
            .bg = MODAL_BUTTON, .icon_id = way.icon, .label = way.label, .font_size = body_font, .text_color = WHITE,
            .enabled = true,
        };
        UISwitchStyle sound = {
            .icon_id = muted ? "audio-mute" : "audio", .label = "SOUND", .font_size = body_font,
        };
        UISwitchStyle screen = {
            .icon_id = full ? "shrink" : "fullscreen", .label = "FULL SCREEN", .font_size = body_font,
        };
        ui_switch_update(&s.sound_switch, !muted, GetFrameTime());
        ui_switch_update(&s.fullscreen_switch, full, GetFrameTime());
        ui_switch_draw(&s.sound_switch, s.sound, &sound, ui_button_hit(s.sound, GetMouseX(), GetMouseY()));
        ui_switch_draw(&s.fullscreen_switch, s.fullscreen, &screen, ui_button_hit(s.fullscreen, GetMouseX(), GetMouseY()));
        ui_button_pixel_retro_draw(s.portal, &portal, ui_button_hit(s.portal, GetMouseX(), GetMouseY()));
        y += (float)rows * (button_h + pad);
    }
    if (message)
        text_wrap(s.text->message, (int)(card.x + pad), (int)y, (int)(w - 2.0f * pad), body_font, MODAL_TEXT, true, true);
    y += body_h;
    for (int i = 0; count > i; i++) {
        const GateAction* action = &s.text->actions[i];
        s.buttons[i] = (Rectangle){ card.x + pad, y, w - 2.0f * pad, button_h };
        UIButtonPixelRetroStyle style = {
            .bg = 0 == i && !s.settings ? MODAL_ACCEPT : MODAL_BUTTON, .icon_id = action->icon,
            .label = action->label, .font_size = body_font, .text_color = WHITE, .enabled = true,
        };
        ui_button_pixel_retro_draw(s.buttons[i], &style, ui_button_hit(s.buttons[i], GetMouseX(), GetMouseY()));
        y += button_h + pad;
    }
}

bool session_gate_handle_click(int mx, int my) {
    if (!s.active) return false;
    if (s.settings && (!ui_button_hit(s.card, mx, my) || ui_button_hit(s.close, mx, my))) {
        s.active = false;
        return true;
    }
    if (s.settings && ui_button_hit(s.portal, mx, my)) {
        leave_for(WAY_PORTAL);
        return true;
    }
    if (s.settings && ui_button_hit(s.sound, mx, my)) {
        audio_toggle_mute();
        return true;
    }
    if (s.settings && ui_button_hit(s.fullscreen, mx, my)) {
        fullscreen_bridge_toggle();
        return true;
    }
    for (int i = 0; action_count() > i; i++) {
        if (!ui_button_hit(s.buttons[i], mx, my)) continue;
        GateWay way = s.text->actions[i].way;
        if (s.settings) leave_for(way);
        else open_way(way);
        return true;
    }
    return true;
}
