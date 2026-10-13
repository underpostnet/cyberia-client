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
#include "ui/modal.h"
#include "ui/ui_state.h"
#include "log.h"

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
    double     status_entered_at;
    double     last_reconnect_at;
    int        heartbeat_frames;
} ClientCtx;

static ClientCtx g_client = {0};

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

    // This is a draft for a reconnection feature... there are better ways to handle this
    if(!connection_is_open()) {
        static const double retry_interval = 3.0;
        if (now - g_client.last_reconnect_at >= retry_interval) {
            g_client.last_reconnect_at = now;
            LOG_INFO("reconnecting...");
            if (!connection_open()) {
                /* connection_open already logs the failure; stay
                    * DISCONNECTED, retry on the next interval. */
            }
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

/* ── Socket callbacks ────────────────────────────────────────────────── */

static void on_socket_open(void* ctx) {
    network_send(json_pack_handshake("cyberia-mmo", "1.0.0"));
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
}
