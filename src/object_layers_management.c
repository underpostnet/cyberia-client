#include "object_layers_management.h"
#include "config.h"
#include "hash_table.h"
#include "network/engine_client.h"
#include <raylib.h>
#include <cJSON.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <assert.h>

/* forward declaration */
static void parse_ws_direction_frames(cJSON* frames_json, AtlasSpriteSheetData* atlas);

ObjectLayersManager* g_olm_singleton = NULL;

// Track async atlas texture loading state
typedef enum {
    ATLAS_TEX_NONE,
    ATLAS_TEX_LOADING,
    ATLAS_TEX_READY,
    ATLAS_TEX_ERROR
} AtlasTextureState;

typedef struct {
    Texture2D         texture;
    AtlasTextureState state;
    double            last_access_time; /* wall-clock seconds for LRU eviction */
} AtlasTextureEntry;

/* Atlas metadata fetch set — value is sentinel; only key presence matters. */
#define META_SENTINEL ((void*)1)
static void noop_free(void* p) {}
static void free_layer_value(void* p) { free_object_layer((ObjectLayer*)p); }
static void free_atlas_value(void* p) { free_atlas_sprite_sheet_data((AtlasSpriteSheetData*)p); }
static void free_tex_value(void* p) {
    AtlasTextureEntry* e = (AtlasTextureEntry*)p;
    if (e->texture.id > 0) UnloadTexture(e->texture);
    free(e);
}

struct ObjectLayersManager {
    HashTable layers;    // item_id  → ObjectLayer*
    HashTable atlases;   // item_key → AtlasSpriteSheetData*
    HashTable textures;  // item_key → AtlasTextureEntry*
    HashTable meta;      // item_key → META_SENTINEL
};

// --- JSON Parsing Helpers ---

static char* json_get_string_safe(cJSON* item, const char* key, const char* default_val) {
    cJSON* obj = cJSON_GetObjectItemCaseSensitive(item, key);
    if (cJSON_IsString(obj) && (obj->valuestring != NULL)) {
        return strdup(obj->valuestring);
    }
    return default_val ? strdup(default_val) : NULL;
}

static int json_get_int_safe(cJSON* item, const char* key, int default_val) {
    cJSON* obj = cJSON_GetObjectItemCaseSensitive(item, key);
    if (cJSON_IsNumber(obj)) {
        return obj->valueint;
    }
    return default_val;
}

static bool json_get_bool_safe(cJSON* item, const char* key, bool default_val) {
    cJSON* obj = cJSON_GetObjectItemCaseSensitive(item, key);
    if (cJSON_IsBool(obj)) {
        return cJSON_IsTrue(obj);
    }
    return default_val;
}

// --- ObjectLayer JSON Parsing ---

static void parse_stats(cJSON* stats_json, Stats* stats) {
    assert(stats_json);
    assert(stats);
    stats->effect = json_get_int_safe(stats_json, "effect", 0);
    stats->resistance = json_get_int_safe(stats_json, "resistance", 0);
    stats->agility = json_get_int_safe(stats_json, "agility", 0);
    stats->range = json_get_int_safe(stats_json, "range", 0);
    stats->intelligence = json_get_int_safe(stats_json, "intelligence", 0);
    stats->utility = json_get_int_safe(stats_json, "utility", 0);
}

static void parse_render(cJSON* render_json, Render* render) {
    assert(render_json);
    assert(render);

    char* cid = json_get_string_safe(render_json, "cid", "");
    char* metadata_cid = json_get_string_safe(render_json, "metadataCid", "");

    strncpy(render->cid, cid, MAX_CID_LENGTH - 1);
    render->cid[MAX_CID_LENGTH - 1] = '\0';
    strncpy(render->metadata_cid, metadata_cid, MAX_CID_LENGTH - 1);
    render->metadata_cid[MAX_CID_LENGTH - 1] = '\0';

    free(cid);
    free(metadata_cid);
}

static void parse_ledger(cJSON* ledger_json, Ledger* ledger) {
    assert(ledger_json);
    assert(ledger);

    char* type_str = json_get_string_safe(ledger_json, "type", "OFF_CHAIN");
    ledger->type = ledger_type_from_string(type_str);
    free(type_str);

    char* address = json_get_string_safe(ledger_json, "address", "");
    strncpy(ledger->address, address, MAX_ADDRESS_LENGTH - 1);
    ledger->address[MAX_ADDRESS_LENGTH - 1] = '\0';
    free(address);
}

