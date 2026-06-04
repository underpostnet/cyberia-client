#include "socket.h"
#include "util/log.h"
#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <stdint.h>

#include <emscripten/websocket.h>

// Forward declarations of internal event handlers
static EM_BOOL ws_onopen_internal(int eventType, const EmscriptenWebSocketOpenEvent* event, void* userData);
static EM_BOOL ws_onmessage_internal(int eventType, const EmscriptenWebSocketMessageEvent* event, void* userData);
static EM_BOOL ws_onerror_internal(int eventType, const EmscriptenWebSocketErrorEvent* event, void* userData);
static EM_BOOL ws_onclose_internal(int eventType, const EmscriptenWebSocketCloseEvent* event, void* userData);

// Initialize WebSocket client with event handlers
bool ws_open(WebSocketClient* ws_client, const char* url, void* user_ctx, WebSocketHandlers callbacks) {
    assert(ws_client);
    assert(url);
    assert(user_ctx);

    // Check if WebSocket is supported in this environment
    if (!emscripten_websocket_is_supported()) {
        LOG_ERROR("WebSocket is not supported in this environment");
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
        LOG_ERROR("Failed to create WebSocket");
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
bool ws_send_str(const WebSocketClient* ws_client, const char* data) {
    assert(ws_client);
    assert(data);
    assert(strlen(data) > 0);
    if (!ws_is_open(ws_client)) {
        return false;
    }

    // Send as text message
    EMSCRIPTEN_RESULT result = emscripten_websocket_send_utf8_text(ws_client->socket, data);
    if (result != EMSCRIPTEN_RESULT_SUCCESS) {
        LOG_ERROR("WebSocket send failed with error code: %d", result);
        return false;
    }

    return true;
}

bool ws_send_binary(const WebSocketClient* ws_client, const void* data, size_t len) {
    assert(ws_client);
    assert(data);
    assert(len > 0);
    if (!ws_is_open(ws_client)) {
        return false;
    }
    EMSCRIPTEN_RESULT result = emscripten_websocket_send_binary(ws_client->socket, (void*)data, (uint32_t)len);
    if (result != EMSCRIPTEN_RESULT_SUCCESS) {
        LOG_ERROR("WebSocket binary send failed: %d", result);
        return false;
    }
    return true;
}

void ws_close(WebSocketClient* ws_client) {
    assert(ws_client);
    if (ws_client->socket > 0) {
        emscripten_websocket_close(ws_client->socket, 1000, "Client initiated closure");
        emscripten_websocket_delete(ws_client->socket);
    }
    ws_client->socket = 0;
    ws_client->connected = false;
}

bool ws_is_open(const WebSocketClient* ws_client) {
    assert(ws_client);
    return ws_client && ws_client->connected;
}

// ============================================================================
// Internal Event Handlers (Emscripten WebSocket Callbacks)
// ============================================================================
static EM_BOOL ws_onopen_internal(int eventType, const EmscriptenWebSocketOpenEvent* event, void* userData) {
    printf("[WS] onopen fired — socket=%d\n", event ? event->socket : -1);
    WebSocketClient* socket_ctx = userData;
    if (socket_ctx && socket_ctx->callbacks.on_open_cb) {
        socket_ctx->connected = true;
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
    LOG_ERROR("WebSocket error occurred");
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
        LOG_ERROR("WebSocket closed unexpectedly: code=%d, reason='%s'", event->code, event->reason);
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
