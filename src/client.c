#include "client.h"
#include "network/socket.h"
#include "config.h"
#include "game_state.h"
#include "message_parser.h"
#include "binary_aoi_decoder.h"
#include "serial.h"
#include <cJSON.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

typedef struct {
    WebSocketClient ws_client;
    int initialized;
    int message_count;
    size_t bytes_downloaded;
    size_t bytes_uploaded;
} client_ctx;

static client_ctx client_state = {0};

// Forward declarations of event callbacks
static void on_websocket_open(void* ctx);
static void on_websocket_message(const uint8_t* data, uint32_t length, bool is_text, void* ctx);
static void on_websocket_error(void* ctx);
static void on_websocket_close(int code, const char* reason, void* ctx);

// Initialize the client subsystem
int client_init(void) {
    memset(&client_state, 0, sizeof(client_state));
    client_state.message_count = 0;
    client_state.bytes_downloaded = 0;
    client_state.bytes_uploaded = 0;

    // Setup WebSocket event handlers
    WebSocketHandlers callbacks = {
        .on_open_cb = on_websocket_open,
        .on_message_cb = on_websocket_message,
        .on_error_cb = on_websocket_error,
        .on_close_cb = on_websocket_close,
    };

    // Initialize WebSocket connection
    bool result = ws_init(&client_state.ws_client, WS_URL, &client_state, callbacks);
    if (!result) {
        fprintf(stderr, "[ERROR] Failed to initialize WebSocket connection\n");
        return -1;
    }

    client_state.initialized = 1;
    return 0;
}

// Client cleanup
void client_cleanup(void) {
    if (!client_state.initialized) {
        return;
    }

    ws_close(&client_state.ws_client);
    client_state.initialized = 0;
}

// Check if client is connected
int client_is_connected(void) {
    if (!client_state.initialized) {
        return 0;
    }
    return (int)ws_is_connected(&client_state.ws_client);
}

// Send a raw string message to the server
int client_send_msg(const char* msg) {
    assert(msg);
    if (!client_state.initialized || !client_is_connected()) {
        return -1;
    }

    bool result = ws_send(&client_state.ws_client, msg);
    if (result) {
        client_state.bytes_uploaded += strlen(msg);
    }

    return result ? 0 : -1;
}

int client_send(cJSON* json_obj) {
    assert(json_obj);
    char* str = cJSON_PrintUnformatted(json_obj);
    return client_send_msg(str);
}

// Get network statistics
void client_get_network_stats(size_t* bytes_downloaded, size_t* bytes_uploaded) {
    if (bytes_downloaded) {
        *bytes_downloaded = client_state.bytes_downloaded;
    }
    if (bytes_uploaded) {
        *bytes_uploaded = client_state.bytes_uploaded;
    }
}

int client_send_tap(float grid_x, float grid_y) {
    cJSON* json = cJSON_CreateObject();
    serialize_player_action(json, grid_x, grid_y);
    int result = client_send(json);
    cJSON_Delete(json);
    return result;
}

// ============================================================================
// WebSocket Event Callbacks (Event-Driven Architecture)
// ============================================================================
static void on_websocket_open(void* ctx) {
    client_ctx* client_st = ctx;
    client_st->ws_client.connected = true;  // Update connection status

    // Send initial handshake message
    cJSON* handshake = cJSON_CreateObject();
    serialize_handshake(handshake, "cyberia-mmo", "1.0.0");
    client_send(handshake);
    cJSON_Delete(handshake);
}

static void on_websocket_message(const uint8_t* data, uint32_t length, bool is_text, void* ctx) {
    client_ctx* client_st = ctx;
    if (!data || length <= 0 || !client_st) {
        return;
    }

    client_st->message_count++;
    client_st->bytes_downloaded += length;      // Track downloaded bytes

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
