#ifndef CONFIG_H
#define CONFIG_H

/**
 * @file config.h
 * @brief Configuration file for the Cyberia client
 *
 * This file contains all configuration constants and settings
 * for the client application.
 */

// ============================================================================
// Network Configuration
// ============================================================================

/**
 * @brief WebSocket server URL
 *
 * The URL of the WebSocket server to connect to.
 * Format: ws://host:port/path or wss://host:port/path for secure connection
 *
 * Examples:
 *   - Local development: "ws://localhost:8080/ws"
 *   - Production: "wss://game.example.com/ws"
 */
extern const char* WS_URL;

// ============================================================================
// Application Configuration
// ============================================================================

/**
 * @brief Application version
 */
#define APP_VERSION "1.0.0"

/**
 * @brief Application name
 */
#define APP_NAME "Cyberia Client"

#endif // CONFIG_H
