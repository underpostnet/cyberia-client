#ifndef CLIENT_H
#define CLIENT_H

/**
 * @file client.h
 * @brief Client subsystem interface for network communication
 * 
 * This module handles all WebSocket communication with the server.
 * It maintains the connection state and processes incoming/outgoing messages.
 * The client operates in an event-driven architecture with callbacks for
 * connection lifecycle events.
 */

/**
 * @brief Initialize the client subsystem
 * 
 * Establishes WebSocket connection to the server and sets up event handlers.
 * This function is non-blocking; connection status updates are provided
 * through callback events.
 * 
 * @return 0 on success, -1 on failure
 */
int client_init(void);

/**
 * @brief Update client subsystem (process events)
 * 
 * This function should be called every frame to process WebSocket events
 * and handle any pending network operations. It's non-blocking and returns
 * immediately after processing queued events.
 */
void client_update(void);

/**
 * @brief Cleanup client subsystem
 * 
 * Closes the WebSocket connection gracefully and releases all network
 * resources. This should be called before application shutdown.
 */
void client_cleanup(void);

/**
 * @brief Check if client is connected to the server
 * 
 * @return 1 if connected, 0 if disconnected
 */
int client_is_connected(void);

/**
 * @brief Send a message to the server
 * 
 * @param message Null-terminated string to send
 * @return 0 on success, -1 on failure
 */
int client_send(const char* message);

/**
 * @brief Get the last received message from the server
 * 
 * @return Pointer to the last message (null-terminated string)
 *         Returns "No message" if no message has been received yet
 */
const char* client_get_last_message(void);

#endif // CLIENT_H