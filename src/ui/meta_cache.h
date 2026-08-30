/**
 * meta_cache — shared core of the code-keyed engine metadata caches.
 *
 * The Go server sends only a CODE over AOI. The presentation metadata comes
 * from the engine REST endpoint `<url_prefix><code>`, which answers with the
 * envelope `{ status, data: <doc> }`. Metadata is immutable, so each code is
 * fetched at most once per session.
 *
 * A user of this core declares an entry struct whose first member is a
 * `MetaCacheHead`, then one static `MetaCache` that describes the array.
 */

#ifndef CYBERIA_UI_META_CACHE_H
#define CYBERIA_UI_META_CACHE_H

#include "network/engine_client.h"

#include <cJSON.h>
#include <stddef.h>

#define META_CACHE_CODE_MAX 64

typedef enum {
    META_CACHE_NONE = 0,
    META_CACHE_LOADING,
    META_CACHE_READY,
    META_CACHE_ERROR,
} MetaCacheState;

/* First member of every cache entry struct. */
typedef struct {
    char           code[META_CACHE_CODE_MAX];
    MetaCacheState state;
} MetaCacheHead;

typedef struct {
    void*       entries;      /* array of `cap` structs, each `elem_size` bytes */
    size_t      elem_size;
    int         cap;
    int         count;
    const char* url_prefix;   /* REST path, the code is appended */
    const char* label;        /* names the cache in log lines */
    /* Fills the entry from the envelope's `data` object. */
    void (*ingest)(void* entry, const cJSON* doc);
} MetaCache;

/* Cached entry for `code`, or NULL. */
void* meta_cache_find(MetaCache* c, const char* code);

/* Starts an async REST fetch if the code is not cached or loading. */
void meta_cache_fetch(MetaCache* c, const char* code, FetchCompletedCb on_completed);

/* Applies one completed fetch. Call it from `on_completed`. Frees `r->data`. */
void meta_cache_on_fetched(MetaCache* c, const FetchResponse* r);

#endif /* CYBERIA_UI_META_CACHE_H */
