#ifndef CONFIG_H
#define CONFIG_H

#include <stdbool.h>

//TODO: Convert to an defines, ini, yml, json, etc

/**
 * @file config.h
 * @brief Configuration file for the Cyberia client
 *
 * This file contains all configuration constants and settings
 * for the client application including network URLs, cache sizes,
 * and animation parameters.
 */

// ============================================================================
// WebSocket Configuration
// ============================================================================

/**
 * @brief WebSocket server URL
 *
 * The URL of the WebSocket server to connect to for real-time updates.
 * Format: ws://host:port/path or wss://host:port/path for secure connection
 *
 * Examples:
 *   - Local development: "ws://localhost:8080/ws"
 *   - Production: "wss://server.cyberiaonline.com/ws"
 */
static const char* WS_URL = "wss://server.cyberiaonline.com/ws";
// const char* WS_URL = "ws://localhost:8080/ws";

// ============================================================================
// API Configuration
// ============================================================================

/**
 * @brief REST API base URL
 *
 * The base URL for REST API calls used to fetch object layers,
 * item data, and other game information.
 *
 * Examples:
 *   - Local development: "http://localhost:8080/api/v1"
 *   - Production: "https://server.cyberiaonline.com/api/v1"
 */
static const char* API_BASE_URL = "https://server.cyberiaonline.com/api/v1";
// const char* API_BASE_URL = "http://localhost:8080/api/v1";

// ============================================================================
// Assets Configuration
// ============================================================================

/**
 * @brief Assets server base URL
 *
 * The base URL for fetching textures, images, and other static assets.
 * Used to construct URLs for entity skins, weapons, and other visual elements.
 *
 * Examples:
 *   - Local development: "http://localhost:8080/assets"
 *   - Production: "https://server.cyberiaonline.com/assets"
 */
static const char* ASSETS_BASE_URL = "https://server.cyberiaonline.com/assets";
// const char* ASSETS_BASE_URL = "http://localhost:8080/assets";

// ============================================================================
// Game Configuration
// ============================================================================

/**
 * @brief Ghost item ID
 *
 * The item ID displayed when a player or bot is dead.
 * Usually "ghost" or similar placeholder.
 */
static const char* GHOST_ITEM_ID = "ghost";

// ============================================================================
// Network Configuration
// ============================================================================

/**
 * @brief HTTP request timeout in seconds
 *
 * Timeout for HTTP requests made to fetch assets and API data.
 * Set to a reasonable value to handle slow connections and prevent
 * indefinite hangs.
 */
static const long HTTP_TIMEOUT_SECONDS = 10L;

// ============================================================================
// Cache Configuration
// ============================================================================

/**
 * @brief Maximum number of textures in the cache
 *
 * Limits the number of textures that can be stored in memory.
 * Each texture can consume significant VRAM, so this prevents
 * unbounded memory growth during extended play sessions.
 */
static const int MAX_TEXTURE_CACHE_SIZE = 512;

/**
 * @brief Maximum number of object layers in the cache
 *
 * Limits the number of parsed ObjectLayer definitions stored in memory.
 * These are the metadata/structure for items, not the textures themselves.
 */
static const int MAX_LAYER_CACHE_SIZE = 256;

/**
 * @brief Queue size for texture caching requests
 *
 * Maximum number of texture loading requests that can be queued.
 * Prevents unbounded queue growth when many textures need to be cached.
 */
static const int TEXTURE_QUEUE_SIZE = 1024;

// ============================================================================
// Animation Configuration
// ============================================================================

/**
 * @brief Default frame duration in milliseconds
 *
 * Used for animation frame timing when an object layer doesn't
 * specify its own frame duration.
 */
static const int DEFAULT_FRAME_DURATION_MS = 100;

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
#define APP_NAME "CYBERIA | MMO"

// ============================================================================
// Development Configuration
// ============================================================================

/**
 * @brief Force development UI to always be enabled
 *
 * When set to true, the dev_ui will always be enabled regardless of
 * the server's devUi setting. This overrides the value received from
 * the server in the init message.
 *
 * Set to false to respect the server's dev_ui setting.
 */
static const bool FORCE_DEV_UI = false;
// const bool FORCE_DEV_UI = true;

#endif // CONFIG_H
