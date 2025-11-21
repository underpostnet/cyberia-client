#include "client.h"
#include "network.h"
#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(PLATFORM_WEB)
    #include <emscripten/emscripten.h>
#endif

// Maximum message buffer size
#define MAX_MESSAGE_SIZE 8192

// Global client state
static struct {
    WebSocketClient ws_client;
    WebSocketHandlers handlers;
    int initialized;
    char last_message[MAX_MESSAGE_SIZE];
    int message_count;
} client_state = {0};

// Forward declarations of event callbacks
static void on_websocket_open(void* user_data);
static void on_websocket_message(const char* data, int length, void* user_data);
static void on_websocket_error(void* user_data);
static void on_websocket_close(int code, const char* reason, void* user_data);

// Initialize the client subsystem
int client_init(void) {
    if (client_state.initialized) {
        printf("[CLIENT] Already initialized\n");
        return 0;
    }

    // Initialize client state
    memset(&client_state, 0, sizeof(client_state));
    strncpy(client_state.last_message, "No message received yet", MAX_MESSAGE_SIZE - 1);
    client_state.message_count = 0;

    // Setup WebSocket event handlers
    client_state.handlers.on_open = on_websocket_open;
    client_state.handlers.on_message = on_websocket_message;
    client_state.handlers.on_error = on_websocket_error;
    client_state.handlers.on_close = on_websocket_close;
    client_state.handlers.user_data = &client_state;

    // Initialize WebSocket connection
    printf("[CLIENT] Connecting to %s\n", WS_URL);
    int result = ws_init(&client_state.ws_client, WS_URL, &client_state.handlers);

    if (result != 0) {
        printf("[CLIENT] Failed to initialize WebSocket connection\n");
        return -1;
    }

    client_state.initialized = 1;
    printf("[CLIENT] Initialization complete\n");

    return 0;
}

// Client main update (processes events)
void client_update(void) {
    if (!client_state.initialized) {
        return;
    }

    // WebSocket events are handled asynchronously by Emscripten
    // This function can be used for periodic tasks if needed
}

// Client cleanup
void client_cleanup(void) {
    if (!client_state.initialized) {
        return;
    }

    printf("[CLIENT] Shutting down...\n");
    ws_close(&client_state.ws_client);
    client_state.initialized = 0;
    printf("[CLIENT] Cleanup complete\n");
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
    if (!client_state.initialized) {
        printf("[CLIENT] Cannot send - not initialized\n");
        return -1;
    }

    if (!client_is_connected()) {
        printf("[CLIENT] Cannot send - not connected\n");
        return -1;
    }

    int length = strlen(message);
    printf("[CLIENT] Sending message (%d bytes): %s\n", length, message);
    return ws_send(&client_state.ws_client, message, length);
}

// Get the last received message
const char* client_get_last_message(void) {
    return client_state.last_message;
}

// ============================================================================
// WebSocket Event Callbacks (Event-Driven Architecture)
// ============================================================================

// Called when WebSocket connection is opened
static void on_websocket_open(void* user_data) {
    printf("[CLIENT] WebSocket connection established\n");

#if defined(PLATFORM_WEB)
    // Log to JavaScript console for debugging
    EM_ASM(
        console.log('%c[WS] on_open', 'color: #00ff00; font-weight: bold');
        console.log('[WS] WebSocket connection successfully established');
        console.log('[WS] Ready to send and receive messages');
    );
#endif

    // Update connection status
    client_state.ws_client.connected = 1;

    // Send initial handshake message
    const char* handshake = "{\"type\":\"handshake\",\"client\":\"cyberia-immersive\",\"version\":\"1.0.0\"}";
    client_send(handshake);
}

// Called when WebSocket message is received
static void on_websocket_message(const char* data, int length, void* user_data) {
    if (!data || length <= 0) {
        return;
    }

    client_state.message_count++;

     // printf("[CLIENT] Message received (%d bytes) [count: %d]\n",
     //       length, client_state.message_count);

    // Store the message
    int copy_length = (length < MAX_MESSAGE_SIZE - 1) ? length : MAX_MESSAGE_SIZE - 1;
    memcpy(client_state.last_message, data, copy_length);
    client_state.last_message[copy_length] = '\0';

#if defined(PLATFORM_WEB)
    // Log to JavaScript console with full message content
    EM_ASM({
        // console.log('%c[WS] on_message', 'color: #00aaff; font-weight: bold');
        // console.log('[WS] Message #' + $0 + ' received (' + $1 + ' bytes)');
        var message = UTF8ToString($2, $1);
        // console.log('[WS] Content:', message);
        try {
            var json = JSON.parse(message);
            // console.log('[WS] Parsed JSON:', json);
        } catch(e) {
            // console.log('[WS] (Not valid JSON or binary data)');
        }
    }, client_state.message_count, length, data);
#endif
}

// Called when WebSocket error occurs
static void on_websocket_error(void* user_data) {
    printf("[CLIENT] WebSocket error occurred\n");

#if defined(PLATFORM_WEB)
    // Log to JavaScript console
    EM_ASM(
        console.error('%c[WS] on_error', 'color: #ff0000; font-weight: bold');
        console.error('[WS] WebSocket error occurred');
        console.error('[WS] Connection may have failed or been interrupted');
    );
#endif

    // Mark as disconnected
    client_state.ws_client.connected = 0;
}

// Called when WebSocket connection is closed
static void on_websocket_close(int code, const char* reason, void* user_data) {
    printf("[CLIENT] WebSocket closed (code: %d, reason: %s)\n",
           code, reason ? reason : "none");

#if defined(PLATFORM_WEB)
    // Log to JavaScript console with details
    EM_ASM({
        console.log('%c[WS] on_close', 'color: #ffaa00; font-weight: bold');
        console.log('[WS] WebSocket connection closed');
        console.log('[WS] Close code:', $0);
        var reason = $1 ? UTF8ToString($1) : 'No reason provided';
        console.log('[WS] Close reason:', reason);
        if ($0 === 1000) console.log('[WS] Code meaning: Normal Closure');
        else if ($0 === 1001) console.log('[WS] Code meaning: Going Away');
        else if ($0 === 1002) console.log('[WS] Code meaning: Protocol Error');
        else if ($0 === 1006) console.log('[WS] Code meaning: Abnormal Closure');
        else if ($0 === 1011) console.log('[WS] Code meaning: Internal Server Error');
    }, code, reason);
#endif

    // Mark as disconnected
    client_state.ws_client.connected = 0;
}
