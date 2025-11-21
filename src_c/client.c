#include "client.h"
#include "network.h"
#include "config.h"
#include "render.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Maximum message buffer size
#define MAX_MESSAGE_SIZE 8192

// Global client state
static struct {
    WebSocketClient ws_client;
    WebSocketHandlers handlers;
    int initialized;
    char last_message[MAX_MESSAGE_SIZE];
    int message_updated;
} client_state = {0};

// Forward declarations of event callbacks
static void on_websocket_open(void* user_data);
static void on_websocket_message(const char* data, int length, void* user_data);
static void on_websocket_error(void* user_data);
static void on_websocket_close(int code, const char* reason, void* user_data);

// Initialize the client subsystem
int client_init(void) {
    if (client_state.initialized) {
        printf("Client: already initialized\n");
        return 0;
    }

    // Initialize client state
    memset(&client_state, 0, sizeof(client_state));
    strncpy(client_state.last_message, "Waiting for data...", MAX_MESSAGE_SIZE - 1);

    // Setup WebSocket event handlers
    client_state.handlers.on_open = on_websocket_open;
    client_state.handlers.on_message = on_websocket_message;
    client_state.handlers.on_error = on_websocket_error;
    client_state.handlers.on_close = on_websocket_close;
    client_state.handlers.user_data = &client_state;

    // Initialize WebSocket connection
    printf("Client: connecting to %s\n", WS_URL);
    int result = ws_init(&client_state.ws_client, WS_URL, &client_state.handlers);
    
    if (result != 0) {
        printf("Client: failed to initialize WebSocket\n");
        return -1;
    }

    client_state.initialized = 1;
    printf("Client: initialized successfully\n");

    return 0;
}

// Client main update (processes events)
void client_update(void) {
    if (!client_state.initialized) {
        return;
    }

    // Check if we have a new message to display
    if (client_state.message_updated) {
        render_set_text(client_state.last_message);
        client_state.message_updated = 0;
    }

    // Additional periodic tasks could go here
    // For example: sending heartbeat, checking timeouts, etc.
}

// Client cleanup
void client_cleanup(void) {
    if (!client_state.initialized) {
        return;
    }

    printf("Client: shutting down...\n");
    ws_close(&client_state.ws_client);
    client_state.initialized = 0;
    printf("Client: cleanup complete\n");
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
        printf("Client: not initialized\n");
        return -1;
    }

    if (!client_is_connected()) {
        printf("Client: not connected\n");
        return -1;
    }

    int length = strlen(message);
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
    printf("Client: WebSocket connection established\n");
    
    // Update connection status
    client_state.ws_client.connected = 1;
    
    // Update display
    strncpy(client_state.last_message, "Connected to server!", MAX_MESSAGE_SIZE - 1);
    client_state.message_updated = 1;
    
    // Optionally send an initial message to the server
    const char* hello_msg = "{\"type\":\"hello\",\"client\":\"mmo-client\",\"version\":\"1.0\"}";
    client_send(hello_msg);
}

// Called when WebSocket message is received
static void on_websocket_message(const char* data, int length, void* user_data) {
    if (!data || length <= 0) {
        return;
    }

    printf("Client: received message (%d bytes)\n", length);

    // Ensure null-terminated string
    int copy_length = (length < MAX_MESSAGE_SIZE - 1) ? length : MAX_MESSAGE_SIZE - 1;
    memcpy(client_state.last_message, data, copy_length);
    client_state.last_message[copy_length] = '\0';

    // Mark message as updated for rendering
    client_state.message_updated = 1;

    // Log a preview of the message
    char preview[100];
    int preview_len = (length < 80) ? length : 80;
    memcpy(preview, data, preview_len);
    preview[preview_len] = '\0';
    printf("Client: message preview: %s%s\n", preview, (length > 80) ? "..." : "");
}

// Called when WebSocket error occurs
static void on_websocket_error(void* user_data) {
    printf("Client: WebSocket error occurred\n");
    
    // Update display with error message
    strncpy(client_state.last_message, 
            "WebSocket Error: Connection failed or lost", 
            MAX_MESSAGE_SIZE - 1);
    client_state.message_updated = 1;
    
    // Mark as disconnected
    client_state.ws_client.connected = 0;
}

// Called when WebSocket connection is closed
static void on_websocket_close(int code, const char* reason, void* user_data) {
    printf("Client: WebSocket closed (code: %d, reason: %s)\n", code, reason);
    
    // Update display with close information
    char close_msg[256];
    snprintf(close_msg, sizeof(close_msg), 
             "Connection Closed\nCode: %d\nReason: %s", 
             code, reason ? reason : "No reason provided");
    
    strncpy(client_state.last_message, close_msg, MAX_MESSAGE_SIZE - 1);
    client_state.last_message[MAX_MESSAGE_SIZE - 1] = '\0';
    client_state.message_updated = 1;
    
    // Mark as disconnected
    client_state.ws_client.connected = 0;
}