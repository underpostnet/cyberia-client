#include "object_layers_management.h"
#include "config.h"
#include "network/engine_client.h"
#include <raylib.h>
#include <cJSON.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "helper.h"
#include <assert.h>

#include "js/services.h"

#define HASH_TABLE_SIZE 256

// -----------------------------------------------------------------------
// Singleton for metadata/PNG fetch callbacks
// -----------------------------------------------------------------------
// The engine_client callback signature doesn't carry user context, so
// on_atlas_meta_fetched finds the manager via this module-global.
// Defined at the top so every function can reference it.
ObjectLayersManager* g_olm_singleton = NULL;

// --- Data Structures ---

typedef struct ObjectLayerEntry {
    char* key;
    ObjectLayer* layer;
    struct ObjectLayerEntry* next;
} ObjectLayerEntry;

typedef struct AtlasEntry {
    char* key;
    AtlasSpriteSheetData* atlas;
    struct AtlasEntry* next;
} AtlasEntry;

// Track async atlas texture loading state
typedef enum {
    ATLAS_TEX_NONE,
    ATLAS_TEX_LOADING,
    ATLAS_TEX_READY,
    ATLAS_TEX_ERROR
} AtlasTextureState;

typedef struct AtlasTextureEntry {
    char* item_key;
    Texture2D texture;
    AtlasTextureState state;
    uint32_t request_id;
    struct AtlasTextureEntry* next;
} AtlasTextureEntry;

// --- Atlas metadata REST fetch state ---

typedef enum {
    ATLAS_META_NONE = 0,
    ATLAS_META_FETCHING,
    ATLAS_META_DONE,
    ATLAS_META_ERROR
} AtlasMetaState;

typedef struct AtlasMetaFetchEntry {
    char* item_key;
    AtlasMetaState state;
    struct AtlasMetaFetchEntry* next;
} AtlasMetaFetchEntry;

struct ObjectLayersManager {
    // Object layer metadata cache (keyed by item_id)
    ObjectLayerEntry* layer_buckets[HASH_TABLE_SIZE];

    // Atlas sprite sheet data cache (keyed by item_key)
    AtlasEntry* atlas_buckets[HASH_TABLE_SIZE];

    // Atlas texture cache (keyed by item_key)
    AtlasTextureEntry* tex_buckets[HASH_TABLE_SIZE];

    // Atlas metadata REST fetch state (keyed by item_key)
    AtlasMetaFetchEntry* meta_buckets[HASH_TABLE_SIZE];

    // Per-frame texture load budget — reset each frame via obj_layers_mgr_reset_frame_budget()
    int tex_loads_this_frame;
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
    assert(stats_json && stats);
    stats->effect = json_get_int_safe(stats_json, "effect", 0);
    stats->resistance = json_get_int_safe(stats_json, "resistance", 0);
    stats->agility = json_get_int_safe(stats_json, "agility", 0);
    stats->range = json_get_int_safe(stats_json, "range", 0);
    stats->intelligence = json_get_int_safe(stats_json, "intelligence", 0);
    stats->utility = json_get_int_safe(stats_json, "utility", 0);
}

static void parse_render(cJSON* render_json, Render* render) {
    assert(render_json && render);

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
    assert(ledger_json && ledger);

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
    assert(data_json && data);

    parse_stats(cJSON_GetObjectItem(data_json, "stats"), &data->stats);
    parse_item(cJSON_GetObjectItem(data_json, "item"), &data->item);
    parse_ledger(cJSON_GetObjectItem(data_json, "ledger"), &data->ledger);
    parse_render(cJSON_GetObjectItem(data_json, "render"), &data->render);
}

// --- Atlas Sprite Sheet JSON Parsing ---

