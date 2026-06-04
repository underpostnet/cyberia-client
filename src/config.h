#ifndef CONFIG_H
#define CONFIG_H

#include <stdbool.h>

#define STRINGIFY(x) #x

// --------------------------------------------------------
// URLs

static const char* WS_URL =
#if defined(WS_URL_OVERRIDE)
    STRINGIFY(WS_URL_OVERRIDE);
#else
    "ws://";
#endif

static const char* API_BASE_URL =
#if defined(API_BASE_URL_OVERRIDE)
    STRINGIFY(API_BASE_URL_OVERRIDE);
#else
    "https://";
#endif

// --------------------------------------------------------
// Simulation
#define TICK_RATE_HZ          30
#define TICK_DURATION_S       (1.0 / (double)TICK_RATE_HZ)

/* Bootstrap fallback for the render-tick interpolation offset, in ticks.
 * The live value is derived from the runtime interpolation window
 * (g_game_state.interpolation_ms, served by the client-hints endpoint); this
 * constant only applies before that window is hydrated. */
#define INTERP_TICKS          2

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
