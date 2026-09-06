#ifndef CONFIG_H
#define CONFIG_H

#include <stdbool.h>

#if defined(CYBERIA_DEBUG)
#define WS_URL                "ws://localhost:8081/ws"
#else
#define WS_URL                "wss://server.cyberiaonline.com/ws"
#endif

#if defined(CYBERIA_DEBUG)
#define API_BASE_URL    "http://localhost:8081"
#else
#define API_BASE_URL    "https://www.cyberiaonline.com"
#endif

/** Override with -DTICK_RATE_OVERRIDE=<hz>. */
#if defined(TICK_RATE_OVERRIDE)
#define TICK_RATE_HZ             TICK_RATE_OVERRIDE
#else
#define TICK_RATE_HZ             30
#endif

/** Utility 1/fps */
#define TICK_DURATION_S       (1.0 / (double)TICK_RATE_HZ)

/* Cache ceilings. One atlas texture per item. */
static const int MAX_TEXTURE_CACHE_SIZE = 512;
static const int MAX_LAYER_CACHE_SIZE = 256;
static const int MAX_ATLAS_CACHE_SIZE = 256;

/* Lookup code for GET /api/cyberia-client-hints/:code — the only source of
 * the presentation surface (palette, colour keys, status icons, camera and
 * cell tunings). A presentation override key only: the client carries no
 * instance, world, or server identifier. */
static const char* CYBERIA_CLIENT_HINTS_CODE = "cyberia-main";

#endif // CONFIG_H
