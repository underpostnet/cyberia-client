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
// static const char* WS_URL = "wss://server.cyberiaonline.com/ws";
static const char* WS_URL = "ws://localhost:8081/ws";

// ============================================================================
// Engine API Configuration
// ============================================================================

/**
 * @brief Engine API base URL
 *
 * The base URL for the Cyberia engine API. Used for:
 * - Atlas sprite sheet metadata (GET /api/atlas-sprite-sheet/)
 * - File blob retrieval (GET /api/file/blob/:id)
 * - Object layer metadata (GET /api/object-layer/)
 *
 * Examples:
 *   - Local development: "http://localhost:4005"
 *   - Production: "https://www.cyberiaonline.com"
 */
// static const char* API_BASE_URL = "https://www.cyberiaonline.com";
static const char* API_BASE_URL = "http://localhost:4005";

// ============================================================================
// Game Configuration
// ============================================================================

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
 * @brief Enable development UI overlay
 *
 * Client-local toggle — the server has no authority over dev-UI state.
 * When true, the dev overlay renders regardless of any presentation hint.
 * When false, the optional engine ClientHints override (if any) is honored.
 */
static const bool ENABLE_DEV_UI = false;

/**
 * @brief Lookup code for the presentation client-hints endpoint.
 *
 * The C client fires GET /api/cyberia-client-hints/:CYBERIA_CLIENT_HINTS_CODE
 * at startup to discover the full presentation surface (palette, entity
 * colour keys, status-icon visuals, camera + cell tunings). The endpoint
 * is the *only* source of those values — the client carries no compile-time
 * palette. Until the fetch settles the runtime returns a tiny inline
 * bootstrap fallback so the splash screen has something to draw. Gameplay
 * is never affected by this fetch.
 *
 * NOTE: this code is purely a *presentation override key*. The C client
 * never carries an instance / world / server identifier; everything that
 * scopes the simulation arrives dynamically via the WebSocket handshake.
 */
static const char* CYBERIA_CLIENT_HINTS_CODE = "cyberia-main";

#endif // CONFIG_H
