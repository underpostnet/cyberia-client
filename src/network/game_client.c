#include "game_client.h"
#include "network/socket.h"
#include "config.h"
#include "game_state.h"
#include "message_parser.h"
#include "binary_aoi_decoder.h"
#include "serial.h"
#include "replication.h"
#include "domain/local_player.h"
#include "ui/ui_state.h"
#include "util/log.h"

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <raylib.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    WebSocketClient ws_client;
    conn_stats      stats;
    double          status_entered_at;
    double          last_reconnect_at;
    int             heartbeat_frames;
} ClientCtx;

static ClientCtx g_client = {0};

static void on_websocket_open(void* ctx);
static void on_websocket_message(const uint8_t* data, uint32_t length, bool is_text, void* ctx);
static void on_websocket_error(void* ctx);
static void on_websocket_close(int code, const char* reason, void* ctx);

static void client_reset_state(void) {
    game_state_reset();
    local_player_reset();
    ui_state_reset();
    binary_aoi_reset_prev_snapshots();
    prediction_reset((Vector2){0.0f, 0.0f});
}

bool connection_open(void) {
    message_parser_set_init_handler(client_on_init_received);
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

void client_on_init_received(void) {
    LOG_INFO("init_data received");
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

bool network_send_binary(const uint8_t* data, uint16_t len) {
    assert(data);
    assert(len > 0);
    if (!connection_is_open()) return false;
    bool ok = ws_send_binary(&g_client.ws_client, data, len);
    g_client.stats.bytes_up += ok ? len : 0;
    return ok;
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
    LOG_INFO("WebSocket open");
}

static void on_websocket_message(const uint8_t* data, uint32_t length, bool is_text, void* ctx) {
    assert(data);
    assert(length > 0);
    assert(ctx);
    ClientCtx* st = ctx;

    st->stats.bytes_down += length;

    if (is_text) {
        char* msg = malloc(length + 1);
        assert(msg);
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
}
