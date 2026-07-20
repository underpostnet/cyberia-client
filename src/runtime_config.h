#ifndef RUNTIME_CONFIG_H
#define RUNTIME_CONFIG_H

/* One WASM binary serves every world instance. The instance code is the first
 * segment of window.location.pathname; an empty segment (root) is the default
 * instance and yields a prefix-free "<origin>/ws" so the server's `/` route
 * serves it. Endpoint origins come from window.CYBERIA_WS_ORIGIN /
 * window.CYBERIA_ENGINE_API_ORIGIN, injected by the static server; config.h
 * constants are the fallback for local builds. Call runtime_config_init()
 * before connection_open() or any engine fetch. */

void        runtime_config_init(void);
const char* runtime_config_instance_code(void);
const char* runtime_config_ws_url(void);
const char* runtime_config_api_base_url(void);

#endif // RUNTIME_CONFIG_H
