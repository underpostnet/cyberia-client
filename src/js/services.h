#ifndef JS_SERVICES_H
#define JS_SERVICES_H

/* For REST/blob fetches, prefer the native engine fetch API
 * (network/engine_client.h: fetch_request_start) over adding a new JS bridge
 * here. The C path handles HTTP via emscripten_fetch, owns caching, and parses
 * with cJSON — see dialogue_data.c / quest_metadata_cache.c. Add to this file
 * only for things that genuinely must run in JS (DOM/browser APIs). */

/* Set the engine API base URL on the JS side (FetchState.api_base_url),
 * consulted by interact_overlay.js for DOM <img> previews. */
void js_init_engine_api(const char* api_base_url);

#endif /* JS_SERVICES_H */
