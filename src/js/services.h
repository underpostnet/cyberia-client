#ifndef JS_SERVICES_H
#define JS_SERVICES_H

/* Set the engine API base URL on the JS side (FetchState.api_base_url),
 * consulted by interact_overlay.js for DOM <img> previews. */
void js_init_engine_api(const char* api_base_url);

/* Queue an async fetch for quest metadata from the engine REST endpoint
 * GET /api/cyberia-quest/:code.  The JS side dispatches the HTTP request
 * and calls back into quest_metadata_cache_ingest_json() on completion. */
void js_fetch_quest_metadata(const char* code);

#endif /* JS_SERVICES_H */
