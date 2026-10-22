#include "ui/session_gate.h"
#include "ui/modal.h"
#include "ui/ui_button.h"
#include "ui/ui_switch.h"
#include "config.h"
#include "audio/audio.h"
#include "js/fullscreen_bridge.h"
#include "net/client_event.h"
#include "net/game_client.h"
#include "utils.h"
#include "view/viewport.h"
#include "ui/text.h"

#include <assert.h>
#include <raylib.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

const Color MODAL_PANEL_BORDER = { 80, 80, 130, 220 };
const Color MODAL_TEXT = { 220, 220, 230, 240 };
const Color MODAL_ACCEPT = { 38, 138, 76, 255 };
const Color MODAL_BUTTON = { 24, 30, 48, 255 };

static Rectangle s_card, s_close, s_sound, s_fullscreen, s_portal, s_signout, s_retry;
static bool s_mobile, s_muted, s_full, s_sound_on, s_full_on;
static int s_leaves, s_redirects, s_launches, s_connections, s_entered;
static char s_entered_ticket[64];
static PortalPage s_page;

bool viewport_is_mobile(void) { return s_mobile; }
float GetFrameTime(void) { return 0.016f; }
int GetMouseX(void) { return -1; }
int GetMouseY(void) { return -1; }
bool audio_is_muted(void) { return s_muted; }
void audio_toggle_mute(void) { s_muted = !s_muted; }
bool fullscreen_bridge_is_active(void) { return s_full; }
void fullscreen_bridge_toggle(void) { s_full = !s_full; }
const char* config_instance_code(void) { return "world"; }
void config_open_portal(PortalPage page) { s_redirects++; s_page = page; }
void config_open_launch(const char* code, bool account) {
    assert(0 == strcmp("world", code) && account);
    s_launches++;
}
void config_enter_world(const char* path, const char* ticket) {
    s_entered++;
    snprintf(s_entered_ticket, sizeof s_entered_ticket, "%s", ticket);
}
void game_client_enter_world(void) {}
bool connection_open(void) { s_connections++; return true; }
void copy_str(char* dst, size_t cap, const char* src) { snprintf(dst, cap, "%s", src); }
void client_event_push(client_event_kind_t kind, client_event_payload_t payload) {
    assert(CLIENT_EVENT_SESSION_LEAVE == kind);
    s_leaves++;
}
void modal_draw_card(Rectangle card, bool anchored, float age, float overlay_h, Color border, float header_h) {
    s_card = card;
}
void modal_draw_title(const char* text, Rectangle card, float pad, float header_h, int font, Color color, float close_x) {}
int text_wrap(const char* text, int x, int y, int maxw, int size, Color color, bool center, bool draw) {
    assert(NULL == strstr(text, "Guest session:"));
    return size;
}
bool ui_button_hit(Rectangle r, int x, int y) {
    return r.x <= x && r.x + r.width > x && r.y <= y && r.y + r.height > y;
}
void ui_button_draw_close(Rectangle bounds) { s_close = bounds; }
void ui_button_pixel_retro_draw(Rectangle bounds, const UIButtonPixelRetroStyle* style, bool hovered) {
    if (0 == strcmp("CYBERIA PORTAL", style->label)) s_portal = bounds;
    if (0 == strcmp("SIGN OUT", style->label)) s_signout = bounds;
    if (0 == strcmp("TRY AGAIN", style->label)) s_retry = bounds;
}
void ui_switch_init(UISwitch* control, bool on) { control->on = on; }
void ui_switch_update(UISwitch* control, bool on, float dt) { control->on = on; }
void ui_switch_draw(const UISwitch* control, Rectangle bounds, const UISwitchStyle* style, bool hovered) {
    if (0 == strcmp("SOUND", style->label)) { s_sound = bounds; s_sound_on = control->on; }
    if (0 == strcmp("FULL SCREEN", style->label)) { s_fullscreen = bounds; s_full_on = control->on; }
}

static void tap(Rectangle rect) {
    assert(session_gate_handle_click((int)(rect.x + 1.0f), (int)(rect.y + rect.height * 0.5f)));
}

