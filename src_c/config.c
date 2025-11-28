#include "config.h"

/**
 * @file config.c
 * @brief Configuration implementation for the Cyberia client
 */

// ============================================================================
// Network Configuration Implementation
// ============================================================================

/**
 * @brief WebSocket server URL
 *
 * Default URL for development environment.
 * This can be changed to point to production server.
 */
const char* WS_URL = "wss://server.cyberiaonline.com/ws";
// const char* WS_URL = "ws://localhost:8080/ws";