static void parse_item(cJSON* item_json, Item* item) {
    assert(item_json && item);

    char* id = json_get_string_safe(item_json, "id", "");
    char* type = json_get_string_safe(item_json, "type", "");
    char* desc = json_get_string_safe(item_json, "description", "");

    strncpy(item->id, id, MAX_ITEM_ID_LENGTH - 1);
    strncpy(item->type, type, MAX_TYPE_LENGTH - 1);
    strncpy(item->description, desc, MAX_DESCRIPTION_LENGTH - 1);
    item->activable = json_get_bool_safe(item_json, "activable", false);

    free(id);
    free(type);
    free(desc);
}

static void parse_object_layer_data(cJSON* data_json, ObjectLayerData* data) {
    assert(data_json);
    assert(data);

    parse_stats(cJSON_GetObjectItem(data_json, "stats"), &data->stats);
    parse_item(cJSON_GetObjectItem(data_json, "item"), &data->item);
    parse_ledger(cJSON_GetObjectItem(data_json, "ledger"), &data->ledger);
    parse_render(cJSON_GetObjectItem(data_json, "render"), &data->render);
}

// --- Atlas Sprite Sheet JSON Parsing ---

static void parse_direction_frame_data(cJSON* array_json, DirectionFrameData* dfd) {
    assert(dfd);
    assert(array_json);
    dfd->count = 0;

    if (!cJSON_IsArray(array_json)) return;

    int arr_size = cJSON_GetArraySize(array_json);
    if (arr_size > MAX_FRAMES_PER_DIRECTION) arr_size = MAX_FRAMES_PER_DIRECTION;

    for (int i = 0; i < arr_size; i++) {
        cJSON* frame_json = cJSON_GetArrayItem(array_json, i);
        if (!frame_json) continue;

        FrameMetadata* fm = &dfd->frames[dfd->count];
        fm->x = json_get_int_safe(frame_json, "x", 0);
        fm->y = json_get_int_safe(frame_json, "y", 0);
        fm->width = json_get_int_safe(frame_json, "width", 0);
        fm->height = json_get_int_safe(frame_json, "height", 0);
        fm->frame_index = json_get_int_safe(frame_json, "frameIndex", i);
        dfd->count++;
    }
}

ObjectLayer* lookup_cached_layer(const char* item_id) {
    assert(item_id);
    assert(g_olm_singleton);
    return (ObjectLayer*)hash_table_get(&g_olm_singleton->layers, item_id);
}

/* LRU eviction.
 *
 * When the textures table reaches MAX_TEXTURE_CACHE_SIZE entries, we walk
 * the table once, find the entry with the oldest last_access_time, and
 * remove it. The free_tex_value callback unloads the GPU texture; the
 * hash_table machinery frees the key. This keeps texture memory bounded
 * during long sessions across many maps without changing the cache hit
 * rate observed during typical play (recent textures win). */
static void evict_oldest_texture(void) {
    HashTable* t = &g_olm_singleton->textures;
    if (t->count < (size_t)MAX_TEXTURE_CACHE_SIZE) return;

    const char* oldest_key = NULL;
    double      oldest_at  = 1e18;
    for (size_t i = 0; i < t->capacity; i++) {
        HashSlot* s = &t->slots[i];
        if (s->state != SLOT_OCCUPIED || !s->value) continue;
        AtlasTextureEntry* e = (AtlasTextureEntry*)s->value;
        /* Never evict an entry still mid-fetch — its slot is needed by the
         * pending callback. */
        if (e->state == ATLAS_TEX_LOADING) continue;
        if (e->last_access_time < oldest_at) {
            oldest_at  = e->last_access_time;
            oldest_key = s->key;
        }
    }
    if (oldest_key) hash_table_remove(t, oldest_key);
}

/* Atlas PNG blob fetch — routed through engine_client. Callback decodes
 * PNG and uploads to GPU on the frame the fetch completes. */
