#ifndef CYBERIA_TEXTURE_CACHE_H
#define CYBERIA_TEXTURE_CACHE_H

#include <raylib.h>

#include "network/engine_client.h"

/*
 * General-purpose async texture cache.
 *
 * Loads PNGs over the engine_client fetch pipeline, decodes + uploads to the
 * GPU on completion, caches the result keyed by URL, and LRU-evicts down to a
 * fixed capacity. Standalone: depends only on raylib, hash_table, and the
 * engine_client fetch API — no domain or render modules.
 *
 * Multiple independent caches may coexist (atlas textures, UI icons, …). Each
 * is a distinct authority over its own URLs; they never share state.
 *
 * Async routing: the engine_client fetch callback carries no user-data, so the
 * consumer supplies a static trampoline at create time that forwards the
 * completion back into the right cache instance via
 * texture_cache_on_blob_fetched().
 */

typedef struct TextureCache TextureCache;

/* on_blob — consumer trampoline of the form:
 *     static void cb(const FetchResponse* r) {
 *         texture_cache_on_blob_fetched(<this cache>, r);
 *     }
 */
TextureCache* texture_cache_create(int capacity, const char* debug_name, FetchCompletedCb on_blob);
void          texture_cache_destroy(TextureCache* tc);

/* Ready Texture2D, or {.id = 0} while loading or on error. The first call for
 * a URL fires the async fetch; later calls return the cached result and
 * refresh the entry's LRU timestamp. */
Texture2D     texture_cache_get(TextureCache* tc, const char* url);

/* Route an engine_client fetch completion into the cache (keyed by URL). */
void          texture_cache_on_blob_fetched(TextureCache* tc, const FetchResponse* r);

/* Drop a cached entry and unload its GPU texture. No-op if absent. */
void          texture_cache_evict(TextureCache* tc, const char* url);

#endif /* CYBERIA_TEXTURE_CACHE_H */
