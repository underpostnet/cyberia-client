#ifndef CONFIG_H
#define CONFIG_H

#include <stdbool.h>

static const char* WS_URL =
#if defined(WS_URL_OVERRIDE)
    WS_URL_OVERRIDE;
#else
    "wss://";
#endif

static const char* API_BASE_URL =
#if defined(API_BASE_URL_OVERRIDE)
    API_BASE_URL_OVERRIDE;
#else
    "https://";
#endif

#define TICK_RATE_HZ          30
#define TICK_DURATION_S       (1.0 / (double)TICK_RATE_HZ)

/* Bootstrap render-tick interpolation offset, in ticks. Applies only until
 * g_game_state.interpolation_ms arrives from the client-hints endpoint. */
#define INTERP_TICKS          2

/* Cache ceilings. One atlas texture per item. */
static const int MAX_TEXTURE_CACHE_SIZE = 512;
static const int MAX_LAYER_CACHE_SIZE = 256;
static const int MAX_ATLAS_CACHE_SIZE = 256;

/* Frame duration for object layers that specify none. */
static const int DEFAULT_FRAME_DURATION_MS = 100;

/* Lookup code for GET /api/cyberia-client-hints/:code — the only source of
 * the presentation surface (palette, colour keys, status icons, camera and
 * cell tunings). A presentation override key only: the client carries no
 * instance, world, or server identifier. */
static const char* CYBERIA_CLIENT_HINTS_CODE = "cyberia-main";

#endif // CONFIG_H
