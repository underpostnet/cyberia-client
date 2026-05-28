#include "client.h"
#include "network/socket.h"
#include "config.h"
#include "game_state.h"
#include "message_parser.h"
#include "binary_aoi_decoder.h"
#include "serial.h"
#include "prediction/prediction.h"
#include "domain/local_player.h"
#include "ui/ui_state.h"
#include "util/log.h"

#include <assert.h>
#include <raylib.h>
#include <stdlib.h>
#include <string.h>

/* ── State machine ──────────────────────────────────────────────────────
 *
 *   DISCONNECTED ───open()──▶ CONNECTING
 *   CONNECTING   ──onopen──▶  CONNECTED → AWAITING_INIT
 *   AWAITING_INIT─init_data─▶ RUNNING
 *   * ───onclose──▶ DISCONNECTED  (full state reset)
 *
 * The reconnection guard runs in CLIENT_DISCONNECTED only. AWAITING_INIT
 * has its own timeout that forces a close → reconnect when the server
 * accepted the upgrade but never sent init_data.
 */

#define CLIENT_RECONNECT_INTERVAL_S  3.0
#define CLIENT_AWAITING_INIT_TIMEOUT 15.0

typedef struct {
    WebSocketClient ws_client;
    conn_stats      stats;
    ClientStatus    status;
    double          status_entered_at;
    double          last_reconnect_at;
    int             heartbeat_frames;
} ClientCtx;

static ClientCtx g_client = {0};

static void on_websocket_open(void* ctx);
static void on_websocket_message(const uint8_t* data, uint32_t length, bool is_text, void* ctx);
static void on_websocket_error(void* ctx);
static void on_websocket_close(int code, const char* reason, void* ctx);

static void client_set_status(ClientStatus next) {
    if (g_client.status == next) return;
    g_client.status = next;
    g_client.status_entered_at = GetTime();
}

/* Consolidated post-disconnect reset. Single function so every reset path
 * lands the same set of fields back at their fresh-session defaults. */
static void client_reset_state(void) {
    g_game_state.init_received        = false;
    g_game_state.player_id[0]         = '\0';
    g_game_state.other_player_count   = 0;
    g_game_state.bot_count            = 0;
    g_game_state.resource_count       = 0;
    g_game_state.obstacle_count       = 0;
    g_game_state.foreground_count     = 0;
    g_game_state.portal_count         = 0;
    g_game_state.floor_count          = 0;
    g_game_state.full_inventory_count = 0;
    local_player_reset();
    ui_state_reset();
    binary_aoi_reset_prev_snapshots();
    prediction_reset((Vector2){0.0f, 0.0f});
}

bool connection_open(void) {
    WebSocketHandlers callbacks = {
        .on_open_cb    = on_websocket_open,
        .on_message_cb = on_websocket_message,
        .on_error_cb   = on_websocket_error,
        .on_close_cb   = on_websocket_close,
    };
    if (!ws_open(&g_client.ws_client, WS_URL, &g_client, callbacks)) {
        LOG_ERROR("WebSocket open failed");
        return false;
    }
    client_set_status(CLIENT_CONNECTING);
    return true;
}

void connection_close(void) {
    ws_close(&g_client.ws_client);
}

bool connection_is_open(void) {
    return ws_is_open(&g_client.ws_client);
}

conn_stats connection_get_stats(void) {
    return g_client.stats;
}

ClientStatus client_status(void) {
    return g_client.status;
}

void client_on_init_received(void) {
    if (g_client.status == CLIENT_AWAITING_INIT || g_client.status == CLIENT_CONNECTED) {
        client_set_status(CLIENT_RUNNING);
        LOG_INFO("init_data received — entering RUNNING");
    }
}

void game_client_tick(void) {
    g_client.heartbeat_frames++;
    if ((g_client.heartbeat_frames % 1800) == 0) { /* ~30 s @ 60 fps */
        LOG_DEBUG("heartbeat frame=%d t=%.1fs status=%d active=%d init=%d",
                  g_client.heartbeat_frames, GetTime(),
                  (int)g_client.status, connection_is_open() ? 1 : 0,
                  g_game_state.init_received ? 1 : 0);
    }

    const double now = GetTime();

    switch (g_client.status) {
        case CLIENT_DISCONNECTED: {
            if (now - g_client.last_reconnect_at >= CLIENT_RECONNECT_INTERVAL_S) {
                g_client.last_reconnect_at = now;
                LOG_INFO("reconnecting...");
                if (!connection_open()) {
                    /* connection_open already logs the failure; stay
                     * DISCONNECTED, retry on the next interval. */
                }
            }
            break;
        }
        case CLIENT_AWAITING_INIT: {
            if (now - g_client.status_entered_at >= CLIENT_AWAITING_INIT_TIMEOUT) {
                LOG_WARN("init_data did not arrive within %.0fs — closing socket",
                         CLIENT_AWAITING_INIT_TIMEOUT);
                connection_close();
                /* on_websocket_close drives us back to DISCONNECTED. */
            }
            break;
        }
        case CLIENT_CONNECTING:
        case CLIENT_CONNECTED:
        case CLIENT_RUNNING:
            break;
    }
}

bool network_send_binary(const uint8_t* data, uint16_t len) {
    assert(data);
    assert(len > 0);
    if (!connection_is_open()) return false;
    bool ok = ws_send_binary(&g_client.ws_client, data, len);
    g_client.stats.bytes_up += ok ? len : 0;
    return ok;
}

bool network_send_event_tap(Vector2 grid, uint32_t client_tick_v, uint32_t sequence) {
    BinWriter w;
    uplink_player_action(&w, grid.x, grid.y, client_tick_v, sequence);
    return network_send_binary(w.buf, w.pos);
}

bool network_send_chat(const char* to_id, const char* text) {
    assert(to_id);
    assert(text);
    BinWriter w;
    uplink_chat(&w, to_id, text);
    return network_send_binary(w.buf, w.pos);
}

/* ── WebSocket callbacks ─────────────────────────────────────────────── */

static void on_websocket_open(void* ctx) {
    BinWriter w;
    uplink_handshake(&w, "cyberia-mmo", "1.0.0");
    network_send_binary(w.buf, w.pos);

    client_set_status(CLIENT_AWAITING_INIT);
    LOG_INFO("WebSocket open — awaiting init_data");
}

static void on_websocket_message(const uint8_t* data, uint32_t length, bool is_text, void* ctx) {
    ClientCtx* st = ctx;
    if (!data || length == 0 || !st) return;

    st->stats.bytes_down += length;

    if (is_text) {
        char* msg = (char*)malloc((size_t)length + 1);
        if (!msg) {
            LOG_ERROR("OOM allocating %u-byte WS text buffer", length);
            return;
        }
        memcpy(msg, data, length);
        msg[length] = '\0';
        if (!message_parser_parse(msg)) {
            LOG_ERROR("failed to process WS text frame");
        }
        free(msg);
    } else if (0 != binary_aoi_process(data, (size_t)length)) {
        LOG_ERROR("failed to process binary AOI message");
    }
}

static void on_websocket_error(void* ctx) {
    LOG_ERROR("WebSocket error");
}

static void on_websocket_close(int code, const char* reason, void* ctx) {
    if (code != 1000) {
        LOG_WARN("WebSocket closed unexpectedly code=%d reason='%s'",
                 code, reason ? reason : "none");
    }
    client_reset_state();
    client_set_status(CLIENT_DISCONNECTED);
}
