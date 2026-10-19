#include "net/socket.h"

#include <assert.h>
#include <emscripten/websocket.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

static struct {
    unsigned short state;
    bool deleted;
    void* ctx;
    em_websocket_open_callback_func open;
    em_websocket_message_callback_func message;
    em_websocket_error_callback_func error;
    em_websocket_close_callback_func close;
} s_handles[16];
static int s_created, s_deleted, s_sent, s_opens, s_messages, s_errors, s_closes;
static bool s_supported = true, s_reopen;
static Socket s_socket;
static SocketHandlers s_handlers;

bool emscripten_websocket_is_supported(void) { return s_supported; }
EMSCRIPTEN_WEBSOCKET_T emscripten_websocket_new(EmscriptenWebSocketCreateAttributes* attrs) {
    assert(NULL != attrs->url && attrs->createOnMainThread);
    return ++s_created;
}
EMSCRIPTEN_RESULT emscripten_websocket_get_ready_state(EMSCRIPTEN_WEBSOCKET_T handle, unsigned short* state) {
    if (s_handles[handle].deleted) return -1;
    *state = s_handles[handle].state;
    return EMSCRIPTEN_RESULT_SUCCESS;
}
EMSCRIPTEN_RESULT emscripten_websocket_send_binary(EMSCRIPTEN_WEBSOCKET_T handle, void* data, uint32_t length) {
    assert(!s_handles[handle].deleted && 1 == s_handles[handle].state);
    s_sent++;
    return EMSCRIPTEN_RESULT_SUCCESS;
}
EMSCRIPTEN_RESULT emscripten_websocket_close(EMSCRIPTEN_WEBSOCKET_T handle, unsigned short code, const char* reason) {
    s_handles[handle].state = 3;
    return EMSCRIPTEN_RESULT_SUCCESS;
}
EMSCRIPTEN_RESULT emscripten_websocket_delete(EMSCRIPTEN_WEBSOCKET_T handle) {
    assert(!s_handles[handle].deleted);
    s_handles[handle].deleted = true;
    s_deleted++;
    return EMSCRIPTEN_RESULT_SUCCESS;
}
EMSCRIPTEN_RESULT emscripten_websocket_set_onopen_callback(EMSCRIPTEN_WEBSOCKET_T handle, void* ctx,
                                                          em_websocket_open_callback_func callback) {
    s_handles[handle].ctx = ctx;
    s_handles[handle].open = callback;
    return EMSCRIPTEN_RESULT_SUCCESS;
}
EMSCRIPTEN_RESULT emscripten_websocket_set_onmessage_callback(EMSCRIPTEN_WEBSOCKET_T handle, void* ctx,
                                                             em_websocket_message_callback_func callback) {
    s_handles[handle].message = callback;
    return EMSCRIPTEN_RESULT_SUCCESS;
}
EMSCRIPTEN_RESULT emscripten_websocket_set_onerror_callback(EMSCRIPTEN_WEBSOCKET_T handle, void* ctx,
                                                           em_websocket_error_callback_func callback) {
    s_handles[handle].error = callback;
    return EMSCRIPTEN_RESULT_SUCCESS;
}
EMSCRIPTEN_RESULT emscripten_websocket_set_onclose_callback(EMSCRIPTEN_WEBSOCKET_T handle, void* ctx,
                                                           em_websocket_close_callback_func callback) {
    s_handles[handle].close = callback;
    return EMSCRIPTEN_RESULT_SUCCESS;
}

static void opened(void* ctx) {
    s_opens++;
    assert(socket_send(ctx, "auth", 4));
}
static void received(const uint8_t* data, uint32_t length, void* ctx) { s_messages++; }
static void errored(void* ctx) { s_errors++; }
static void closed(int code, const char* reason, void* ctx) {
    s_closes++;
    assert(!socket_is_active(ctx));
    if (s_reopen) assert(socket_open(ctx, "wss://world.test/ws", ctx, s_handlers));
}
static void close_event(int handle) {
    EmscriptenWebSocketCloseEvent event = { .socket = handle, .wasClean = true, .code = 1000 };
    s_handles[handle].close(0, &event, s_handles[handle].ctx);
}

int main(void) {
    s_handlers = (SocketHandlers){ .on_open = opened, .on_receive = received, .on_error = errored, .on_close = closed };
    s_supported = false;
    assert(!socket_open(&s_socket, "wss://world.test/ws", &s_socket, s_handlers));
    assert(!socket_is_active(&s_socket));
    s_supported = true;
    assert(socket_open(&s_socket, "wss://world.test/ws", &s_socket, s_handlers));
    int first = s_socket.socket;
    assert(socket_is_active(&s_socket) && !socket_is_open(&s_socket));
    assert(!socket_send(&s_socket, "auth", 4));
    assert(socket_open(&s_socket, "wss://world.test/ws", &s_socket, s_handlers));
    assert(1 == s_created && first == s_socket.socket);

    s_handles[first].state = 1;
    EmscriptenWebSocketOpenEvent open = { .socket = first };
    assert(s_handles[first].open(0, &open, s_handles[first].ctx));
    assert(1 == s_opens && 1 == s_sent);
    EmscriptenWebSocketMessageEvent message = { .socket = first };
    assert(s_handles[first].message(0, &message, s_handles[first].ctx));
    assert(1 == s_messages);
    s_handles[first].state = 2;
    assert(!socket_is_open(&s_socket) && !socket_send(&s_socket, "auth", 4));
    assert(!s_handles[first].open(0, &open, s_handles[first].ctx));
    assert(1 == s_opens && 1 == s_sent);

    s_reopen = true;
    close_event(first);
    int second = s_socket.socket;
    assert(first != second && socket_is_active(&s_socket));
    assert(1 == s_deleted && 1 == s_closes);
    EmscriptenWebSocketErrorEvent error = { .socket = first };
    assert(!s_handles[first].open(0, &open, s_handles[first].ctx));
    assert(!s_handles[first].message(0, &message, s_handles[first].ctx));
    assert(!s_handles[first].error(0, &error, s_handles[first].ctx));
    close_event(first);
    assert(second == s_socket.socket && 1 == s_closes && 0 == s_errors && 1 == s_messages);

    s_reopen = false;
    s_handles[second].state = 3;
    error.socket = second;
    assert(s_handles[second].error(0, &error, s_handles[second].ctx));
    assert(1 == s_errors && 1 == s_closes);
    assert(!socket_send(&s_socket, "auth", 4));
    close_event(second);
    assert(2 == s_closes && 2 == s_deleted && !socket_is_active(&s_socket));
    socket_close(&s_socket);
    assert(2 == s_deleted);

    puts("socket_test OK");
    return 0;
}
