#include "network.h"
#include <stdio.h>

#include <emscripten/websocket.h>

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
        fprintf(stderr, "[ERROR] Invalid parameters for WebSocket initialization\n");
        return -1;
    }

    // Check if WebSocket is supported in this environment
    if (!emscripten_websocket_is_supported()) {
        fprintf(stderr, "[ERROR] WebSocket is not supported in this environment\n");
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
        fprintf(stderr, "[ERROR] Failed to create WebSocket\n");
        return -1;
    }

    client->socket = socket;
    client->connected = 0;

    // Register event callbacks
    emscripten_websocket_set_onopen_callback(socket, handlers, on_open_internal);
    emscripten_websocket_set_onmessage_callback(socket, handlers, on_message_internal);
    emscripten_websocket_set_onerror_callback(socket, handlers, on_error_internal);
    emscripten_websocket_set_onclose_callback(socket, handlers, on_close_internal);

    return 0;
}

// Send message through WebSocket
int ws_send(WebSocketClient* client, const char* data, int length) {
    if (!client || !data || length <= 0) {
        return -1;
    }

    if (!client->connected) {
        return -1;
    }

    // Send as text message
    EMSCRIPTEN_RESULT result = emscripten_websocket_send_utf8_text(client->socket, data);
    if (result != EMSCRIPTEN_RESULT_SUCCESS) {
        fprintf(stderr, "[ERROR] WebSocket send failed with error code: %d\n", result);
        return -1;
    }

    return 0;
}

// Close WebSocket connection
void ws_close(WebSocketClient* client) {
    if (!client) {
        return;
    }

    if (client->socket > 0) {
        emscripten_websocket_close(client->socket, 1000, "Client initiated closure");
        emscripten_websocket_delete(client->socket);
        client->socket = 0;
        client->connected = 0;
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
    (void)eventType;
    (void)event;
    WebSocketHandlers* handlers = (WebSocketHandlers*)userData;

    // Call user callback
    if (handlers && handlers->on_open) {
        handlers->on_open(handlers->user_data);
    }

    return EM_TRUE;
}

// Internal event handler: onmessage
static EM_BOOL on_message_internal(int eventType, const EmscriptenWebSocketMessageEvent* event, void* userData) {
    (void)eventType;
    WebSocketHandlers* handlers = (WebSocketHandlers*)userData;



    // Call user callback with message data
    if (handlers && handlers->on_message) {
        handlers->on_message((const char*)event->data, event->numBytes, handlers->user_data);
    }

    return EM_TRUE;
}

// Internal event handler: onerror
static EM_BOOL on_error_internal(int eventType, const EmscriptenWebSocketErrorEvent* event, void* userData) {
    (void)eventType;
    (void)event;
    WebSocketHandlers* handlers = (WebSocketHandlers*)userData;

    fprintf(stderr, "[ERROR] WebSocket error occurred\n");

    // Call user callback
    if (handlers && handlers->on_error) {
        handlers->on_error(handlers->user_data);
    }

    return EM_TRUE;
}

// Internal event handler: onclose
static EM_BOOL on_close_internal(int eventType, const EmscriptenWebSocketCloseEvent* event, void* userData) {
    (void)eventType;
    WebSocketHandlers* handlers = (WebSocketHandlers*)userData;

    // Only log unexpected closures
    if (!event->wasClean || event->code != 1000) {
        fprintf(stderr, "[WARN] WebSocket closed unexpectedly: code=%d, reason='%s'\n",
               event->code, event->reason);
    }

    // Call user callback
    if (handlers && handlers->on_close) {
        handlers->on_close(event->code, event->reason, handlers->user_data);
    }

    return EM_TRUE;
}
