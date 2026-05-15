#include "client.h"
#include "network/socket.h"
#include "config.h"
#include "game_state.h"
#include "message_parser.h"
#include "binary_aoi_decoder.h"
#include "serial.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <raylib.h>

typedef struct {
    WebSocketClient ws_client;
    conn_stats stats;
} client_ctx;

static client_ctx client_state = {0};

// Forward declarations of event callbacks
static void on_websocket_open(void* ctx);
static void on_websocket_message(const uint8_t* data, uint32_t length, bool is_text, void* ctx);
static void on_websocket_error(void* ctx);
static void on_websocket_close(int code, const char* reason, void* ctx);

// Initialize the client subsystem
bool connection_open(void) {
    // Setup WebSocket event handlers
    WebSocketHandlers callbacks = {
        .on_open_cb = on_websocket_open,
        .on_message_cb = on_websocket_message,
        .on_error_cb = on_websocket_error,
        .on_close_cb = on_websocket_close,
    };

    // Initialize WebSocket connection
    bool is_open = ws_open(&client_state.ws_client, WS_URL, &client_state, callbacks);
    if (!is_open) {
        fprintf(stderr, "[ERROR] Failed to initialize WebSocket connection\n");
        return false;
    }

    return true;
}

void connection_close(void) {
    ws_close(&client_state.ws_client);
}

bool connection_is_open(void) {
    return ws_is_open(&client_state.ws_client);
}

// Send a binary frame to the server
bool network_send_binary(const uint8_t* data, uint16_t len) {
    assert(data && len > 0);
    if (!connection_is_open()) return false;
    bool ok = ws_send_binary(&client_state.ws_client, data, len);
    client_state.stats.bytes_up += ok ? len : 0;
    return ok;
}

conn_stats connection_get_stats(void) {
    return client_state.stats;
}

bool network_send_event_tap(Vector2 grid) {
    BinWriter w;
    uplink_player_action(&w, grid.x, grid.y);
    return network_send_binary(w.buf, w.pos);
}

bool network_send_chat(const char* to_id, const char* text) {
    assert(to_id && text);
    BinWriter w;
    uplink_chat(&w, to_id, text);
    return network_send_binary(w.buf, w.pos);
}

// ============================================================================
// WebSocket Event Callbacks (Event-Driven Architecture)
// ============================================================================
static void on_websocket_open(void* ctx) {
    client_ctx* client_st = ctx;
    client_st->ws_client.connected = true;  // Update connection status

    // Send binary handshake
    BinWriter w;
    uplink_handshake(&w, "cyberia-mmo", "1.0.0");
    network_send_binary(w.buf, w.pos);
}

static void on_websocket_message(const uint8_t* data, uint32_t length, bool is_text, void* ctx) {
    client_ctx* client_st = ctx;
    if (!data || length <= 0 || !client_st) {
        return;
    }

    client_st->stats.bytes_down += length;      // Track downloaded bytes

    if (length >= MAX_MESSAGE_SIZE) {
        fprintf(stderr, "[WARN] Message too large (%d bytes), dropping\n", length);
        return;
    }

    if (is_text)
    {
        // Text message → JSON parser (init_data, skill_item_ids, etc.)
        char msg[MAX_MESSAGE_SIZE];
        memcpy(msg, (const char*)data, length);
        msg[length] = '\0';

        if (!message_parser_parse(msg)) {
            fprintf(stderr, "[ERROR] Failed to process message\n");
        }
    }
    else
    {
        // Binary message → binary AOI decoder
        if (0 != binary_aoi_process(data, (size_t)length)) {
            fprintf(stderr, "[ERROR] Failed to process binary AOI message\n");
        }
    }
}

static void on_websocket_error(void* ctx) {
    fprintf(stderr, "[ERROR] WebSocket error occurred\n");
}

static void on_websocket_close(int code, const char* reason, void* ctx) {
    // Only log unexpected closures
    if (code != 1000) {
        fprintf(stderr, "[WARN] WebSocket closed unexpectedly (code: %d, reason: %s)\n", code, reason ? reason : "none");
    }
}
