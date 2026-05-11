#ifndef CLIENT_H
#define CLIENT_H

#include <stddef.h>
#include <cJSON.h>

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
 * @brief Send a raw string message to the server
 *
 * @param message Null-terminated string to send
 * @return 0 on success, -1 on failure
 */
int client_send_msg(const char* message);

/**
 * @brief Send a cJSON object to the server (serialised to text)
 *
 * @param json cJSON object; not freed by this function
 * @return 0 on success, -1 on failure
 */
int client_send(cJSON* json_obj);

/**
 * @brief Get network statistics (bytes uploaded and downloaded)
 *
 * @param bytes_downloaded Pointer to store downloaded bytes count
 * @param bytes_uploaded Pointer to store uploaded bytes count
 */
void client_get_network_stats(size_t* bytes_downloaded, size_t* bytes_uploaded);

int client_send_tap(float grid_x, float grid_y);

#endif // CLIENT_H
