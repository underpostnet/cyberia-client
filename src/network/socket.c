#include "socket.h"
#include <stdio.h>
#include <assert.h>

#include <emscripten/websocket.h>

// Forward declarations of internal event handlers
static EM_BOOL ws_onopen_internal(int eventType, const EmscriptenWebSocketOpenEvent* event, void* userData);
static EM_BOOL ws_onmessage_internal(int eventType, const EmscriptenWebSocketMessageEvent* event, void* userData);
static EM_BOOL ws_onerror_internal(int eventType, const EmscriptenWebSocketErrorEvent* event, void* userData);
static EM_BOOL ws_onclose_internal(int eventType, const EmscriptenWebSocketCloseEvent* event, void* userData);

// Initialize WebSocket client with event handlers
bool ws_open(WebSocketClient* ws_client, const char* url, void* user_ctx, WebSocketHandlers callbacks) {
    assert(ws_client && url && user_ctx);

    // Check if WebSocket is supported in this environment
    if (!emscripten_websocket_is_supported()) {
        fprintf(stderr, "[ERROR] WebSocket is not supported in this environment\n");
        return false;
    }

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
        return false;
    }

    ws_client->socket = socket;
    ws_client->connected = false;
    ws_client->user_ctx = user_ctx;
    ws_client->callbacks = callbacks;

    // Register event callbacks; userData is forwarded verbatim to user callbacks
    emscripten_websocket_set_onopen_callback(socket, ws_client, ws_onopen_internal);
    emscripten_websocket_set_onmessage_callback(socket, ws_client, ws_onmessage_internal);
    emscripten_websocket_set_onerror_callback(socket, ws_client, ws_onerror_internal);
    emscripten_websocket_set_onclose_callback(socket, ws_client, ws_onclose_internal);

    return true;
}

// Send message through WebSocket
bool ws_send_str(WebSocketClient* ws_client, const char* data) {
    if (!ws_client || !ws_client->connected || !data) {
        return false;
    }

    // Send as text message
    EMSCRIPTEN_RESULT result = emscripten_websocket_send_utf8_text(ws_client->socket, data);
    if (result != EMSCRIPTEN_RESULT_SUCCESS) {
        fprintf(stderr, "[ERROR] WebSocket send failed with error code: %d\n", result);
        return false;
    }

    return true;
}

bool ws_send_binary(WebSocketClient* ws_client, const uint8_t* data, size_t len) {
    if (!ws_client || !ws_client->connected || !data || len == 0) {
        return false;
    }
    EMSCRIPTEN_RESULT result = emscripten_websocket_send_binary(ws_client->socket, (void*)data, (uint32_t)len);
    if (result != EMSCRIPTEN_RESULT_SUCCESS) {
        fprintf(stderr, "[ERROR] WebSocket binary send failed: %d\n", result);
        return false;
    }
    return true;
}

void ws_close(WebSocketClient* ws_client) {
    if (!ws_client) {
        return;
    }

    if (ws_client->socket > 0) {
        emscripten_websocket_close(ws_client->socket, 1000, "Client initiated closure");
        emscripten_websocket_delete(ws_client->socket);
    }
    // NOTE: maybe we should check close and delete results before clearing
    ws_client->socket = 0;
    ws_client->connected = false;
}

bool ws_is_open(WebSocketClient* ws_client) {
    return ws_client && ws_client->connected;
}

// ============================================================================
// Internal Event Handlers (Emscripten WebSocket Callbacks)
// ============================================================================
static EM_BOOL ws_onopen_internal(int eventType, const EmscriptenWebSocketOpenEvent* event, void* userData) {
    (void)eventType; (void)event;
    printf("[WS] onopen fired — socket=%d\n", event ? event->socket : -1);
    WebSocketClient* socket_ctx = userData;
    if (socket_ctx && socket_ctx->callbacks.on_open_cb) {
        socket_ctx->callbacks.on_open_cb(socket_ctx->user_ctx);
        return EM_TRUE;
    }
    return EM_FALSE;
}

static EM_BOOL ws_onmessage_internal(int eventType, const EmscriptenWebSocketMessageEvent* event, void* userData) {
    WebSocketClient* socket_ctx = userData;
    if (socket_ctx && socket_ctx->callbacks.on_message_cb) {
        socket_ctx->callbacks.on_message_cb(event->data, event->numBytes, event->isText, socket_ctx->user_ctx);
        return EM_TRUE;
    }
    return EM_FALSE;
}

static EM_BOOL ws_onerror_internal(int eventType, const EmscriptenWebSocketErrorEvent* event, void* userData) {
    fprintf(stderr, "[ERROR] WebSocket error occurred\n");
    WebSocketClient* socket_ctx = userData;
    if(socket_ctx) {
        if (socket_ctx->callbacks.on_error_cb) {
            socket_ctx->callbacks.on_error_cb(socket_ctx->user_ctx);
        }
        ws_close(socket_ctx);
    }
    return EM_TRUE;
}

static EM_BOOL ws_onclose_internal(int eventType, const EmscriptenWebSocketCloseEvent* event, void* userData) {
    WebSocketClient* socket_ctx = userData;
    // Only log unexpected closures
    if (!(bool)event->wasClean || event->code != 1000) {
        fprintf(stderr, "[WARN] WebSocket closed unexpectedly: code=%d, reason='%s'\n", event->code, event->reason);
    }
    if(socket_ctx) {
        if (socket_ctx->callbacks.on_close_cb) {
            socket_ctx->callbacks.on_close_cb(event->code, event->reason, socket_ctx->user_ctx);
        }
        socket_ctx->socket = 0;
        socket_ctx->connected = false;
    }

    return EM_TRUE;
}
