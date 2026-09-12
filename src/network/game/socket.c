#include "socket.h"
#include "util/log.h"
#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <stdint.h>

#include <emscripten/websocket.h>

// Forward declarations of internal event handlers
static EM_BOOL socket_onopen_internal(int eventType, const EmscriptenWebSocketOpenEvent* event, void* userData);
static EM_BOOL socket_onmessage_internal(int eventType, const EmscriptenWebSocketMessageEvent* event, void* userData);
static EM_BOOL socket_onerror_internal(int eventType, const EmscriptenWebSocketErrorEvent* event, void* userData);
static EM_BOOL socket_onclose_internal(int eventType, const EmscriptenWebSocketCloseEvent* event, void* userData);

bool socket_open(Socket* sock, const char* url, void* user_ctx, SocketHandlers callbacks) {
    assert(sock);
    assert(url);
    assert(user_ctx);

    // Check if WebSocket is supported in this environment
    if (!emscripten_websocket_is_supported()) {
        LOG_ERROR("WebSocket is not supported in this environment");
        return false;
    }

    EmscriptenWebSocketCreateAttributes attrs;
    emscripten_websocket_init_create_attributes(&attrs);
    attrs.url = url;
    attrs.protocols = NULL;
    attrs.createOnMainThread = EM_TRUE;

    EMSCRIPTEN_WEBSOCKET_T handle = emscripten_websocket_new(&attrs);
    if (handle <= 0) {
        LOG_ERROR("Failed to create WebSocket");
        return false;
    }

    sock->socket = handle;
    sock->connected = false;
    sock->user_ctx = user_ctx;
    sock->callbacks = callbacks;

    // Register event callbacks; userData is forwarded verbatim to user callbacks
    emscripten_websocket_set_onopen_callback(handle, sock, socket_onopen_internal);
    emscripten_websocket_set_onmessage_callback(handle, sock, socket_onmessage_internal);
    emscripten_websocket_set_onerror_callback(handle, sock, socket_onerror_internal);
    emscripten_websocket_set_onclose_callback(handle, sock, socket_onclose_internal);

    return true;
}

bool socket_send(const Socket* sock, const void* data, size_t len) {
    assert(sock);
    assert(data);
    assert(len > 0);
    if (!socket_is_open(sock)) {
        return false;
    }
    EMSCRIPTEN_RESULT result = emscripten_websocket_send_binary(sock->socket, (void*)data, (uint32_t)len);
    if (result != EMSCRIPTEN_RESULT_SUCCESS) {
        LOG_ERROR("WebSocket send failed: %d", result);
        return false;
    }
    return true;
}

void socket_close(Socket* sock) {
    assert(sock);
    if (sock->socket > 0) {
        emscripten_websocket_close(sock->socket, 1000, "Client initiated closure");
        emscripten_websocket_delete(sock->socket);
    }
    sock->socket = 0;
    sock->connected = false;
}

bool socket_is_open(const Socket* sock) {
    assert(sock);
    return sock && sock->connected;
}

// ============================================================================
// Internal Event Handlers (Emscripten WebSocket Callbacks)
// ============================================================================
static EM_BOOL socket_onopen_internal(int eventType, const EmscriptenWebSocketOpenEvent* event, void* userData) {
    printf("[WS] onopen fired — socket=%d\n", event ? event->socket : -1);
    Socket* sock = userData;
    if (sock && sock->callbacks.on_open) {
        sock->connected = true;
        sock->callbacks.on_open(sock->user_ctx);
        return EM_TRUE;
    }
    return EM_FALSE;
}

static EM_BOOL socket_onmessage_internal(int eventType, const EmscriptenWebSocketMessageEvent* event, void* userData) {
    Socket* sock = userData;
    if (sock && sock->callbacks.on_receive) {
        sock->callbacks.on_receive(event->data, event->numBytes, sock->user_ctx);
        return EM_TRUE;
    }
    return EM_FALSE;
}

static EM_BOOL socket_onerror_internal(int eventType, const EmscriptenWebSocketErrorEvent* event, void* userData) {
    LOG_ERROR("WebSocket error occurred");
    Socket* sock = userData;
    if (sock) {
        if (sock->callbacks.on_error) {
            sock->callbacks.on_error(sock->user_ctx);
        }
        socket_close(sock);
    }
    return EM_TRUE;
}

static EM_BOOL socket_onclose_internal(int eventType, const EmscriptenWebSocketCloseEvent* event, void* userData) {
    Socket* sock = userData;
    // Only log unexpected closures
    if (!(bool)event->wasClean || event->code != 1000) {
        LOG_ERROR("WebSocket closed unexpectedly: code=%d, reason='%s'", event->code, event->reason);
    }
    if (sock) {
        if (sock->callbacks.on_close) {
            sock->callbacks.on_close(event->code, event->reason, sock->user_ctx);
        }
        sock->socket = 0;
        sock->connected = false;
    }

    return EM_TRUE;
}