static void layout(int width, int height, bool account) {
    s_mobile = 600 > width || 600 > height;
    session_gate_set_account(account);
    session_gate_settings();
    session_gate_draw((Rectangle){ 0, 0, (float)width, (float)height });
    assert(0.0f <= s_card.x && 0.0f <= s_card.y);
    assert((float)width >= s_card.x + s_card.width);
    assert((float)height >= s_card.y + s_card.height);
    assert(44.0f <= s_sound.height && 44.0f <= s_fullscreen.height);
    assert(44.0f <= s_close.width && 44.0f <= s_close.height);
    assert(s_fullscreen.y >= s_sound.y + s_sound.height);
    assert(s_portal.y >= s_fullscreen.y + s_fullscreen.height);
}

int main(void) {
    assert(!session_gate_handle_click(0, 0));
    assert(!session_gate_handle_escape());
    const int sizes[][2] = { { 1280, 800 }, { 375, 667 }, { 844, 390 }, { 568, 320 }, { 320, 568 } };
    for (size_t i = 0; sizeof sizes / sizeof sizes[0] > i; i++) {
        for (int account = 0; 2 > account; account++) {
            layout(sizes[i][0], sizes[i][1], 0 != account);
            assert(session_gate_handle_click((int)s_card.x + 2, (int)s_card.y + 2));
            assert(session_gate_settings_open());
            assert(session_gate_handle_click(0, 0));
            assert(!session_gate_active());
            assert(0 == s_leaves && 0 == s_redirects);
        }
    }

    layout(1280, 800, true);
    tap(s_close);
    assert(!session_gate_active());
    layout(1280, 800, true);
    assert(session_gate_handle_escape());
    assert(!session_gate_active());

    layout(1280, 800, true);
    assert(s_sound_on && !s_full_on);
    tap(s_sound);
    assert(s_muted && session_gate_settings_open());
    tap(s_fullscreen);
    assert(s_full && session_gate_settings_open());
    session_gate_draw((Rectangle){ 0, 0, 1280, 800 });
    assert(!s_sound_on && s_full_on);
    s_full = false;
    session_gate_draw((Rectangle){ 0, 0, 1280, 800 });
    assert(!s_full_on);
    tap(s_sound);
    assert(!s_muted);

    tap(s_signout);
    assert(1 == s_leaves && 0 == s_redirects);
    assert(session_gate_leaving() && !session_gate_settings_open());
    assert(session_gate_handle_click(0, 0));
    assert(session_gate_active() && session_gate_leaving());
    assert(session_gate_handle_escape());
    assert(session_gate_active() && session_gate_leaving());
    session_gate_end("left", "");
    assert(1 == s_redirects && PORTAL_SIGN_OUT == s_page);
    assert(!session_gate_leaving());

    session_gate_end("revoked", "");
    session_gate_draw((Rectangle){ 0, 0, 1280, 800 });
    assert(session_gate_handle_click(0, 0));
    assert(session_gate_active() && !session_gate_settings_open());

    session_gate_end("unavailable", "");
    session_gate_draw((Rectangle){ 0, 0, 1280, 800 });
    tap(s_retry);
    assert(1 == s_launches && 0 == s_connections);

    /* The target world holds the session back while the world left saves. */
    session_gate_end("saving", "");
    session_gate_draw((Rectangle){ 0, 0, 1280, 800 });
    tap(s_retry);
    assert(2 == s_launches);
    session_gate_end("ended", "");
    assert(session_gate_active() && !session_gate_leaving());

    /* A travel whose last changes still save goes on with its ticket. */
    session_gate_travel("world-b", "/b", "World B", true);
    assert(2 == s_leaves && session_gate_leaving());
    session_gate_end("left_saving", "ticket-b");
    assert(1 == s_entered && 0 == strcmp("ticket-b", s_entered_ticket) && !session_gate_leaving());
    session_gate_arrived();
    assert(!session_gate_active());
    /* Without a ticket the card says the changes keep saving. */
    session_gate_travel("world-b", "/b", "World B", true);
    session_gate_end("left_saving", "");
    assert(1 == s_entered && session_gate_active() && !session_gate_leaving());

    puts("session_gate_test OK");
    return 0;
}
