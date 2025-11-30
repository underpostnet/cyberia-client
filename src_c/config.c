#include "config.h"

/**
 * @file config.c
 * @brief Configuration implementation for the Cyberia client
 */

// ============================================================================
// WebSocket Configuration
// ============================================================================

/**
 * @brief WebSocket server URL
 *
 * Production: "wss://server.cyberiaonline.com/ws"
 * Development: "ws://localhost:8080/ws"
 */
// const char* WS_URL = "wss://server.cyberiaonline.com/ws";
const char* WS_URL = "ws://localhost:8080/ws";

// ============================================================================
// API Configuration
// ============================================================================

/**
 * @brief REST API base URL
 *
 * Production: "https://server.cyberiaonline.com/api/v1"
 * Development: "http://localhost:8080/api/v1"
 */
// const char* API_BASE_URL = "https://server.cyberiaonline.com/api/v1";
const char* API_BASE_URL = "http://localhost:8080/api/v1";

// ============================================================================
// Assets Configuration
// ============================================================================

/**
 * @brief Assets server URL for textures, images, and UI resources
 *
 * Production: "https://server.cyberiaonline.com/assets"
 * Development: "http://localhost:8080/assets"
 */
// const char* ASSETS_BASE_URL = "https://server.cyberiaonline.com/assets";
const char* ASSETS_BASE_URL = "http://localhost:8080/assets";

// ============================================================================
// Game Configuration
// ============================================================================

/**
 * @brief Ghost item ID - displayed when player or bot is dead
 */
const char* GHOST_ITEM_ID = "ghost";

// ============================================================================
// Network Configuration
// ============================================================================

/**
 * @brief HTTP request timeout in seconds
 */
const long HTTP_TIMEOUT_SECONDS = 10L;

// ============================================================================
// Cache Configuration
// ============================================================================

/**
 * @brief Maximum number of textures to keep in the cache
 */
const int MAX_TEXTURE_CACHE_SIZE = 512;

/**
 * @brief Maximum number of object layers to keep in the cache
 */
const int MAX_LAYER_CACHE_SIZE = 256;

/**
 * @brief Queue size for pending texture caching operations
 */
const int TEXTURE_QUEUE_SIZE = 1024;

// ============================================================================
// Animation Configuration
// ============================================================================

/**
 * @brief Default frame duration in milliseconds for animations
 */
const int DEFAULT_FRAME_DURATION_MS = 100;
