#ifndef INSTANCE_ROUTE_H
#define INSTANCE_ROUTE_H

#include <stddef.h>

/* URL-derived instance routing, kept free of emscripten so it is unit-testable.
 *
 * The instance code is the first segment of the browser location; the empty
 * segment (root) is the default instance and must yield a prefix-free
 * "<origin>/ws", while a non-root instance keeps its segment so the URL matches
 * the server's CYBERIA_BASE_PATH mount. */

/* Drops every character outside the legal code alphabet, in place. The code
 * reaches us from the URL and is interpolated into the websocket URL. */
void instance_route_sanitize_code(char* code);

/* Writes the websocket endpoint for `instance_code`. `runtime_origin` is the
 * injected window.CYBERIA_WS_ORIGIN; when empty, `fallback_endpoint` is the
 * compile-time full endpoint ("wss://host/ws") whose "/ws" suffix is dropped to
 * recover an origin. */
void instance_route_ws_url(const char* runtime_origin, const char* fallback_endpoint,
                           const char* instance_code, char* out, size_t out_size);

#endif // INSTANCE_ROUTE_H
