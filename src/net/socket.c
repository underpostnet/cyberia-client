#include "socket.h"
#include "log.h"
#include <assert.h>
#include <string.h>
#include <stdint.h>

#include <emscripten/websocket.h>

static EM_BOOL socket_onopen_internal(int eventType, const EmscriptenWebSocketOpenEvent* event, void* userData);
static EM_BOOL socket_onmessage_internal(int eventType, const EmscriptenWebSocketMessageEvent* event, void* userData);
static EM_BOOL socket_onerror_internal(int eventType, const EmscriptenWebSocketErrorEvent* event, void* userData);
static EM_BOOL socket_onclose_internal(int eventType, const EmscriptenWebSocketCloseEvent* event, void* userData);

bool socket_open(Socket* sock, const char* url, void* user_ctx, SocketHandlers callbacks) {
    assert(sock);
    assert(url);
    assert(user_ctx);
    if (socket_is_active(sock)) return true;

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
    if (0 >= handle) {
        LOG_ERROR("Failed to create WebSocket");
        return false;
    }

    sock->socket = handle;
    sock->user_ctx = user_ctx;
    sock->callbacks = callbacks;

    emscripten_websocket_set_onopen_callback(handle, sock, socket_onopen_internal);
    emscripten_websocket_set_onmessage_callback(handle, sock, socket_onmessage_internal);
    emscripten_websocket_set_onerror_callback(handle, sock, socket_onerror_internal);
    emscripten_websocket_set_onclose_callback(handle, sock, socket_onclose_internal);

    return true;
}

bool socket_send(const Socket* sock, const void* data, size_t len) {
    assert(sock);
    assert(data);
    assert(0 < len);
    if (!socket_is_open(sock)) {
        return false;
    }
    EMSCRIPTEN_RESULT result = emscripten_websocket_send_binary(sock->socket, (void*)data, (uint32_t)len);
    if (EMSCRIPTEN_RESULT_SUCCESS != result) {
        LOG_ERROR("WebSocket send failed: %d", result);
        return false;
    }
    return true;
}

void socket_close(Socket* sock) {
    assert(sock);
    EMSCRIPTEN_WEBSOCKET_T handle = sock->socket;
    sock->socket = 0;
    if (0 < handle) {
        emscripten_websocket_close(handle, 1000, "Client initiated closure");
        emscripten_websocket_delete(handle);
    }
}

bool socket_is_open(const Socket* sock) {
    assert(sock);
    unsigned short state;
    return socket_is_active(sock) &&
           EMSCRIPTEN_RESULT_SUCCESS == emscripten_websocket_get_ready_state(sock->socket, &state) && 1 == state;
}

bool socket_is_active(const Socket* sock) {
    assert(sock);
    return 0 < sock->socket;
}

static EM_BOOL socket_onopen_internal(int eventType, const EmscriptenWebSocketOpenEvent* event, void* userData) {
    Socket* sock = userData;
    assert(sock && event);
    if (event->socket != sock->socket || !socket_is_open(sock)) return EM_FALSE;
    if (NULL != sock->callbacks.on_open) sock->callbacks.on_open(sock->user_ctx);
    return EM_TRUE;
}

static EM_BOOL socket_onmessage_internal(int eventType, const EmscriptenWebSocketMessageEvent* event, void* userData) {
    Socket* sock = userData;
    assert(sock && event);
    if (event->socket != sock->socket) return EM_FALSE;
    if (NULL != sock->callbacks.on_receive) {
        sock->callbacks.on_receive(event->data, event->numBytes, sock->user_ctx);
        return EM_TRUE;
    }
    return EM_FALSE;
}

static EM_BOOL socket_onerror_internal(int eventType, const EmscriptenWebSocketErrorEvent* event, void* userData) {
    Socket* sock = userData;
    assert(sock && event);
    if (event->socket != sock->socket) return EM_FALSE;
    LOG_ERROR("WebSocket error occurred");
    if (NULL != sock->callbacks.on_error) sock->callbacks.on_error(sock->user_ctx);
    return EM_TRUE;
}

static EM_BOOL socket_onclose_internal(int eventType, const EmscriptenWebSocketCloseEvent* event, void* userData) {
    Socket* sock = userData;
    assert(sock && event);
    if (event->socket != sock->socket) return EM_FALSE;
    if (!event->wasClean || 1000 != event->code) {
        LOG_ERROR("WebSocket closed unexpectedly: code=%d, reason='%s'", event->code, event->reason);
    }
    socket_close(sock);
    if (NULL != sock->callbacks.on_close) sock->callbacks.on_close(event->code, event->reason, sock->user_ctx);

    return EM_TRUE;
}
