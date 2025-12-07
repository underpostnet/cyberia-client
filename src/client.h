#ifndef CLIENT_H
#define CLIENT_H

#include <stddef.h>

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

/**
 * @brief Get network statistics (bytes uploaded and downloaded)
 *
 * @param bytes_downloaded Pointer to store downloaded bytes count
 * @param bytes_uploaded Pointer to store uploaded bytes count
 */
void client_get_network_stats(size_t* bytes_downloaded, size_t* bytes_uploaded);

#endif // CLIENT_H