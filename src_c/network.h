#ifndef NETWORK_H
#define NETWORK_H

#include <emscripten/websocket.h>

// WebSocket client structure
typedef struct {
    EMSCRIPTEN_WEBSOCKET_T socket;
    int connected;
} WebSocketClient;

// Callback function types for event-driven architecture
typedef void (*on_open_callback)(void* user_data);
typedef void (*on_message_callback)(const char* data, int length, void* user_data);
typedef void (*on_error_callback)(void* user_data);
typedef void (*on_close_callback)(int code, const char* reason, void* user_data);

// WebSocket event handlers structure
typedef struct {
    on_open_callback on_open;
    on_message_callback on_message;
    on_error_callback on_error;
    on_close_callback on_close;
    void* user_data;
} WebSocketHandlers;

// Initialize WebSocket client with event handlers
int ws_init(WebSocketClient* client, const char* url, WebSocketHandlers* handlers);

// Send message through WebSocket
int ws_send(WebSocketClient* client, const char* data, int length);

// Close WebSocket connection
void ws_close(WebSocketClient* client);

// Check if WebSocket is connected
int ws_is_connected(WebSocketClient* client);

#endif // NETWORK_H