#ifndef NETWORK_H
#define NETWORK_H

#include <emscripten/websocket.h>

/**
 * @file network.h
 * @brief WebSocket network layer interface
 * 
 * This module provides a clean abstraction over Emscripten's WebSocket API.
 * It implements an event-driven architecture with callback functions for
 * connection lifecycle management.
 */

/**
 * @brief WebSocket client structure
 * 
 * Contains the WebSocket handle and connection state.
 */
typedef struct {
    EMSCRIPTEN_WEBSOCKET_T socket;  // Emscripten WebSocket handle
    int connected;                   // Connection status (1=connected, 0=disconnected)
} WebSocketClient;

/**
 * @brief Callback function type for connection open event
 * @param user_data User-defined data pointer passed during initialization
 */
typedef void (*on_open_callback)(void* user_data);

/**
 * @brief Callback function type for message received event
 * @param data Pointer to received message data
 * @param length Length of the message in bytes
 * @param user_data User-defined data pointer passed during initialization
 */
typedef void (*on_message_callback)(const char* data, int length, void* user_data);

/**
 * @brief Callback function type for error event
 * @param user_data User-defined data pointer passed during initialization
 */
typedef void (*on_error_callback)(void* user_data);

/**
 * @brief Callback function type for connection close event
 * @param code WebSocket close code (e.g., 1000 for normal closure)
 * @param reason Human-readable close reason string
 * @param user_data User-defined data pointer passed during initialization
 */
typedef void (*on_close_callback)(int code, const char* reason, void* user_data);

/**
 * @brief WebSocket event handlers structure
 * 
 * Contains callback functions for all WebSocket lifecycle events.
 * All callbacks are optional (can be NULL), but it's recommended to
 * implement at least on_open and on_message for proper functionality.
 */
typedef struct {
    on_open_callback on_open;        // Called when connection is established
    on_message_callback on_message;  // Called when message is received
    on_error_callback on_error;      // Called when error occurs
    on_close_callback on_close;      // Called when connection is closed
    void* user_data;                 // User-defined data passed to callbacks
} WebSocketHandlers;

/**
 * @brief Initialize WebSocket client with event handlers
 * 
 * Creates a new WebSocket connection and registers event handlers.
 * This function is non-blocking; connection establishment is asynchronous.
 * 
 * @param client Pointer to WebSocketClient structure to initialize
 * @param url WebSocket URL (e.g., "ws://localhost:8080/ws")
 * @param handlers Pointer to WebSocketHandlers structure with callbacks
 * @return 0 on success, -1 on failure
 */
int ws_init(WebSocketClient* client, const char* url, WebSocketHandlers* handlers);

/**
 * @brief Send message through WebSocket
 * 
 * Sends a text message to the server. The function returns immediately
 * after queuing the message for transmission.
 * 
 * @param client Pointer to initialized WebSocketClient
 * @param data Pointer to message data (null-terminated string)
 * @param length Length of the message in bytes
 * @return 0 on success, -1 on failure
 */
int ws_send(WebSocketClient* client, const char* data, int length);

/**
 * @brief Close WebSocket connection
 * 
 * Gracefully closes the WebSocket connection with a normal closure code (1000).
 * 
 * @param client Pointer to WebSocketClient to close
 */
void ws_close(WebSocketClient* client);

/**
 * @brief Check if WebSocket is connected
 * 
 * @param client Pointer to WebSocketClient to check
 * @return 1 if connected, 0 if disconnected or client is NULL
 */
int ws_is_connected(WebSocketClient* client);

#endif // NETWORK_H