static void on_atlas_blob_fetched(const FetchResponse* r) {
    AtlasTextureEntry* entry = hash_table_get(&g_olm_singleton->textures, r->asset_id);
    if (!entry) { free(r->data); return; }

    if (!r->success || !r->data || r->size == 0) {
        entry->state = ATLAS_TEX_ERROR;
        fprintf(stderr, "[WARN] Async fetch failed for atlas item_key: %s\n", r->asset_id);
        free(r->data);
        return;
    }

    Image image = LoadImageFromMemory(".png", r->data, (int)r->size);
    free(r->data);

    if (image.data == NULL) {
        entry->state = ATLAS_TEX_ERROR;
        fprintf(stderr, "[WARN] Failed to decode atlas PNG for item_key: %s\n", r->asset_id);
        return;
    }

    entry->texture = LoadTextureFromImage(image);
    UnloadImage(image);
    entry->state = ATLAS_TEX_READY;
    printf("[INFO] Atlas texture loaded for item_key: %s (%dx%d)\n",
           r->asset_id, entry->texture.width, entry->texture.height);
}

static Texture2D load_or_poll_atlas_texture(const char* item_key) {
    assert(item_key);
    assert(g_olm_singleton);

    AtlasTextureEntry* entry = hash_table_get(&g_olm_singleton->textures, item_key);
    if (entry) {
        entry->last_access_time = GetTime();
        return entry->state == ATLAS_TEX_READY ? entry->texture : (Texture2D){0};
    }

    evict_oldest_texture();

    entry = malloc(sizeof(AtlasTextureEntry));
    if (!entry) return (Texture2D){0};
    *entry = (AtlasTextureEntry){
        .state            = ATLAS_TEX_LOADING,
        .last_access_time = GetTime(),
    };
    hash_table_put(&g_olm_singleton->textures, item_key, entry);

    char url[512];
    snprintf(url, sizeof(url), "%s/api/atlas-sprite-sheet/blob/%s", API_BASE_URL, item_key);
    fetch_request_start(item_key, url, on_atlas_blob_fetched);
    return (Texture2D){0};
}

// ============================================================================
// Public API
// ============================================================================

void create_object_layers_manager(void) {
    assert(NULL == g_olm_singleton);

    ObjectLayersManager* mgr = malloc(sizeof(ObjectLayersManager));
    assert(mgr);

    /* Initial capacity aligned to the config caps so the typical session
     * never triggers the first hash_table resize. The tables still grow if
     * the cap is exceeded — load factor is maintained — but we avoid the
     * predictable churn at ≈179 textures (256 × 0.7). */
    hash_table_init(&mgr->layers,   (size_t)MAX_LAYER_CACHE_SIZE,   free_layer_value);
    hash_table_init(&mgr->atlases,  (size_t)MAX_ATLAS_CACHE_SIZE,   free_atlas_value);
    hash_table_init(&mgr->textures, (size_t)MAX_TEXTURE_CACHE_SIZE, free_tex_value);
    hash_table_init(&mgr->meta,     (size_t)MAX_ATLAS_CACHE_SIZE,   noop_free);

    g_olm_singleton = mgr;
}

ObjectLayersManager* obj_layers_mgr_get(void) {
    return g_olm_singleton;
}

void destroy_object_layers_manager(void) {
    if (NULL == g_olm_singleton) return;

    hash_table_destroy(&g_olm_singleton->layers);
    hash_table_destroy(&g_olm_singleton->atlases);
    hash_table_destroy(&g_olm_singleton->textures);
    hash_table_destroy(&g_olm_singleton->meta);

    free(g_olm_singleton);
    g_olm_singleton = NULL;
}

/* ── Callback for atlas metadata REST fetch (via engine_client pipeline) ─── */

