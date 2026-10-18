#ifndef CONFIG_H
#define CONFIG_H

#include <stdbool.h>
#include <stddef.h>

/** Must equal the server DefaultTickRate. Replay needs the same dt. */
#define TICK_RATE_HZ             30

/** Utility 1/fps */
#define TICK_DURATION_S       (1.0 / (double)TICK_RATE_HZ)

/* Cache ceilings. One atlas texture per item. */
static const int MAX_LAYER_CACHE_SIZE = 256;
static const int MAX_ATLAS_CACHE_SIZE = 256;

/* Lookup code for GET /api/v1/cyberia-client-hints/:code — the only source of
 * the presentation surface (palette, colour keys, status icons, camera and
 * cell tunings). A presentation override key only: the client carries no
 * instance, world, or server identifier. */
static const char* CYBERIA_CLIENT_HINTS_CODE = "cyberia-main";

/* One WASM binary serves every world instance. The instance code is the first
 * segment of window.location.pathname; an empty segment (root) is the default
 * instance and yields a prefix-free "<origin>/ws" for the default server. A
 * non-root instance keeps its segment in "<origin>/<instance>/ws", matching
 * the server's CYBERIA_BASE_PATH mount. The websocket origin has one source:
 * window.CYBERIA_WS_ORIGIN, injected by the static server.
 *
 * The Data Server URL has one source: the --data-server-url=<origin> command
 * line argument. Both are required. Call config_init() before connection_open()
 * or any Data Server fetch. */

void        config_init(int argc, char** argv);
const char* config_ws_url(void);
const char* config_data_server_url(void);
/* The instance code of the URL; empty for the default instance. */
const char* config_instance_code(void);

/* The launch ticket the page read from its fragment, once: a later call
 * answers false. A ticket lives in memory only. */
bool        config_take_launch_ticket(char* out, size_t out_size);

/* The portal pages the game leaves for: the world selection and the sign-out. */
typedef enum { PORTAL_WORLDS, PORTAL_SIGN_OUT } PortalPage;

void        config_open_portal(PortalPage page);

/* Opens the launch bridge of a world for an account or a guest session. An
 * empty code is the default world. */
void        config_open_launch(const char* instance_code, bool account);

/* Plays another world in this page: `path` is its path on the client ("" or
 * "/<code>"). The instance code, the WebSocket URL and the address bar follow
 * it, and the next connection authenticates with `ticket`. */
void        config_enter_world(const char* path, const char* ticket);

#endif // CONFIG_H
