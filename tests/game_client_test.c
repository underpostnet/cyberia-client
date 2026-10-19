#include "net/game_client.h"
#include "net/socket.h"
#include "net/message.h"
#include "net/client_event.h"
#include "config.h"
#include "sim/game_state.h"
#include "sim/local_player.h"
#include "sim/replication.h"
#include "ui/session_gate.h"
#include "ui/modal.h"
#include "ui/ui_state.h"
#include "ui/chat_log.h"
#include "audio/audio_context.h"
#include "fx/fx_death.h"
#include "fx/fx_heal_aura.h"
#include "fx/fx_hit_burst.h"
#include "fx/fx_level_up.h"
#include "fx/fx_obstacle_burst.h"

#include <assert.h>
#include <cJSON.h>
#include <raylib.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

GameState g_game_state;
static double s_now;
static bool s_active, s_open, s_account, s_gate;
static int s_created, s_launch_reads, s_sends, s_lost, s_resets;
static const char* s_launch_ticket = "launch-ticket";
static char s_auth_ticket[64], s_reason[64];
static SocketHandlers s_callbacks;
static void* s_ctx;

double GetTime(void) { return s_now; }
const char* config_ws_url(void) { return "wss://world.test/ws"; }
bool config_take_launch_ticket(char* out, size_t size) {
    snprintf(out, size, "%s", s_launch_ticket);
    s_launch_ticket = "";
    s_launch_reads++;
    return '\0' != out[0];
}
bool socket_open(Socket* sock, const char* url, void* ctx, SocketHandlers callbacks) {
    assert(!s_active);
    s_active = true;
    s_open = false;
    sock->socket = ++s_created;
    s_callbacks = callbacks;
    s_ctx = ctx;
    return true;
}
void socket_close(Socket* sock) { s_active = s_open = false; sock->socket = 0; }
bool socket_is_active(const Socket* sock) { return s_active; }
bool socket_is_open(const Socket* sock) { return s_open; }
bool socket_send(const Socket* sock, const void* data, size_t length) {
    assert(s_open);
    cJSON* msg = cJSON_ParseWithLength(data, length);
    assert(NULL != msg);
    cJSON* payload = cJSON_GetObjectItemCaseSensitive(msg, "payload");
    cJSON* ticket = cJSON_GetObjectItemCaseSensitive(payload, "ticket");
    assert(cJSON_IsString(ticket));
    snprintf(s_auth_ticket, sizeof s_auth_ticket, "%s", ticket->valuestring);
    cJSON_Delete(msg);
    s_sends++;
    return true;
}
void message_set_init_handler(MessageInitHandler handler) { assert(NULL != handler); }
void message_receive(const uint8_t* data, size_t length) {}
bool session_gate_active(void) { return s_gate; }
bool session_gate_account(void) { return s_account; }
bool session_gate_leaving(void) { return false; }
void session_gate_arrived(void) {}
void session_gate_lost(void) { s_gate = true; s_lost++; }
void session_gate_end(const char* reason, const char* travel_ticket) {
    s_gate = true;
    snprintf(s_reason, sizeof s_reason, "%s", reason);
}
void game_state_reset(void) { s_resets++; g_game_state.init_received = false; }
void local_player_reset(void) {}
void ui_state_reset(void) {}
void audio_context_reset(void) {}
void fx_level_up_reset(void) {}
void fx_heal_aura_reset(void) {}
void fx_hit_burst_reset(void) {}
void fx_death_reset(void) {}
void fx_obstacle_burst_reset(void) {}
void session_reset(void) {}
void prediction_reset(void) {}
void client_event_reset(void) {}
void chat_log_reset(void) {}
void modal_push_stasis(void) {}

static void tick(double now) { s_now = now; game_client_on_tick(); }
static void open_event(void) { s_open = true; s_callbacks.on_open(s_ctx); }
static void close_event(void) { s_active = s_open = false; s_callbacks.on_close(1000, "", s_ctx); }

int main(void) {
    tick(5.0);
    assert(1 == s_created && s_active && !s_open);
    tick(10.0);
    tick(30.0);
    assert(1 == s_created && 0 == s_launch_reads);
    open_event();
    assert(1 == s_sends && 1 == s_launch_reads && 0 == strcmp("launch-ticket", s_auth_ticket));
    s_account = true;
    client_on_init_received();
    game_client_hold_resume("resume-ticket");
    close_event();
    assert(1 == s_resets && 0 == s_lost);
    tick(30.5);
    assert(1 == s_created);
    tick(31.0);
    tick(60.0);
    assert(2 == s_created && 1 == s_launch_reads);
    open_event();
    assert(2 == s_sends && 1 == s_launch_reads && 0 == strcmp("resume-ticket", s_auth_ticket));
    client_on_init_received();
    close_event();
    tick(61.0);
    open_event();
    game_client_on_session_end("unavailable", "");
    close_event();
    tick(100.0);
    assert(3 == s_created && s_gate && 0 == strcmp("unavailable", s_reason));

    s_account = s_gate = false;
    s_launch_ticket = "travel-ticket";
    tick(105.0);
    assert(4 == s_created);
    game_client_enter_world();
    tick(110.0);
    assert(4 == s_created);
    close_event();
    tick(111.0);
    assert(5 == s_created);
    open_event();
    assert(2 == s_launch_reads && 0 == strcmp("travel-ticket", s_auth_ticket));
    close_event();
    assert(1 == s_lost && s_gate);
    tick(120.0);
    assert(5 == s_created);

    puts("game_client_test OK");
    return 0;
}
