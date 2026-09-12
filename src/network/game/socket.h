#ifndef SOCKET_H
#define SOCKET_H

#include <emscripten/websocket.h>
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

/* Transport only: the socket moves bytes. It never reads or builds a
 * message — that is the job of util/serial.h and message.h. */

typedef void (*on_open_callback)(void* user_data);
typedef void (*on_receive_callback)(const uint8_t* data, uint32_t length, void* user_data);
typedef void (*on_error_callback)(void* user_data);
typedef void (*on_close_callback)(int code, const char* reason, void* user_data);

typedef struct {
    on_open_callback    on_open;     // Called when the connection opens
    on_receive_callback on_receive;  // Called for each frame from the server
    on_error_callback   on_error;    // Called on a transport error
    on_close_callback   on_close;    // Called when the connection closes
} SocketHandlers;

typedef struct {
    EMSCRIPTEN_WEBSOCKET_T socket;
    bool connected;
    SocketHandlers callbacks;
    void* user_ctx;
} Socket;

bool socket_open(Socket* sock, const char* url, void* user_ctx, SocketHandlers handlers);
void socket_close(Socket* sock);
bool socket_is_open(const Socket* sock);

/* Send packed bytes to the server. */
bool socket_send(const Socket* sock, const void* data, size_t len);

#endif // SOCKET_H
