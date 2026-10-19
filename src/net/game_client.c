#include "game_client.h"
#include "socket.h"
#include "config.h"
#include "sim/game_state.h"
#include "message.h"
#include "serial.h"
#include "sim/replication.h"
#include "client_event.h"
#include "audio/audio_context.h"
#include "sim/local_player.h"
#include "fx/fx_death.h"
#include "fx/fx_heal_aura.h"
#include "fx/fx_hit_burst.h"
#include "fx/fx_level_up.h"
#include "fx/fx_obstacle_burst.h"
#include "ui/chat_log.h"
#include "ui/modal.h"
#include "ui/session_gate.h"
#include "ui/ui_state.h"
#include "log.h"
#include "utils.h"

#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <raylib.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    Socket     sock;
    conn_stats stats;
    double     last_reconnect_at;
    int        heartbeat_frames;
} ClientCtx;

static ClientCtx g_client = {0};

/* How long a dropped account session tries to resume, within the life of its
 * resume ticket, and the longest wait between two tries. */
#define RESUME_WINDOW_S  240.0
#define RESUME_BACKOFF_S 15.0

static struct {
    char   ticket[64];   /* the resume ticket, memory only */
    bool   active;       /* reconnecting with the ticket */
    double until;
    double next;
    double backoff;
} s_resume;

/* An in-place travel: the socket of the target world opens once the socket of
 * the world left has closed, and the gate shows until the target sends init_data. */
static struct {
    bool pending;
    bool open;
} s_enter;

static void on_socket_open(void* ctx);
static void on_socket_receive(const uint8_t* data, uint32_t length, void* ctx);
static void on_socket_error(void* ctx);
static void on_socket_close(int code, const char* reason, void* ctx);

static void client_reset_state(void) {
    game_state_reset();
    local_player_reset();
    ui_state_reset();
    audio_context_reset();
    fx_level_up_reset();
    fx_heal_aura_reset();
    fx_hit_burst_reset();
    fx_death_reset();
    fx_obstacle_burst_reset();
    session_reset();
    prediction_reset();
    client_event_reset();
    chat_log_reset();
}

bool connection_open(void) {
    message_set_init_handler(client_on_init_received);
    SocketHandlers callbacks = {
        .on_open    = on_socket_open,
        .on_receive = on_socket_receive,
        .on_error   = on_socket_error,
        .on_close   = on_socket_close,
    };
    if (!socket_open(&g_client.sock, config_ws_url(), &g_client, callbacks)) {
        LOG_ERROR("WebSocket open failed");
        return false;
    }
    return true;
}

void connection_close(void) {
    socket_close(&g_client.sock);
}

bool connection_is_open(void) {
    return socket_is_open(&g_client.sock);
}

conn_stats connection_get_stats(void) {
    return g_client.stats;
}

void client_on_init_received(void) {
    LOG_INFO("init_data received");
    s_resume.active = false;
    s_enter.open = false;
    session_gate_arrived();
    /* Every join starts in stasis; the modal count tells the server the truth. */
    modal_push_stasis();
}

void game_client_on_tick(void) {
    const double now = GetTime();
    g_client.heartbeat_frames++;
    if ((g_client.heartbeat_frames % 1800) == 0) { /* ~30 s @ 60 fps */
        LOG_DEBUG("heartbeat frame=%d t=%.1fs connected=%d init=%d",
                  g_client.heartbeat_frames, now,
                  connection_is_open() ? 1 : 0,
                  g_game_state.init_received ? 1 : 0);
    }

    if (s_enter.pending && !socket_is_active(&g_client.sock)) {
        s_enter.pending = false;
        s_enter.open = connection_open();
        if (!s_enter.open) session_gate_lost();
    }

    if (s_resume.active && !socket_is_active(&g_client.sock) && now >= s_resume.next) {
        if (now > s_resume.until) {
            s_resume.active = false;
            session_gate_lost();
        } else {
            s_resume.next = now + s_resume.backoff;
            s_resume.backoff = s_resume.backoff * 2.0 < RESUME_BACKOFF_S ? s_resume.backoff * 2.0 : RESUME_BACKOFF_S;
            connection_open();
        }
    }

    /* A guest reconnects as a new guest. An account session resumes or waits
     * at its gate, and an ended session waits for the player there. */
    if (!socket_is_active(&g_client.sock) && !s_resume.active && !session_gate_active() && !session_gate_account()) {
        static const double retry_interval = 3.0;
        if (now - g_client.last_reconnect_at >= retry_interval) {
            g_client.last_reconnect_at = now;
            LOG_INFO("reconnecting...");
            connection_open();
        }
    }
}

bool network_send(cJSON* msg) {
    assert(msg);
    RawPack pack = serial_pack(msg);
    bool ok = connection_is_open() && socket_send(&g_client.sock, pack.data, pack.len);
    g_client.stats.bytes_up += ok ? pack.len : 0;
    free(pack.data);
    return ok;
}

void game_client_hold_resume(const char* ticket) { copy_str(s_resume.ticket, sizeof s_resume.ticket, ticket); }

void game_client_enter_world(void) { s_enter.pending = true; }

/* Starts resuming a dropped account session; false without a resume ticket. */
static bool start_resume(void) {
    if (!session_gate_account() || '\0' == s_resume.ticket[0]) return false;
    s_resume.active = true;
    s_resume.until = GetTime() + RESUME_WINDOW_S;
    s_resume.next = GetTime() + 1.0;
    s_resume.backoff = 1.0;
    return true;
}

void game_client_on_session_end(const char* reason, const char* travel_ticket) {
    if (s_resume.active && 0 == strcmp(reason, "busy")) return;
    if (0 == strcmp(reason, "server_restart") && start_resume()) return;
    s_resume.active = false;
    s_resume.ticket[0] = '\0';
    s_enter.open = false;
    session_gate_end(reason, travel_ticket);
}

/* ── Socket callbacks ────────────────────────────────────────────────── */

static void on_socket_open(void* ctx) {
    char ticket[64];
    if (s_resume.active) copy_str(ticket, sizeof ticket, s_resume.ticket);
    else config_take_launch_ticket(ticket, sizeof(ticket));
    network_send(json_pack_auth(ticket));
    LOG_INFO("WebSocket open");
}

static void on_socket_receive(const uint8_t* data, uint32_t length, void* ctx) {
    assert(data);
    assert(length > 0);
    assert(ctx);
    ClientCtx* st = ctx;
    st->stats.bytes_down += length;
    message_receive(data, (size_t)length);
}

static void on_socket_error(void* ctx) {
    LOG_ERROR("WebSocket error");
}

static void on_socket_close(int code, const char* reason, void* ctx) {
    if (code != 1000) {
        LOG_WARN("WebSocket closed unexpectedly code=%d reason='%s'",
                 code, reason ? reason : "none");
    }
    client_reset_state();
    if (s_enter.open) {
        s_enter.open = false;
        session_gate_lost();
    } else if (session_gate_leaving()) session_gate_lost();
    else if (!session_gate_active() && !s_resume.active && session_gate_account() && !start_resume())
        session_gate_lost();
}