static void parse_direction_frame_data(cJSON* array_json, DirectionFrameData* dfd) {
    assert(dfd);
    dfd->count = 0;

    if (!array_json || !cJSON_IsArray(array_json)) return;

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

// --- ObjectLayer Cache Operations ---

static void cache_object_layer(ObjectLayersManager* manager, const char* item_id, ObjectLayer* layer) {
    assert(manager && item_id);

    unsigned long index = hash_string(item_id) % HASH_TABLE_SIZE;

    ObjectLayerEntry* entry = manager->layer_buckets[index];
    while (entry) {
        if (strcmp(entry->key, item_id) == 0) {
            if (entry->layer) free_object_layer(entry->layer);
            entry->layer = layer;
            return;
        }
        entry = entry->next;
    }

    ObjectLayerEntry* new_entry = (ObjectLayerEntry*)malloc(sizeof(ObjectLayerEntry));
    if (!new_entry) return;

    new_entry->key = strdup(item_id);
    new_entry->layer = layer;
    new_entry->next = manager->layer_buckets[index];
    manager->layer_buckets[index] = new_entry;
}

static ObjectLayer* lookup_cached_layer(ObjectLayersManager* manager, const char* item_id) {
    assert(manager && item_id);

    unsigned long index = hash_string(item_id) % HASH_TABLE_SIZE;
    ObjectLayerEntry* entry = manager->layer_buckets[index];

    while (entry) {
        if (strcmp(entry->key, item_id) == 0) {
            return entry->layer;
        }
        entry = entry->next;
    }
    return NULL;
}

// --- Atlas Cache Operations ---

static void cache_atlas_data(ObjectLayersManager* manager, const char* item_key, AtlasSpriteSheetData* atlas) {
    assert(manager && item_key);

    unsigned long index = hash_string(item_key) % HASH_TABLE_SIZE;

    AtlasEntry* entry = manager->atlas_buckets[index];
    while (entry) {
        if (strcmp(entry->key, item_key) == 0) {
            if (entry->atlas) free_atlas_sprite_sheet_data(entry->atlas);
            entry->atlas = atlas;
            return;
        }
        entry = entry->next;
    }

    AtlasEntry* new_entry = (AtlasEntry*)malloc(sizeof(AtlasEntry));
    if (!new_entry) return;

    new_entry->key = strdup(item_key);
    new_entry->atlas = atlas;
    new_entry->next = manager->atlas_buckets[index];
    manager->atlas_buckets[index] = new_entry;
}

static AtlasSpriteSheetData* lookup_cached_atlas(ObjectLayersManager* manager, const char* item_key) {
    assert(manager && item_key);

    unsigned long index = hash_string(item_key) % HASH_TABLE_SIZE;
    AtlasEntry* entry = manager->atlas_buckets[index];

    while (entry) {
        if (strcmp(entry->key, item_key) == 0) {
            return entry->atlas;
        }
        entry = entry->next;
    }
    return NULL;
}

// --- Atlas Texture Cache Operations ---

static AtlasTextureEntry* lookup_tex_entry(ObjectLayersManager* manager, const char* item_key) {
    assert(manager && item_key);

    unsigned long index = hash_string(item_key) % HASH_TABLE_SIZE;
    AtlasTextureEntry* entry = manager->tex_buckets[index];

    while (entry) {
        if (strcmp(entry->item_key, item_key) == 0) {
            return entry;
        }
        entry = entry->next;
    }
    return NULL;
}

static AtlasTextureEntry* create_tex_entry(ObjectLayersManager* manager, const char* item_key) {
    assert(manager && item_key);

    AtlasTextureEntry* entry = (AtlasTextureEntry*)malloc(sizeof(AtlasTextureEntry));
    if (!entry) return NULL;

    entry->item_key = strdup(item_key);
    entry->texture = (Texture2D){0};
    entry->state = ATLAS_TEX_NONE;
    entry->request_id = 0;
    entry->next = NULL;

    unsigned long index = hash_string(item_key) % HASH_TABLE_SIZE;
    entry->next = manager->tex_buckets[index];
    manager->tex_buckets[index] = entry;

    return entry;
}

/*
 * Texture (PNG blob) fetch — uses direct js_start_fetch_binary / js_get_fetch_result
 * with a HIGH request ID offset (500000+) to avoid collisions with the
 * engine_client callback pipeline (which uses IDs starting at 1).
 *
 * Per-frame budget is enforced via tex_loads_this_frame so PNG decode + GPU
 * upload is spread across multiple frames.
 */
#define MAX_TEX_LOADS_PER_FRAME 8
/* Offset for texture fetch request IDs — far above engine_client (~512 max) */
#define OLM_REQUEST_ID_OFFSET 500000

static uint32_t s_tex_req_counter = 0;

static Texture2D load_or_poll_atlas_texture(ObjectLayersManager* manager, const char* item_key) {
    if (!manager || !item_key || item_key[0] == '\0') return (Texture2D){0};

    AtlasTextureEntry* entry = lookup_tex_entry(manager, item_key);

    if (!entry) {
        entry = create_tex_entry(manager, item_key);
        if (!entry) return (Texture2D){0};

        entry->request_id = OLM_REQUEST_ID_OFFSET + (++s_tex_req_counter);
        entry->state = ATLAS_TEX_LOADING;

        char url[512];
        snprintf(url, sizeof(url), "%s/api/atlas-sprite-sheet/blob/%s", API_BASE_URL, item_key);

        js_start_fetch_binary(url, entry->request_id);
        return (Texture2D){0};
    }

    if (entry->state == ATLAS_TEX_READY) {
        return entry->texture;
    }

    if (entry->state == ATLAS_TEX_ERROR) {
        return (Texture2D){0};
    }

    if (entry->state == ATLAS_TEX_LOADING) {
        if (manager->tex_loads_this_frame >= MAX_TEX_LOADS_PER_FRAME) {
            return (Texture2D){0};
        }
        int size = 0;
        unsigned char* data = js_get_fetch_result(entry->request_id, &size);

        if (data && size > 0) {
            manager->tex_loads_this_frame++;
            Image image = LoadImageFromMemory(".png", data, size);
            if (image.data != NULL) {
                entry->texture = LoadTextureFromImage(image);
                UnloadImage(image);
                entry->state = ATLAS_TEX_READY;
                printf("[INFO] Atlas texture loaded for item_key: %s (%dx%d)\n",
                       item_key, entry->texture.width, entry->texture.height);
            } else {
                entry->state = ATLAS_TEX_ERROR;
                fprintf(stderr, "[WARN] Failed to decode atlas PNG for item_key: %s\n", item_key);
            }
            free(data);
        } else if (size == -1) {
            entry->state = ATLAS_TEX_ERROR;
            fprintf(stderr, "[WARN] Async fetch failed for atlas item_key: %s\n", item_key);
        }
    }

    return entry->state == ATLAS_TEX_READY ? entry->texture : (Texture2D){0};
}

// ============================================================================
// Public API
// ============================================================================

ObjectLayersManager* create_object_layers_manager(void) {
    ObjectLayersManager* manager = malloc(sizeof(ObjectLayersManager));
    if (!manager) return NULL;

    for (int i = 0; i < HASH_TABLE_SIZE; i++) {
        manager->layer_buckets[i] = NULL;
        manager->atlas_buckets[i] = NULL;
        manager->tex_buckets[i] = NULL;
        manager->meta_buckets[i] = NULL;
    }

    manager->tex_loads_this_frame = 0;

    // Set singleton for atlas metadata fetch callback
    g_olm_singleton = manager;

    return manager;
}

void destroy_object_layers_manager(ObjectLayersManager* manager) {
    if (!manager) return;

    for (int i = 0; i < HASH_TABLE_SIZE; i++) {
        ObjectLayerEntry* entry = manager->layer_buckets[i];
        while (entry) {
            ObjectLayerEntry* next = entry->next;
            if (entry->layer) free_object_layer(entry->layer);
            free(entry->key);
            free(entry);
            entry = next;
        }
        manager->layer_buckets[i] = NULL;
    }

    for (int i = 0; i < HASH_TABLE_SIZE; i++) {
        AtlasEntry* entry = manager->atlas_buckets[i];
        while (entry) {
            AtlasEntry* next = entry->next;
            if (entry->atlas) free_atlas_sprite_sheet_data(entry->atlas);
            free(entry->key);
            free(entry);
            entry = next;
        }
        manager->atlas_buckets[i] = NULL;
    }

    for (int i = 0; i < HASH_TABLE_SIZE; i++) {
        AtlasTextureEntry* entry = manager->tex_buckets[i];
        while (entry) {
            AtlasTextureEntry* next = entry->next;
            if (entry->texture.id > 0) {
                UnloadTexture(entry->texture);
            }
            free(entry->item_key);
            free(entry);
            entry = next;
        }
        manager->tex_buckets[i] = NULL;
    }

    for (int i = 0; i < HASH_TABLE_SIZE; i++) {
        AtlasMetaFetchEntry* entry = manager->meta_buckets[i];
        while (entry) {
            AtlasMetaFetchEntry* next = entry->next;
            free(entry->item_key);
            free(entry);
            entry = next;
        }
        manager->meta_buckets[i] = NULL;
    }

    free(manager);
}

ObjectLayer* get_or_fetch_object_layer(ObjectLayersManager* manager, const char* item_id) {
    assert(manager && item_id);
    return lookup_cached_layer(manager, item_id);
}

/* forward declaration */
static void parse_ws_direction_frames(cJSON* frames_json, AtlasSpriteSheetData* atlas);

/* ── Callback for atlas metadata REST fetch (via engine_client pipeline) ─── */

static void on_atlas_meta_fetched(uint32_t request_id, FetchState state, void* data, size_t size) {
    (void)request_id;
    if (FETCH_STATE_READY != state) { free(data); return; }

    /* Retrieve the manager via the module-level singleton. */
    ObjectLayersManager* manager = g_olm_singleton;
    if (!manager) { free(data); return; }

    /* Parse REST response: { "data": { "metadata": { itemKey, atlasWidth, ... } } } */
    cJSON* root = cJSON_ParseWithLength((const char*)data, (size_t)size);
    free(data);
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
    if (item_key[0] == '\0' || lookup_cached_atlas(manager, item_key)) {
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

    cache_atlas_data(manager, item_key, atlas);
    printf("[ATLAS REST] Metadata cached via callback for: %s (%dx%d)\n",
           item_key, atlas->atlas_width, atlas->atlas_height);

    /* Update meta state */
    unsigned long meta_idx = hash_string(item_key) % HASH_TABLE_SIZE;
    for (AtlasMetaFetchEntry* e = manager->meta_buckets[meta_idx]; e; e = e->next) {
        if (strcmp(e->item_key, item_key) == 0) { e->state = ATLAS_META_DONE; break; }
    }

    /* Kick off PNG blob fetch now that metadata is cached */
    load_or_poll_atlas_texture(manager, item_key);

    cJSON_Delete(root);
}

AtlasSpriteSheetData* get_or_fetch_atlas_data(ObjectLayersManager* manager, const char* item_key) {
    assert(manager && item_key);

    AtlasSpriteSheetData* atlas = lookup_cached_atlas(manager, item_key);
    if (!atlas) {
        get_atlas_texture(manager, item_key);
    }
    return atlas;
}

static void parse_ws_direction_frames(cJSON* frames_json, AtlasSpriteSheetData* atlas);

Texture2D get_atlas_texture(ObjectLayersManager* manager, const char* item_key) {
    if (!manager || !item_key || item_key[0] == '\0') return (Texture2D){0};

    if (lookup_cached_atlas(manager, item_key)) {
        return load_or_poll_atlas_texture(manager, item_key);
    }

    unsigned long meta_idx = hash_string(item_key) % HASH_TABLE_SIZE;
    AtlasMetaFetchEntry* meta = NULL;
    {
        AtlasMetaFetchEntry* e = manager->meta_buckets[meta_idx];
        while (e) {
            if (strcmp(e->item_key, item_key) == 0) { meta = e; break; }
            e = e->next;
        }
    }

    if (!meta) {
        obj_layers_mgr_schedule_atlas_fetch(manager, item_key);
        return (Texture2D){0};
    }

    return (Texture2D){0};
}

// ============================================================================
// Cache Population from WebSocket Metadata
// ============================================================================

void obj_layers_mgr_reset_frame_budget(ObjectLayersManager* manager) {
    if (manager) manager->tex_loads_this_frame = 0;
}

void obj_layers_mgr_schedule_atlas_fetch(ObjectLayersManager* manager, const char* item_key) {
    if (!manager || !item_key || item_key[0] == '\0') return;

    unsigned long index = hash_string(item_key) % HASH_TABLE_SIZE;
    AtlasMetaFetchEntry* e = manager->meta_buckets[index];
    while (e) {
        if (strcmp(e->item_key, item_key) == 0) return;
        e = e->next;
    }
    if (lookup_cached_atlas(manager, item_key)) return;

    AtlasMetaFetchEntry* entry = (AtlasMetaFetchEntry*)malloc(sizeof(AtlasMetaFetchEntry));
    if (!entry) return;
    entry->item_key = strdup(item_key);
    entry->state = ATLAS_META_FETCHING;
    entry->next = manager->meta_buckets[index];
    manager->meta_buckets[index] = entry;

    char url[512];
    snprintf(url, sizeof(url), "%s/api/atlas-sprite-sheet/metadata/%s", API_BASE_URL, item_key);
    uint32_t req_id = fetch_request_start(url, on_atlas_meta_fetched);
    if (req_id == 0) {
        entry->state = ATLAS_META_ERROR;
        fprintf(stderr, "[ATLAS REST] Failed to schedule fetch for: %s\n", item_key);
        return;
    }
    printf("[ATLAS REST] Fetch scheduled via engine_client: %s\n", item_key);
}

void populate_object_layer_from_json(ObjectLayersManager* manager, const char* item_id, const cJSON* ol_json) {
    assert(manager && item_id && ol_json);

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

    cache_object_layer(manager, item_id, layer);
}

static void parse_ws_direction_frames(cJSON* frames_json, AtlasSpriteSheetData* atlas) {
    assert(frames_json && atlas);
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

void populate_atlas_from_json(ObjectLayersManager* manager, const char* item_key, const cJSON* atlas_json) {
    assert(manager && item_key && atlas_json);

    AtlasSpriteSheetData* atlas = create_atlas_sprite_sheet_data();
    if (!atlas) return;

    char* file_id = json_get_string_safe((cJSON*)atlas_json, "fileId", "");
    strncpy(atlas->file_id, file_id, MAX_FILE_ID_LENGTH - 1);
    free(file_id);

    strncpy(atlas->item_key, item_key, MAX_ITEM_ID_LENGTH - 1);
    atlas->atlas_width = json_get_int_safe((cJSON*)atlas_json, "atlasWidth", 0);
    atlas->atlas_height = json_get_int_safe((cJSON*)atlas_json, "atlasHeight", 0);
    atlas->cell_pixel_dim = json_get_int_safe((cJSON*)atlas_json, "cellPixelDim", 20);
    atlas->frame_duration = json_get_int_safe((cJSON*)atlas_json, "frame_duration", 100);

    cJSON* frames = cJSON_GetObjectItem((cJSON*)atlas_json, "frames");
    if (frames) parse_ws_direction_frames(frames, atlas);

    cache_atlas_data(manager, item_key, atlas);

    if (item_key[0] != '\0') {
        load_or_poll_atlas_texture(manager, item_key);
    }

    printf("[INFO] Atlas populated from WS for: %s (%dx%d)\n",
           item_key, atlas->atlas_width, atlas->atlas_height);
}

