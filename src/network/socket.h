#ifndef SOCKET_H
#define SOCKET_H

#include <emscripten/websocket.h>
#include <stdbool.h>

typedef void (*onopen_callback)(void* user_data);
typedef void (*onmessage_callback)(const char* data, uint32_t length, bool is_binary, void* user_data);
typedef void (*onerror_callback)(void* user_data);
typedef void (*onclose_callback)(int code, const char* reason, void* user_data);

typedef struct {
    onopen_callback on_open_cb;        // Called when connection is established
    onmessage_callback on_message_cb;  // Called when message is received
    onerror_callback on_error_cb;      // Called when error occurs
    onclose_callback on_close_cb;      // Called when connection is closed
} WebSocketHandlers;

typedef struct {
    EMSCRIPTEN_WEBSOCKET_T socket;
    bool connected;
    WebSocketHandlers callbacks;
    void* user_ctx;
} WebSocketClient;

/**
 * Creates a new WebSocket connection and registers event handlers.
 * This function is non-blocking; connection establishment is asynchronous.
 */
bool ws_init(WebSocketClient* ws_client, const char* url, void* user_ctx, WebSocketHandlers handlers);
void ws_close(WebSocketClient* ws_client);
bool ws_is_connected(WebSocketClient* ws_client);

/**
 * Sends a text message to the server. The function returns immediately
 * after queuing the message for transmission.
 *
 * @param client Pointer to initialized WebSocketClient
 * @param data Pointer to message data (null-terminated string)
 */
bool ws_send(WebSocketClient* ws_client, const char* data);

#endif // SOCKET_H
