#include "session_gate.h"

#include "config.h"
#include "modal.h"
#include "net/client_event.h"
#include "net/game_client.h"
#include "text.h"
#include "ui_button.h"
#include "view/viewport.h"

#include <raylib.h>
#include <stddef.h>
#include <string.h>

#define GATE_ACTIONS_MAX 3

typedef enum { WAY_GUEST, WAY_ACCOUNT, WAY_WORLDS, WAY_SIGN_OUT, WAY_RECONNECT, WAY_CONTINUE } GateWay;

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
#define WORLDS         { WAY_WORLDS, "WORLD SELECT", "home" }
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
      { { WAY_ACCOUNT, "PLAY HERE", "log-in" }, WORLDS } },
    { "unavailable", "The world cannot open your session now.", { { WAY_RECONNECT, "TRY AGAIN", "reload" }, WORLDS } },
    { "invalid_state", "Your progress could not be saved.", { PLAY_ACCOUNT, WORLDS } },
    { "left_saving",
      "Your last changes are still on their way to storage. The world keeps them if storage answers within 10 minutes.",
      { { WAY_CONTINUE, "CONTINUE", "forward" } } },
};

static const GateText GATE_LOST_ACCOUNT = { "", "The connection to the world was lost.", { PLAY_ACCOUNT, WORLDS } };
static const GateText GATE_LOST_GUEST   = { "", "The connection to the world was lost.", { PLAY_RECONNECT, WORLDS } };
static const GateText GATE_RESTART_ACCOUNT = { "", "The world restarted.", { PLAY_ACCOUNT, WORLDS } };
static const GateText GATE_RESTART_GUEST   = { "", "The world restarted.", { PLAY_RECONNECT, WORLDS } };
static const GateText GATE_MENU_ACCOUNT = {
    "", "Your items, coins and progress are saved to your account.",
    { WORLDS, { WAY_GUEST, "PLAY AS GUEST", "anon" }, { WAY_SIGN_OUT, "SIGN OUT", "log-out" } },
};
static const GateText GATE_MENU_GUEST = {
    "", "Guest session: your items and progress end when you leave.",
    { WORLDS, { WAY_ACCOUNT, "SIGN IN TO KEEP PROGRESS", "log-in" } },
};
static const GateText GATE_LEAVING = { "", "Saving your progress...", { { 0 } } };

static struct {
    bool            active;
    bool            account;
    bool            menu;     /* the card is the session menu: it closes */
    bool            leaving;  /* a leave waits for the end of its session */
    GateWay         pending;  /* the way a leave opens */
    const GateText* text;
    float           age;
    Rectangle       buttons[GATE_ACTIONS_MAX];
    Rectangle       close;
} s;

static void show(const GateText* text) {
    s.active = true;
    s.menu = false;
    s.text = text;
    s.age = 0.0f;
}

/* Takes a way: the portal for a launch, the world selection or the sign-out,
 * or a new connection. */
static void open_way(GateWay way) {
    switch (way) {
        case WAY_GUEST:     config_open_portal(PORTAL_PLAY_GUEST); return;
        case WAY_ACCOUNT:   config_open_portal(PORTAL_PLAY_ACCOUNT); return;
        case WAY_WORLDS:    config_open_portal(PORTAL_WORLDS); return;
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

void session_gate_end(const char* reason) {
    if (s.leaving && 0 == strcmp(reason, "left")) {
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
    if (s.leaving) finish_leave();
    else show(s.account ? &GATE_LOST_ACCOUNT : &GATE_LOST_GUEST);
}

void session_gate_menu(void) {
    show(s.account ? &GATE_MENU_ACCOUNT : &GATE_MENU_GUEST);
    s.menu = true;
}

void session_gate_set_account(bool account) { s.account = account; }
bool session_gate_account(void) { return s.account; }
bool session_gate_active(void) { return s.active; }
bool session_gate_leaving(void) { return s.leaving; }

/* A way chosen in the menu: the session ends first, with its final save. */
static void leave_for(GateWay way) {
    s.pending = way;
    s.leaving = true;
    show(&GATE_LEAVING);
    client_event_push(CLIENT_EVENT_SESSION_LEAVE, (client_event_payload_t){ 0 });
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
    float button_h = mobile ? 40.0f : 44.0f;
    int count = action_count();
    float body_h = (float)text_wrap(s.text->message, 0, 0, (int)(w - 2.0f * pad), body_font, MODAL_TEXT, true, false);
    float header_h = (float)title_font + pad;
    float h = header_h + pad + body_h + pad + (float)count * (button_h + pad);
    Rectangle card = { area.x + (area.width - w) * 0.5f, area.y + (area.height - h) * 0.5f, w, h };

    modal_draw_card(card, false, s.age, area.y + area.height, MODAL_PANEL_BORDER, header_h);
    modal_draw_title("SESSION", card, pad, header_h, title_font, MODAL_TEXT, card.x + card.width);
    s.close = (Rectangle){ 0 };
    if (s.menu) {
        float size = header_h - pad;
        s.close = (Rectangle){ card.x + card.width - pad - size, card.y + pad * 0.5f, size, size };
        UIButtonStyle close = { .icon_id = "close-yellow", .no_fill = true };
        ui_button_draw(s.close, &close, ui_button_resolve_state(true, false, ui_button_hit(s.close, GetMouseX(), GetMouseY())));
    }
    float y = card.y + header_h + pad;
    text_wrap(s.text->message, (int)(card.x + pad), (int)y, (int)(w - 2.0f * pad), body_font, MODAL_TEXT, true, true);
    y += body_h + pad;
    for (int i = 0; count > i; i++) {
        const GateAction* action = &s.text->actions[i];
        s.buttons[i] = (Rectangle){ card.x + pad, y, w - 2.0f * pad, button_h };
        UIButtonPixelRetroStyle style = {
            .bg = 0 == i ? MODAL_ACCEPT : (Color){ 24, 30, 48, 255 }, .icon_id = action->icon,
            .label = action->label, .font_size = body_font, .text_color = WHITE, .enabled = true,
        };
        ui_button_pixel_retro_draw(s.buttons[i], &style, ui_button_hit(s.buttons[i], GetMouseX(), GetMouseY()));
        y += button_h + pad;
    }
}

bool session_gate_handle_click(int mx, int my) {
    if (!s.active) return false;
    if (s.menu && ui_button_hit(s.close, mx, my)) {
        s.active = false;
        return true;
    }
    for (int i = 0; action_count() > i; i++) {
        if (!ui_button_hit(s.buttons[i], mx, my)) continue;
        GateWay way = s.text->actions[i].way;
        if (s.menu) leave_for(way);
        else open_way(way);
        return true;
    }
    return true;
}