static void on_atlas_meta_fetched(const FetchResponse* r) {
    if (!r->success) { free(r->data); return; }

    assert(g_olm_singleton);

    /* Parse REST response: { "data": { "metadata": { itemKey, atlasWidth, ... } } } */
    cJSON* root = cJSON_ParseWithLength((const char*)r->data, r->size);
    free(r->data);
    if (!root) return;

    cJSON* doc   = cJSON_GetObjectItem(root, "data");
    cJSON* rmeta = doc ? cJSON_GetObjectItem(doc, "metadata") : NULL;
    if (!rmeta) { cJSON_Delete(root); return; }

    char item_key[MAX_ITEM_ID_LENGTH];
    memset(item_key, 0, sizeof(item_key));
    {
        char* ik = json_get_string_safe(rmeta, "itemKey", "");
        strncpy(item_key, ik, MAX_ITEM_ID_LENGTH - 1);
        free(ik);
    }
    if (item_key[0] == '\0' || hash_table_contains(&g_olm_singleton->atlases, item_key)) {
        cJSON_Delete(root);
        return;
    }

    AtlasSpriteSheetData* atlas = create_atlas_sprite_sheet_data();
    if (!atlas) { cJSON_Delete(root); return; }

    strncpy(atlas->item_key, item_key, MAX_ITEM_ID_LENGTH - 1);
    atlas->atlas_width    = json_get_int_safe(rmeta, "atlasWidth",  0);
    atlas->atlas_height   = json_get_int_safe(rmeta, "atlasHeight", 0);
    atlas->cell_pixel_dim = json_get_int_safe(rmeta, "cellPixelDim", 20);
    atlas->frame_duration = json_get_int_safe(rmeta, "frame_duration", 100);
    cJSON* frames = cJSON_GetObjectItem(rmeta, "frames");
    if (frames) parse_ws_direction_frames(frames, atlas);

    hash_table_put(&g_olm_singleton->atlases, item_key, atlas);
    printf("[ATLAS REST] Metadata cached via callback for: %s (%dx%d)\n",
           item_key, atlas->atlas_width, atlas->atlas_height);

    /* Kick off PNG blob fetch now that metadata is cached */
    load_or_poll_atlas_texture(item_key);

    cJSON_Delete(root);
}

AtlasSpriteSheetData* get_or_fetch_atlas_data(const char* item_key) {
    assert(item_key);
    assert(g_olm_singleton);

    AtlasSpriteSheetData* atlas = hash_table_get(&g_olm_singleton->atlases, item_key);
    if (!atlas) {
        get_atlas_texture(item_key);
    }
    return atlas;
}

Texture2D get_atlas_texture(const char* item_key) {
    assert(item_key);
    assert(g_olm_singleton);

    if (hash_table_contains(&g_olm_singleton->atlases, item_key)) {
        return load_or_poll_atlas_texture(item_key);
    }

    if (!hash_table_contains(&g_olm_singleton->meta, item_key)) {
        obj_layers_mgr_schedule_atlas_fetch(item_key);
    }
    return (Texture2D){0};
}

// ============================================================================
// Cache Population from WebSocket Metadata
// ============================================================================

void obj_layers_mgr_schedule_atlas_fetch(const char* item_key) {
    assert(item_key);
    assert(g_olm_singleton);

    if (hash_table_contains(&g_olm_singleton->meta, item_key)) return;
    if (hash_table_contains(&g_olm_singleton->atlases, item_key)) return;

    hash_table_put(&g_olm_singleton->meta, item_key, META_SENTINEL);

    char url[512];
    snprintf(url, sizeof(url), "%s/api/atlas-sprite-sheet/metadata/%s", API_BASE_URL, item_key);
    fetch_request_start(item_key, url, on_atlas_meta_fetched);
    printf("[ATLAS REST] Fetch scheduled via engine_client: %s\n", item_key);
}

void populate_object_layer_from_json(const char* item_id, const cJSON* ol_json) {
    assert(ol_json);
    assert(item_id);
    assert(g_olm_singleton);

    ObjectLayer* layer = create_object_layer();
    if (!layer) return;

    char* sha = json_get_string_safe((cJSON*)ol_json, "sha256", "");
    strncpy(layer->sha256, sha, 64);
    free(sha);

    cJSON* data = cJSON_GetObjectItem((cJSON*)ol_json, "data");
    parse_object_layer_data(data, &layer->data);

    if (strlen(layer->data.item.id) == 0) {
        strncpy(layer->data.item.id, item_id, MAX_ITEM_ID_LENGTH - 1);
    }

    hash_table_put(&g_olm_singleton->layers, item_id, layer);
}

