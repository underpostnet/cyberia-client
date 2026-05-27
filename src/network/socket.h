#ifndef SOCKET_H
#define SOCKET_H

#include <emscripten/websocket.h>
#include <stdbool.h>

typedef void (*onopen_callback)(void* user_data);
typedef void (*onmessage_callback)(const uint8_t* data, uint32_t length, bool is_text, void* user_data);
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


bool ws_open(WebSocketClient* ws_client, const char* url, void* user_ctx, WebSocketHandlers handlers);
void ws_close(WebSocketClient* ws_client);
bool ws_is_open(WebSocketClient* ws_client);

/**
 * Sends a text message to the server. The function returns immediately
 * after queuing the message for transmission.
 */
bool ws_send_str(WebSocketClient* ws_client, const char* data);

/**
 * Sends a binary message to the server.
 */
bool ws_send_binary(WebSocketClient* ws_client, const uint8_t* data, size_t len);

#endif // SOCKET_H
