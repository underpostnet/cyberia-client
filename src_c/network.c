#include "network.h"
#include "config.h"
#include <emscripten/websocket.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Global handlers storage (stored per socket)
static WebSocketHandlers* g_handlers = NULL;

// Forward declarations of internal event handlers
static EM_BOOL on_open_internal(int eventType, const EmscriptenWebSocketOpenEvent* event, void* userData);
static EM_BOOL on_message_internal(int eventType, const EmscriptenWebSocketMessageEvent* event, void* userData);
static EM_BOOL on_error_internal(int eventType, const EmscriptenWebSocketErrorEvent* event, void* userData);
static EM_BOOL on_close_internal(int eventType, const EmscriptenWebSocketCloseEvent* event, void* userData);

// Initialize WebSocket client with event handlers
int ws_init(WebSocketClient* client, const char* url, WebSocketHandlers* handlers) {
    if (!client || !url || !handlers) {
        printf("WebSocket init: invalid parameters\n");
        return -1;
    }

    // Check if WebSocket is supported
    if (!emscripten_websocket_is_supported()) {
        printf("WebSocket is not supported in this environment\n");
        return -1;
    }

    // Store handlers globally (in a real implementation, you'd use a hash map)
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
        printf("Failed to create WebSocket\n");
        return -1;
    }

    client->socket = socket;
    client->connected = 0;

    // Register event callbacks
    emscripten_websocket_set_onopen_callback(socket, handlers, on_open_internal);
    emscripten_websocket_set_onmessage_callback(socket, handlers, on_message_internal);
    emscripten_websocket_set_onerror_callback(socket, handlers, on_error_internal);
    emscripten_websocket_set_onclose_callback(socket, handlers, on_close_internal);

    printf("WebSocket initialized for URL: %s\n", url);
    return 0;
}

// Send message through WebSocket
int ws_send(WebSocketClient* client, const char* data, int length) {
    if (!client || !data || length <= 0) {
        printf("WebSocket send: invalid parameters\n");
        return -1;
    }

    if (!client->connected) {
        printf("WebSocket send: not connected\n");
        return -1;
    }

    // Send as text message
    EMSCRIPTEN_RESULT result = emscripten_websocket_send_utf8_text(client->socket, data);
    if (result != EMSCRIPTEN_RESULT_SUCCESS) {
        printf("WebSocket send failed with error: %d\n", result);
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
        emscripten_websocket_close(client->socket, 1000, "Client closing connection");
        emscripten_websocket_delete(client->socket);
        client->socket = 0;
        client->connected = 0;
        printf("WebSocket connection closed\n");
    }
}

// Check if WebSocket is connected
int ws_is_connected(WebSocketClient* client) {
    if (!client) {
        return 0;
    }
    return client->connected;
}

// Internal event handler: onopen
static EM_BOOL on_open_internal(int eventType, const EmscriptenWebSocketOpenEvent* event, void* userData) {
    WebSocketHandlers* handlers = (WebSocketHandlers*)userData;
    
    printf("WebSocket opened\n");

    // Update connection status
    if (g_handlers) {
        // Find the client (in a real implementation, you'd look this up properly)
        // For now, we'll just mark it as connected via the callback
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
        printf("WebSocket received text message (%d bytes)\n", event->numBytes);
        
        if (handlers && handlers->on_message) {
            handlers->on_message((const char*)event->data, event->numBytes, handlers->user_data);
        }
    } else {
        // Binary message received
        printf("WebSocket received binary message (%d bytes)\n", event->numBytes);
        
        if (handlers && handlers->on_message) {
            handlers->on_message((const char*)event->data, event->numBytes, handlers->user_data);
        }
    }

    return EM_TRUE;
}

// Internal event handler: onerror
static EM_BOOL on_error_internal(int eventType, const EmscriptenWebSocketErrorEvent* event, void* userData) {
    WebSocketHandlers* handlers = (WebSocketHandlers*)userData;
    
    printf("WebSocket error occurred\n");

    // Call user callback
    if (handlers && handlers->on_error) {
        handlers->on_error(handlers->user_data);
    }

    return EM_TRUE;
}

// Internal event handler: onclose
static EM_BOOL on_close_internal(int eventType, const EmscriptenWebSocketCloseEvent* event, void* userData) {
    WebSocketHandlers* handlers = (WebSocketHandlers*)userData;
    
    printf("WebSocket closed: code=%d, reason='%s', wasClean=%d\n", 
           event->code, event->reason, event->wasClean);

    // Call user callback
    if (handlers && handlers->on_close) {
        handlers->on_close(event->code, event->reason, handlers->user_data);
    }

    return EM_TRUE;
}