static void parse_ws_direction_frames(cJSON* frames_json, AtlasSpriteSheetData* atlas) {
    assert(frames_json);
    assert(atlas);
    parse_direction_frame_data(cJSON_GetObjectItem(frames_json, "up_idle"), &atlas->up_idle);
    parse_direction_frame_data(cJSON_GetObjectItem(frames_json, "down_idle"), &atlas->down_idle);
    parse_direction_frame_data(cJSON_GetObjectItem(frames_json, "right_idle"), &atlas->right_idle);
    parse_direction_frame_data(cJSON_GetObjectItem(frames_json, "left_idle"), &atlas->left_idle);
    parse_direction_frame_data(cJSON_GetObjectItem(frames_json, "up_right_idle"), &atlas->up_right_idle);
    parse_direction_frame_data(cJSON_GetObjectItem(frames_json, "down_right_idle"), &atlas->down_right_idle);
    parse_direction_frame_data(cJSON_GetObjectItem(frames_json, "up_left_idle"), &atlas->up_left_idle);
    parse_direction_frame_data(cJSON_GetObjectItem(frames_json, "down_left_idle"), &atlas->down_left_idle);
    parse_direction_frame_data(cJSON_GetObjectItem(frames_json, "default_idle"), &atlas->default_idle);
    parse_direction_frame_data(cJSON_GetObjectItem(frames_json, "up_walking"), &atlas->up_walking);
    parse_direction_frame_data(cJSON_GetObjectItem(frames_json, "down_walking"), &atlas->down_walking);
    parse_direction_frame_data(cJSON_GetObjectItem(frames_json, "right_walking"), &atlas->right_walking);
    parse_direction_frame_data(cJSON_GetObjectItem(frames_json, "left_walking"), &atlas->left_walking);
    parse_direction_frame_data(cJSON_GetObjectItem(frames_json, "up_right_walking"), &atlas->up_right_walking);
    parse_direction_frame_data(cJSON_GetObjectItem(frames_json, "down_right_walking"), &atlas->down_right_walking);
    parse_direction_frame_data(cJSON_GetObjectItem(frames_json, "up_left_walking"), &atlas->up_left_walking);
    parse_direction_frame_data(cJSON_GetObjectItem(frames_json, "down_left_walking"), &atlas->down_left_walking);
    parse_direction_frame_data(cJSON_GetObjectItem(frames_json, "none_idle"), &atlas->none_idle);
}

/* Dead — no external callers. Kept commented for reference; the REST path
 * (obj_layers_mgr_schedule_atlas_fetch → on_atlas_meta_fetched) supersedes
 * the WS-driven population this function provided.
 *
 * void populate_atlas_from_json(ObjectLayersManager* manager, const char* item_key, const cJSON* atlas_json) {
 *     assert(manager && item_key && atlas_json);
 *
 *     AtlasSpriteSheetData* atlas = create_atlas_sprite_sheet_data();
 *     if (!atlas) return;
 *
 *     char* file_id = json_get_string_safe((cJSON*)atlas_json, "fileId", "");
 *     strncpy(atlas->file_id, file_id, MAX_FILE_ID_LENGTH - 1);
 *     free(file_id);
 *
 *     strncpy(atlas->item_key, item_key, MAX_ITEM_ID_LENGTH - 1);
 *     atlas->atlas_width = json_get_int_safe((cJSON*)atlas_json, "atlasWidth", 0);
 *     atlas->atlas_height = json_get_int_safe((cJSON*)atlas_json, "atlasHeight", 0);
 *     atlas->cell_pixel_dim = json_get_int_safe((cJSON*)atlas_json, "cellPixelDim", 20);
 *     atlas->frame_duration = json_get_int_safe((cJSON*)atlas_json, "frame_duration", 100);
 *
 *     cJSON* frames = cJSON_GetObjectItem((cJSON*)atlas_json, "frames");
 *     if (frames) parse_ws_direction_frames(frames, atlas);
 *
 *     cache_atlas_data(manager, item_key, atlas);
 *
 *     if (item_key[0] != '\0') {
 *         load_or_poll_atlas_texture(manager, item_key);
 *     }
 *
 *     printf("[INFO] Atlas populated from WS for: %s (%dx%d)\n",
 *            item_key, atlas->atlas_width, atlas->atlas_height);
 * }
 */

