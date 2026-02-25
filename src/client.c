#include "client.h"
#include "network.h"
#include "config.h"
#include "game_state.h"
#include "message_parser.h"
#include "serial.h"
#include <stdio.h>
#include <string.h>

#include <emscripten/emscripten.h>

// Global client state
static struct {
    WebSocketClient ws_client;
    WebSocketHandlers handlers;
    int initialized;
    char last_message[MAX_MESSAGE_SIZE];
    int message_count;
    size_t bytes_downloaded;
    size_t bytes_uploaded;
} client_state = {0};

// Forward declarations of event callbacks
static void on_websocket_open(void* user_data);
static void on_websocket_message(const char* data, int length, void* user_data);
static void on_websocket_error(void* user_data);
static void on_websocket_close(int code, const char* reason, void* user_data);

// Initialize the client subsystem
int client_init(void) {
    // Initialize client state
    memset(&client_state, 0, sizeof(client_state));
    strncpy(client_state.last_message, "No message received yet", MAX_MESSAGE_SIZE - 1);
    client_state.message_count = 0;
    client_state.bytes_downloaded = 0;
    client_state.bytes_uploaded = 0;

    // Setup WebSocket event handlers
    client_state.handlers.on_open = on_websocket_open;
    client_state.handlers.on_message = on_websocket_message;
    client_state.handlers.on_error = on_websocket_error;
    client_state.handlers.on_close = on_websocket_close;
    client_state.handlers.user_data = &client_state;

    // Initialize WebSocket connection
    int result = ws_init(&client_state.ws_client, WS_URL, &client_state.handlers);

    if (result != 0) {
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
    return ws_is_connected(&client_state.ws_client);
}

// Send a message to the server
int client_send(const char* message) {
    if (!client_state.initialized || !client_is_connected()) {
        return -1;
    }

    int length = strlen(message);
    int result = ws_send(&client_state.ws_client, message, length);
    if (result == 0) {
        client_state.bytes_uploaded += length;

        // Update dev UI with network stats
        g_game_state.upload_size_bytes = client_state.bytes_uploaded;
    }

    return result;
}

// Get the last received message
const char* client_get_last_message(void) {
    return client_state.last_message;
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

// Called when WebSocket connection is opened
static void on_websocket_open(void* user_data) {
    (void)user_data;
    // Update connection status
    client_state.ws_client.connected = 1;

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

// Called when WebSocket message is received
static void on_websocket_message(const char* data, int length, void* user_data) {
    (void)user_data;
    if (!data || length <= 0) {
        return;
    }

    client_state.message_count++;

    // Track downloaded bytes
    client_state.bytes_downloaded += length;

    // Update dev UI with network stats
    g_game_state.download_size_bytes = client_state.bytes_downloaded;

    // Check if message fits in buffer
    if (length >= MAX_MESSAGE_SIZE) {
        fprintf(stderr, "[WARN] Message too large (%d bytes), truncating\n", length);
        length = MAX_MESSAGE_SIZE - 1;
    }

    // Store the message with proper null termination
    memcpy(client_state.last_message, data, length);
    client_state.last_message[length] = '\0';

    // Process the message through the game state system
    if (message_parser_process(client_state.last_message) != 0) {
        fprintf(stderr, "[ERROR] Failed to process message\n");
    }
}

// Called when WebSocket error occurs
static void on_websocket_error(void* user_data) {
    (void)user_data;
    fprintf(stderr, "[ERROR] WebSocket error occurred\n");
    client_state.ws_client.connected = 0;
}

// Called when WebSocket connection is closed
static void on_websocket_close(int code, const char* reason, void* user_data) {
    (void)user_data;
    // Only log unexpected closures
    if (code != 1000) {
        fprintf(stderr, "[WARN] WebSocket closed unexpectedly (code: %d, reason: %s)\n",
                code, reason ? reason : "none");
    }

    client_state.ws_client.connected = 0;
}
