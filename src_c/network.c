#include "network.h"
#include "config.h"
#include <emscripten/websocket.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Global handlers storage (simple approach for single connection)
static WebSocketHandlers* g_handlers = NULL;

// Forward declarations of internal event handlers
static EM_BOOL on_open_internal(int eventType, const EmscriptenWebSocketOpenEvent* event, void* userData);
static EM_BOOL on_message_internal(int eventType, const EmscriptenWebSocketMessageEvent* event, void* userData);
static EM_BOOL on_error_internal(int eventType, const EmscriptenWebSocketErrorEvent* event, void* userData);
static EM_BOOL on_close_internal(int eventType, const EmscriptenWebSocketCloseEvent* event, void* userData);

// Initialize WebSocket client with event handlers
int ws_init(WebSocketClient* client, const char* url, WebSocketHandlers* handlers) {
    if (!client || !url || !handlers) {
        printf("[NETWORK] Invalid parameters for WebSocket initialization\n");
        return -1;
    }

    // Check if WebSocket is supported in this environment
    if (!emscripten_websocket_is_supported()) {
        printf("[NETWORK] WebSocket is not supported in this environment\n");
        return -1;
    }

    // Store handlers globally (for single connection scenario)
    g_handlers = handlers;

    // Create WebSocket attributes
    EmscriptenWebSocketCreateAttributes attrs;
    emscripten_websocket_init_create_attributes(&attrs);
    attrs.url = url;
    attrs.protocols = NULL;
    attrs.createOnMainThread = EM_TRUE;

    // Create WebSocket
    EMSCRIPTEN_WEBSOCKET_T socket = emscripten_websocket_new(&attrs);
    if (socket <= 0) {
        printf("[NETWORK] Failed to create WebSocket\n");
        return -1;
    }

    client->socket = socket;
    client->connected = 0;

    // Register event callbacks
    emscripten_websocket_set_onopen_callback(socket, handlers, on_open_internal);
    emscripten_websocket_set_onmessage_callback(socket, handlers, on_message_internal);
    emscripten_websocket_set_onerror_callback(socket, handlers, on_error_internal);
    emscripten_websocket_set_onclose_callback(socket, handlers, on_close_internal);

    printf("[NETWORK] WebSocket initialized for URL: %s\n", url);
    return 0;
}

// Send message through WebSocket
int ws_send(WebSocketClient* client, const char* data, int length) {
    if (!client || !data || length <= 0) {
        printf("[NETWORK] Invalid parameters for send operation\n");
        return -1;
    }

    if (!client->connected) {
        printf("[NETWORK] Cannot send - not connected\n");
        return -1;
    }

    // Send as text message
    EMSCRIPTEN_RESULT result = emscripten_websocket_send_utf8_text(client->socket, data);
    if (result != EMSCRIPTEN_RESULT_SUCCESS) {
        printf("[NETWORK] Send failed with error code: %d\n", result);
        return -1;
    }

    printf("[NETWORK] Message sent successfully (%d bytes)\n", length);
    return 0;
}

// Close WebSocket connection
void ws_close(WebSocketClient* client) {
    if (!client) {
        return;
    }

    if (client->socket > 0) {
        printf("[NETWORK] Closing WebSocket connection\n");
        emscripten_websocket_close(client->socket, 1000, "Client initiated closure");
        emscripten_websocket_delete(client->socket);
        client->socket = 0;
        client->connected = 0;
        printf("[NETWORK] WebSocket closed\n");
    }
}

// Check if WebSocket is connected
int ws_is_connected(WebSocketClient* client) {
    if (!client) {
        return 0;
    }
    return client->connected;
}

// ============================================================================
// Internal Event Handlers (Emscripten WebSocket Callbacks)
// ============================================================================

// Internal event handler: onopen
static EM_BOOL on_open_internal(int eventType, const EmscriptenWebSocketOpenEvent* event, void* userData) {
    WebSocketHandlers* handlers = (WebSocketHandlers*)userData;
    
    printf("[NETWORK] WebSocket connection opened\n");

    // Update connection status through global handlers
    if (g_handlers) {
        // Mark as connected (will be done in the user callback)
    }

    // Call user callback
    if (handlers && handlers->on_open) {
        handlers->on_open(handlers->user_data);
    }

    return EM_TRUE;
}

// Internal event handler: onmessage
static EM_BOOL on_message_internal(int eventType, const EmscriptenWebSocketMessageEvent* event, void* userData) {
    WebSocketHandlers* handlers = (WebSocketHandlers*)userData;
    
    if (event->isText) {
        // Text message received
        printf("[NETWORK] Received text message (%d bytes)\n", event->numBytes);
    } else {
        // Binary message received
        printf("[NETWORK] Received binary message (%d bytes)\n", event->numBytes);
    }

    // Call user callback with message data
    if (handlers && handlers->on_message) {
        handlers->on_message((const char*)event->data, event->numBytes, handlers->user_data);
    }

    return EM_TRUE;
}

// Internal event handler: onerror
static EM_BOOL on_error_internal(int eventType, const EmscriptenWebSocketErrorEvent* event, void* userData) {
    WebSocketHandlers* handlers = (WebSocketHandlers*)userData;
    
    printf("[NETWORK] WebSocket error occurred\n");

    // Call user callback
    if (handlers && handlers->on_error) {
        handlers->on_error(handlers->user_data);
    }

    return EM_TRUE;
}

// Internal event handler: onclose
static EM_BOOL on_close_internal(int eventType, const EmscriptenWebSocketCloseEvent* event, void* userData) {
    WebSocketHandlers* handlers = (WebSocketHandlers*)userData;
    
    printf("[NETWORK] WebSocket closed: code=%d, reason='%s', wasClean=%d\n", 
           event->code, event->reason, event->wasClean);

    // Call user callback
    if (handlers && handlers->on_close) {
        handlers->on_close(event->code, event->reason, handlers->user_data);
    }

    return EM_TRUE;
}