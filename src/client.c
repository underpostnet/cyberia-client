#include "client.h"
#include "network/socket.h"
#include "config.h"
#include "game_state.h"
#include "message_parser.h"
#include "binary_aoi_decoder.h"
#include "serial.h"
#include <stdio.h>
#include <string.h>
#include <assert.h>

typedef struct {
    WebSocketClient ws_client;
    int initialized;
    char last_message[MAX_MESSAGE_SIZE];
    int message_count;
    size_t bytes_downloaded;
    size_t bytes_uploaded;
} client_ctx;

static client_ctx client_state = {0};

// Forward declarations of event callbacks
static void on_websocket_open(void* ctx);
static void on_websocket_message(const char* data, uint32_t length, bool is_binary, void* ctx);
static void on_websocket_error(void* ctx);
static void on_websocket_close(int code, const char* reason, void* ctx);

// Initialize the client subsystem
int client_init(void) {
    memset(&client_state, 0, sizeof(client_state));
    strncpy(client_state.last_message, "No message received yet", MAX_MESSAGE_SIZE - 1);
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

// Send a message to the server
int client_send(const char* message) {
    if (!client_state.initialized || !client_is_connected()) {
        return -1;
    }

    size_t length = strlen(message);
    bool result = ws_send(&client_state.ws_client, message);
    if (result) {
        client_state.bytes_uploaded += length;
    }

    return result ? 0 : -1;
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

// ============================================================================
// WebSocket Event Callbacks (Event-Driven Architecture)
// ============================================================================
static void on_websocket_open(void* ctx) {
    client_ctx* client_st = ctx;
    client_st->ws_client.connected = true;  // Update connection status

    // Send initial handshake message
    char* handshake = serial_create_handshake("cyberia-mmo", "1.0.0");
    if (handshake) {
        client_send(handshake);
        free(handshake);
    } else {
        // Fallback to simple handshake
        const char* fallback = "{\"type\":\"handshake\",\"client\":\"cyberia-mmo\",\"version\":\"1.0.0\"}";
        client_send(fallback);
    }
}

static void on_websocket_message(const char* data, uint32_t length, bool is_binary, void* ctx) {
    if (!data) {
        return;
    }

    client_ctx* client_st = ctx;
    client_st->message_count++;
    client_st->bytes_downloaded += length;      // Track downloaded bytes

    if (is_binary) {
        // Binary message → binary AOI decoder
        if (binary_aoi_process((const uint8_t*)data, (size_t)length) != 0) {
            fprintf(stderr, "[ERROR] Failed to process binary AOI message\n");
        }
    } else {
        // Text message → JSON parser (init_data, skill_item_ids, etc.)
        if (length >= MAX_MESSAGE_SIZE) {
            fprintf(stderr, "[WARN] Message too large (%d bytes), truncating\n", length);
            length = MAX_MESSAGE_SIZE - 1;
        }

        memcpy(client_st->last_message, data, length);
        client_st->last_message[length] = '\0';

        if (message_parser_process(client_st->last_message) != 0) {
            fprintf(stderr, "[ERROR] Failed to process message\n");
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
