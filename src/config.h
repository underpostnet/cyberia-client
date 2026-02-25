#ifndef CONFIG_H
#define CONFIG_H

#include <stdbool.h>

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
// static const char* WS_URL = "ws://localhost:8080/ws";

// ============================================================================
// Engine API Configuration
// ============================================================================

/**
 * @brief Engine API base URL (www.cyberiaonline.com)
 *
 * The base URL for the Cyberia engine API. Used for:
 * - Authentication (POST /api/user/auth)
 * - Atlas sprite sheet metadata (GET /api/atlas-sprite-sheet/)
 * - File blob retrieval (GET /api/file/blob/:id)
 * - Object layer metadata (GET /api/object-layer/)
 *
 * Examples:
 *   - Local development: "http://localhost:4001"
 *   - Production: "https://www.cyberiaonline.com"
 */
static const char* API_BASE_URL = "https://www.cyberiaonline.com";
// static const char* API_BASE_URL = "http://localhost:4001";

// ============================================================================
// Authentication Configuration
// ============================================================================

/**
 * @brief API authentication email
 *
 * Email used to authenticate against the Cyberia engine API
 * (POST /api/user/auth). Required for accessing authenticated
 * endpoints such as object layer metadata.
 *
 * Set these to valid credentials before building.
 * In production, consider injecting via build-time defines.
 */
static const char* AUTH_EMAIL = "";

/**
 * @brief API authentication password
 *
 * Password used to authenticate against the Cyberia engine API.
 */
static const char* AUTH_PASSWORD = "";

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
 * Limits the number of atlas textures that can be stored in memory.
 * Each atlas texture can consume significant VRAM, so this prevents
 * unbounded memory growth during extended play sessions.
 * With the atlas approach, each item uses only one texture entry.
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
 * @brief Maximum number of atlas sprite sheet entries in the cache
 *
 * Limits the number of atlas sprite sheet data entries stored in memory.
 * Each entry contains frame metadata for clipping individual frames
 * from the consolidated atlas texture.
 */
static const int MAX_ATLAS_CACHE_SIZE = 256;

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
// static const bool FORCE_DEV_UI = true;

#endif // CONFIG_H