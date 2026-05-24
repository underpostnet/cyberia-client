#ifndef JS_SERVICES_H
#define JS_SERVICES_H

/* Set the engine API base URL on the JS side (FetchState.api_base_url),
 * consulted by interact_overlay.js for DOM <img> previews. */
void js_init_engine_api(const char* api_base_url);

#endif /* JS_SERVICES_H */
