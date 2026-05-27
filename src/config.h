#ifndef CONFIG_H
#define CONFIG_H

#include <stdbool.h>

#define STRINGIFY(x) #x

static const char* WS_URL =
#if defined(WS_URL_OVERRIDE)
    STRINGIFY(WS_URL_OVERRIDE);
#elif defined(CYBERIA_DEBUG)
    "ws://localhost:8081/ws";
#else
    "ws://server.cyberiaonline.com/ws";
#endif

static const char* API_BASE_URL =
#if defined(API_BASE_URL_OVERRIDE)
    STRINGIFY(API_BASE_URL_OVERRIDE);
#elif defined(CYBERIA_DEBUG)
    "http://localhost:4005";
#else
    "https://www.cyberiaonline.com";
#endif

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
// Development Configuration
// ============================================================================

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
