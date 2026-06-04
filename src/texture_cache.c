#include "texture_cache.h"

#include "hash_table.h"
#include "util/log.h"

#include <assert.h>
#include <stdlib.h>

typedef enum {
    TEX_LOADING,
    TEX_READY,
    TEX_ERROR
} TexState;

typedef struct {
    Texture2D texture;
    TexState  state;
    double    last_access_time; /* wall-clock seconds for LRU eviction */
} TexEntry;

struct TextureCache {
    HashTable        entries;   /* url → TexEntry* */
    int              capacity;
    FetchCompletedCb on_blob;
};

static void free_entry(void* p) {
    TexEntry* e = p;
    if (e->texture.id > 0) { UnloadTexture(e->texture); }
    free(e);
}

TextureCache* texture_cache_create(int capacity, const char* debug_name, FetchCompletedCb on_blob) {
    assert(capacity > 0);
    assert(debug_name);
    assert(on_blob);
    TextureCache* tc = malloc(sizeof(TextureCache));
    assert(tc);
    hash_table_init(&tc->entries, (size_t)capacity, free_entry, debug_name);
    tc->capacity = capacity;
    tc->on_blob  = on_blob;
    return tc;
}

void texture_cache_destroy(TextureCache* tc) {
    if (!tc) { return; }
    hash_table_destroy(&tc->entries);
    free(tc);
}

/* Evict the least-recently-accessed ready entry when at capacity. Entries
 * still mid-fetch are skipped — their slot is needed by the pending callback. */
static void evict_oldest(TextureCache* tc) {
    HashTable* t = &tc->entries;
    if (t->count < (size_t)tc->capacity) { return; }

    const char* oldest_key = NULL;
    double      oldest_at   = 1e18;
    for (size_t i = 0; i < t->capacity; i++) {
        HashSlot* s = &t->slots[i];
        if (SLOT_OCCUPIED != s->state || !s->value) { continue; }
        TexEntry* e = s->value;
        if (TEX_LOADING == e->state) { continue; }
        if (e->last_access_time < oldest_at) {
            oldest_at  = e->last_access_time;
            oldest_key = s->key;
        }
    }
    if (oldest_key) { hash_table_remove(t, oldest_key); }
}

Texture2D texture_cache_get(TextureCache* tc, const char* url) {
    assert(tc);
    assert(url);

    TexEntry* e = hash_table_get(&tc->entries, url);
    if (e) {
        e->last_access_time = GetTime();
        return TEX_READY == e->state ? e->texture : (Texture2D){0};
    }

    evict_oldest(tc);

    e = malloc(sizeof(TexEntry));
    assert(e);
    *e = (TexEntry){ .state = TEX_LOADING, .last_access_time = GetTime() };
    hash_table_put(&tc->entries, url, e);

    /* asset_id == url so the completion routes back to this same key. */
    fetch_request_start(url, url, tc->on_blob);
    return (Texture2D){0};
}

void texture_cache_on_blob_fetched(TextureCache* tc, const FetchResponse* r) {
    assert(tc);
    assert(r);

    TexEntry* e = hash_table_get(&tc->entries, r->asset_id);
    if (!e) { free(r->data); return; }

    if (!r->success || !r->data || 0 == r->size) {
        e->state = TEX_ERROR;
        LOG_ERROR("[TEXCACHE] fetch failed: %s", r->asset_id);
        free(r->data);
        return;
    }

    Image image = LoadImageFromMemory(".png", r->data, (int)r->size);
    free(r->data);
    if (NULL == image.data) {
        e->state = TEX_ERROR;
        LOG_ERROR("[TEXCACHE] PNG decode failed: %s", r->asset_id);
        return;
    }

    e->texture = LoadTextureFromImage(image);
    UnloadImage(image);
    e->state = TEX_READY;
    LOG_INFO("[TEXCACHE] loaded: %s (%dx%d)", r->asset_id, e->texture.width, e->texture.height);
}

void texture_cache_evict(TextureCache* tc, const char* url) {
    assert(tc);
    assert(url);
    hash_table_remove(&tc->entries, url);
}
