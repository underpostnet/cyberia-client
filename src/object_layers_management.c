#include "object_layers_management.h"
#include "config.h"
#include "cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <emscripten/emscripten.h>

// External JS functions
extern char* js_fetch_object_layer(const char* item_id);
extern char* js_fetch_atlas_sprite_sheet(const char* item_key);
extern void js_init_engine_api(const char* api_base_url, const char* email, const char* password);

// External JS functions for async binary fetching (reused for atlas PNG blobs)
extern void js_start_fetch_binary(const char* url, int request_id);
extern unsigned char* js_get_fetch_result(int request_id, int* size);

#define HASH_TABLE_SIZE 256

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
    char* file_id;
    Texture2D texture;
    AtlasTextureState state;
    int request_id;
    struct AtlasTextureEntry* next;
} AtlasTextureEntry;

struct ObjectLayersManager {
    // Object layer metadata cache (keyed by item_id)
    ObjectLayerEntry* layer_buckets[HASH_TABLE_SIZE];

    // Atlas sprite sheet data cache (keyed by item_key)
    AtlasEntry* atlas_buckets[HASH_TABLE_SIZE];

    // Atlas texture cache (keyed by file_id)
    AtlasTextureEntry* tex_buckets[HASH_TABLE_SIZE];

    TextureManager* texture_manager;

    // Authentication state
    bool authenticated;

    // Request ID counter for async fetches
    int next_request_id;
};

// --- Helper Functions ---

static unsigned long hash_string(const char* str) {
    unsigned long hash = 5381;
    int c;
    while ((c = *str++)) {
        hash = ((hash << 5) + hash) + c;
    }
    return hash;
}

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

static int json_array_count(cJSON* array_obj) {
    if (cJSON_IsArray(array_obj)) {
        return cJSON_GetArraySize(array_obj);
    }
    return 0;
}

// --- ObjectLayer JSON Parsing ---

static void parse_stats(cJSON* stats_json, Stats* stats) {
    if (!stats_json || !stats) return;
    stats->effect = json_get_int_safe(stats_json, "effect", 0);
    stats->resistance = json_get_int_safe(stats_json, "resistance", 0);
    stats->agility = json_get_int_safe(stats_json, "agility", 0);
    stats->range = json_get_int_safe(stats_json, "range", 0);
    stats->intelligence = json_get_int_safe(stats_json, "intelligence", 0);
    stats->utility = json_get_int_safe(stats_json, "utility", 0);
}

static void parse_render_frames(cJSON* frames_json, RenderFrames* frames) {
    if (!frames_json || !frames) return;

    frames->up_idle_count = json_array_count(cJSON_GetObjectItem(frames_json, "up_idle"));
    frames->down_idle_count = json_array_count(cJSON_GetObjectItem(frames_json, "down_idle"));
    frames->left_idle_count = json_array_count(cJSON_GetObjectItem(frames_json, "left_idle"));
    frames->right_idle_count = json_array_count(cJSON_GetObjectItem(frames_json, "right_idle"));

    frames->up_left_idle_count = json_array_count(cJSON_GetObjectItem(frames_json, "up_left_idle"));
    frames->up_right_idle_count = json_array_count(cJSON_GetObjectItem(frames_json, "up_right_idle"));
    frames->down_left_idle_count = json_array_count(cJSON_GetObjectItem(frames_json, "down_left_idle"));
    frames->down_right_idle_count = json_array_count(cJSON_GetObjectItem(frames_json, "down_right_idle"));

    frames->default_idle_count = json_array_count(cJSON_GetObjectItem(frames_json, "default_idle"));
    frames->none_idle_count = json_array_count(cJSON_GetObjectItem(frames_json, "none_idle"));

    frames->up_walking_count = json_array_count(cJSON_GetObjectItem(frames_json, "up_walking"));
    frames->down_walking_count = json_array_count(cJSON_GetObjectItem(frames_json, "down_walking"));
    frames->left_walking_count = json_array_count(cJSON_GetObjectItem(frames_json, "left_walking"));
    frames->right_walking_count = json_array_count(cJSON_GetObjectItem(frames_json, "right_walking"));

    frames->up_left_walking_count = json_array_count(cJSON_GetObjectItem(frames_json, "up_left_walking"));
    frames->up_right_walking_count = json_array_count(cJSON_GetObjectItem(frames_json, "up_right_walking"));
    frames->down_left_walking_count = json_array_count(cJSON_GetObjectItem(frames_json, "down_left_walking"));
    frames->down_right_walking_count = json_array_count(cJSON_GetObjectItem(frames_json, "down_right_walking"));
}

static void parse_render(cJSON* render_json, Render* render) {
    if (!render_json || !render) return;

    render->frame_duration = json_get_int_safe(render_json, "frame_duration", 100);
    render->is_stateless = json_get_bool_safe(render_json, "is_stateless", false);

    cJSON* frames = cJSON_GetObjectItem(render_json, "frames");
    parse_render_frames(frames, &render->frames);
}

static void parse_item(cJSON* item_json, Item* item) {
    if (!item_json || !item) return;

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
    if (!data_json || !data) return;

    parse_stats(cJSON_GetObjectItem(data_json, "stats"), &data->stats);
    parse_render(cJSON_GetObjectItem(data_json, "render"), &data->render);
    parse_item(cJSON_GetObjectItem(data_json, "item"), &data->item);
}

static ObjectLayer* parse_object_layer_json(const char* json_str) {
    if (!json_str) return NULL;

    cJSON* root = cJSON_Parse(json_str);
    if (!root) {
        fprintf(stderr, "[ERROR] Failed to parse ObjectLayer JSON\n");
        return NULL;
    }

    ObjectLayer* layer = create_object_layer();
    if (!layer) {
        cJSON_Delete(root);
        return NULL;
    }

    // Extract sha256
    char* sha = json_get_string_safe(root, "sha256", "");
    strncpy(layer->sha256, sha, 64);
    free(sha);

    // Extract data
    cJSON* data = cJSON_GetObjectItem(root, "data");
    parse_object_layer_data(data, &layer->data);

    // Ensure item ID is set in data.item if missing (fallback to root id)
    if (strlen(layer->data.item.id) == 0) {
        char* root_id = json_get_string_safe(root, "id", "");
        strncpy(layer->data.item.id, root_id, MAX_ITEM_ID_LENGTH - 1);
        free(root_id);
    }

    // Ensure item type is set
    if (strlen(layer->data.item.type) == 0) {
        char* root_type = json_get_string_safe(root, "type", "");
        strncpy(layer->data.item.type, root_type, MAX_TYPE_LENGTH - 1);
        free(root_type);
    }

    cJSON_Delete(root);
    return layer;
}

// --- Atlas Sprite Sheet JSON Parsing ---

/**
 * Parse a single direction's frame metadata array from JSON into DirectionFrameData.
 *
 * Each element in the JSON array is a FrameMetadataSchema object:
 *   { "x": N, "y": N, "width": N, "height": N, "frameIndex": N }
 */
static void parse_direction_frame_data(cJSON* array_json, DirectionFrameData* dfd) {
    if (!dfd) return;
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

/**
 * Parse the atlas sprite sheet JSON response from the API.
 *
 * Expected response structure (after extracting from envelope):
 * {
 *   "_id": "...",
 *   "fileId": { "_id": "hexstring", ... },   // populated, sans data buffer
 *   "metadata": {
 *     "itemKey": "anon",
 *     "atlasWidth": 400,
 *     "atlasHeight": 800,
 *     "cellPixelDim": 20,
 *     "frames": {
 *       "up_idle": [ { x, y, width, height, frameIndex }, ... ],
 *       "down_idle": [ ... ],
 *       ...
 *     }
 *   }
 * }
 */
static AtlasSpriteSheetData* parse_atlas_sprite_sheet_json(cJSON* atlas_json) {
    if (!atlas_json) return NULL;

    AtlasSpriteSheetData* atlas = create_atlas_sprite_sheet_data();
    if (!atlas) return NULL;

    // Extract fileId._id (populated reference)
    cJSON* file_id_obj = cJSON_GetObjectItem(atlas_json, "fileId");
    if (file_id_obj) {
        if (cJSON_IsObject(file_id_obj)) {
            // Populated: fileId is an object with _id field
            char* fid = json_get_string_safe(file_id_obj, "_id", "");
            strncpy(atlas->file_id, fid, MAX_FILE_ID_LENGTH - 1);
            free(fid);
        } else if (cJSON_IsString(file_id_obj)) {
            // Not populated: fileId is a plain string ObjectId
            strncpy(atlas->file_id, file_id_obj->valuestring, MAX_FILE_ID_LENGTH - 1);
        }
    }

    // Extract metadata
    cJSON* metadata = cJSON_GetObjectItem(atlas_json, "metadata");
    if (!metadata) {
        fprintf(stderr, "[WARN] Atlas sprite sheet missing metadata\n");
        free_atlas_sprite_sheet_data(atlas);
        return NULL;
    }

    char* item_key = json_get_string_safe(metadata, "itemKey", "");
    strncpy(atlas->item_key, item_key, MAX_ITEM_ID_LENGTH - 1);
    free(item_key);

    atlas->atlas_width = json_get_int_safe(metadata, "atlasWidth", 0);
    atlas->atlas_height = json_get_int_safe(metadata, "atlasHeight", 0);
    atlas->cell_pixel_dim = json_get_int_safe(metadata, "cellPixelDim", 20);

    // Parse frames (DirectionFramesSchema)
    cJSON* frames = cJSON_GetObjectItem(metadata, "frames");
    if (frames) {
        parse_direction_frame_data(cJSON_GetObjectItem(frames, "up_idle"), &atlas->up_idle);
        parse_direction_frame_data(cJSON_GetObjectItem(frames, "down_idle"), &atlas->down_idle);
        parse_direction_frame_data(cJSON_GetObjectItem(frames, "right_idle"), &atlas->right_idle);
        parse_direction_frame_data(cJSON_GetObjectItem(frames, "left_idle"), &atlas->left_idle);
        parse_direction_frame_data(cJSON_GetObjectItem(frames, "up_right_idle"), &atlas->up_right_idle);
        parse_direction_frame_data(cJSON_GetObjectItem(frames, "down_right_idle"), &atlas->down_right_idle);
        parse_direction_frame_data(cJSON_GetObjectItem(frames, "up_left_idle"), &atlas->up_left_idle);
        parse_direction_frame_data(cJSON_GetObjectItem(frames, "down_left_idle"), &atlas->down_left_idle);
        parse_direction_frame_data(cJSON_GetObjectItem(frames, "default_idle"), &atlas->default_idle);
        parse_direction_frame_data(cJSON_GetObjectItem(frames, "up_walking"), &atlas->up_walking);
        parse_direction_frame_data(cJSON_GetObjectItem(frames, "down_walking"), &atlas->down_walking);
        parse_direction_frame_data(cJSON_GetObjectItem(frames, "right_walking"), &atlas->right_walking);
        parse_direction_frame_data(cJSON_GetObjectItem(frames, "left_walking"), &atlas->left_walking);
        parse_direction_frame_data(cJSON_GetObjectItem(frames, "up_right_walking"), &atlas->up_right_walking);
        parse_direction_frame_data(cJSON_GetObjectItem(frames, "down_right_walking"), &atlas->down_right_walking);
        parse_direction_frame_data(cJSON_GetObjectItem(frames, "up_left_walking"), &atlas->up_left_walking);
        parse_direction_frame_data(cJSON_GetObjectItem(frames, "down_left_walking"), &atlas->down_left_walking);
        parse_direction_frame_data(cJSON_GetObjectItem(frames, "none_idle"), &atlas->none_idle);
    }

    return atlas;
}

// --- ObjectLayer Cache Operations ---

static void cache_object_layer(ObjectLayersManager* manager, const char* item_id, ObjectLayer* layer) {
    if (!manager || !item_id) return;

    unsigned long index = hash_string(item_id) % HASH_TABLE_SIZE;

    // Check if exists
    ObjectLayerEntry* entry = manager->layer_buckets[index];
    while (entry) {
        if (strcmp(entry->key, item_id) == 0) {
            if (entry->layer) free_object_layer(entry->layer);
            entry->layer = layer;
            return;
        }
        entry = entry->next;
    }

    // Create new
    ObjectLayerEntry* new_entry = (ObjectLayerEntry*)malloc(sizeof(ObjectLayerEntry));
    if (!new_entry) return;

    new_entry->key = strdup(item_id);
    new_entry->layer = layer;
    new_entry->next = manager->layer_buckets[index];
    manager->layer_buckets[index] = new_entry;
}

static ObjectLayer* lookup_cached_layer(ObjectLayersManager* manager, const char* item_id) {
    if (!manager || !item_id) return NULL;

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
    if (!manager || !item_key) return;

    unsigned long index = hash_string(item_key) % HASH_TABLE_SIZE;

    // Check if exists
    AtlasEntry* entry = manager->atlas_buckets[index];
    while (entry) {
        if (strcmp(entry->key, item_key) == 0) {
            if (entry->atlas) free_atlas_sprite_sheet_data(entry->atlas);
            entry->atlas = atlas;
            return;
        }
        entry = entry->next;
    }

    // Create new
    AtlasEntry* new_entry = (AtlasEntry*)malloc(sizeof(AtlasEntry));
    if (!new_entry) return;

    new_entry->key = strdup(item_key);
    new_entry->atlas = atlas;
    new_entry->next = manager->atlas_buckets[index];
    manager->atlas_buckets[index] = new_entry;
}

static AtlasSpriteSheetData* lookup_cached_atlas(ObjectLayersManager* manager, const char* item_key) {
    if (!manager || !item_key) return NULL;

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

static AtlasTextureEntry* lookup_tex_entry(ObjectLayersManager* manager, const char* file_id) {
    if (!manager || !file_id) return NULL;

    unsigned long index = hash_string(file_id) % HASH_TABLE_SIZE;
    AtlasTextureEntry* entry = manager->tex_buckets[index];

    while (entry) {
        if (strcmp(entry->file_id, file_id) == 0) {
            return entry;
        }
        entry = entry->next;
    }
    return NULL;
}

static AtlasTextureEntry* create_tex_entry(ObjectLayersManager* manager, const char* file_id) {
    if (!manager || !file_id) return NULL;

    AtlasTextureEntry* entry = (AtlasTextureEntry*)malloc(sizeof(AtlasTextureEntry));
    if (!entry) return NULL;

    entry->file_id = strdup(file_id);
    entry->texture = (Texture2D){0};
    entry->state = ATLAS_TEX_NONE;
    entry->request_id = 0;
    entry->next = NULL;

    unsigned long index = hash_string(file_id) % HASH_TABLE_SIZE;
    entry->next = manager->tex_buckets[index];
    manager->tex_buckets[index] = entry;

    return entry;
}

/**
 * Start or poll the async loading of an atlas texture by file_id.
 * Returns the loaded Texture2D (id > 0 when ready), or empty (id == 0) when still loading.
 */
static Texture2D load_or_poll_atlas_texture(ObjectLayersManager* manager, const char* file_id) {
    if (!manager || !file_id || file_id[0] == '\0') return (Texture2D){0};

    AtlasTextureEntry* entry = lookup_tex_entry(manager, file_id);

    if (!entry) {
        // First request — start async fetch
        entry = create_tex_entry(manager, file_id);
        if (!entry) return (Texture2D){0};

        entry->request_id = manager->next_request_id++;
        entry->state = ATLAS_TEX_LOADING;

        // Build blob URL: {API_BASE_URL}/api/file/blob/{file_id}
        char url[512];
        snprintf(url, sizeof(url), "%s/api/file/blob/%s", API_BASE_URL, file_id);

        js_start_fetch_binary(url, entry->request_id);
        return (Texture2D){0};
    }

    if (entry->state == ATLAS_TEX_READY) {
        return entry->texture;
    }

    if (entry->state == ATLAS_TEX_ERROR) {
        return (Texture2D){0};
    }

    // ATLAS_TEX_LOADING — poll result
    if (entry->state == ATLAS_TEX_LOADING) {
        int size = 0;
        unsigned char* data = js_get_fetch_result(entry->request_id, &size);

        if (data && size > 0) {
            Image image = LoadImageFromMemory(".png", data, size);

            if (image.data != NULL) {
                entry->texture = LoadTextureFromImage(image);
                UnloadImage(image);
                entry->state = ATLAS_TEX_READY;
                fprintf(stderr, "[INFO] Atlas texture loaded for file_id: %s (%dx%d)\n",
                        file_id, entry->texture.width, entry->texture.height);
            } else {
                entry->state = ATLAS_TEX_ERROR;
                fprintf(stderr, "[WARN] Failed to decode atlas PNG for file_id: %s\n", file_id);
            }
            free(data);
        } else if (size == -1) {
            entry->state = ATLAS_TEX_ERROR;
            fprintf(stderr, "[WARN] Async fetch failed for atlas file_id: %s\n", file_id);
        }
        // else: size == 0 means still pending, keep polling
    }

    return entry->state == ATLAS_TEX_READY ? entry->texture : (Texture2D){0};
}

// --- Response Envelope Extraction ---

/**
 * Extract first item from the engine API list response envelope.
 *
 * Expected structure:
 * { "status": "success", "data": { "data": [ { ... } ], "total": N } }
 *
 * For the legacy object-layer endpoint, the structure may be:
 * { "status": "success", "data": { "data": [ { ... } ] } }
 *
 * Returns a cJSON pointer to the first element of the inner data array.
 * The caller must NOT free the returned pointer; it is owned by root.
 * The caller must free root via cJSON_Delete.
 */
static cJSON* extract_first_item_from_envelope(cJSON* root) {
    if (!root) return NULL;

    cJSON* status = cJSON_GetObjectItem(root, "status");
    if (!status || !cJSON_IsString(status) || strcmp(status->valuestring, "success") != 0) {
        return NULL;
    }

    cJSON* outer_data = cJSON_GetObjectItem(root, "data");
    if (!outer_data) return NULL;

    // Check for nested { data: [...] } (paginated list response)
    cJSON* inner_data = cJSON_GetObjectItem(outer_data, "data");
    if (inner_data && cJSON_IsArray(inner_data) && cJSON_GetArraySize(inner_data) > 0) {
        return cJSON_GetArrayItem(inner_data, 0);
    }

    // Maybe outer_data itself is the item (single-item response)
    if (cJSON_IsObject(outer_data)) {
        return outer_data;
    }

    return NULL;
}

/**
 * Extract first item from the legacy object-layer response envelope.
 *
 * The js_fetch_object_layer function returns:
 * { "items": [ { ... } ] }   (old format)
 * OR
 * { "status": "success", "data": { "data": [ { ... } ] } }   (engine format)
 */
static char* extract_first_item_from_response(const char* response_json) {
    if (!response_json) return NULL;

    cJSON* root = cJSON_Parse(response_json);
    if (!root) return NULL;

    // Try engine envelope format first
    cJSON* first = extract_first_item_from_envelope(root);
    if (first) {
        char* item_str = cJSON_Print(first);
        cJSON_Delete(root);
        return item_str;
    }

    // Fall back to legacy { "items": [...] } format
    cJSON* items = cJSON_GetObjectItem(root, "items");
    if (cJSON_IsArray(items) && cJSON_GetArraySize(items) > 0) {
        cJSON* first_item = cJSON_GetArrayItem(items, 0);
        char* item_str = cJSON_Print(first_item);
        cJSON_Delete(root);
        return item_str;
    }

    cJSON_Delete(root);
    return NULL;
}

// ============================================================================
// Public API
// ============================================================================

ObjectLayersManager* create_object_layers_manager(TextureManager* texture_manager) {
    ObjectLayersManager* manager = malloc(sizeof(ObjectLayersManager));
    if (!manager) return NULL;

    for (int i = 0; i < HASH_TABLE_SIZE; i++) {
        manager->layer_buckets[i] = NULL;
        manager->atlas_buckets[i] = NULL;
        manager->tex_buckets[i] = NULL;
    }

    manager->texture_manager = texture_manager;
    manager->authenticated = false;
    manager->next_request_id = 1;

    // Initialize the engine API connection and authenticate
    js_init_engine_api(API_BASE_URL, AUTH_EMAIL, AUTH_PASSWORD);
    manager->authenticated = true;

    return manager;
}

void destroy_object_layers_manager(ObjectLayersManager* manager) {
    if (!manager) return;

    // Free object layer cache
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

    // Free atlas data cache
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

    // Free atlas texture entries (unload GPU textures)
    for (int i = 0; i < HASH_TABLE_SIZE; i++) {
        AtlasTextureEntry* entry = manager->tex_buckets[i];
        while (entry) {
            AtlasTextureEntry* next = entry->next;
            if (entry->texture.id > 0) {
                UnloadTexture(entry->texture);
            }
            free(entry->file_id);
            free(entry);
            entry = next;
        }
        manager->tex_buckets[i] = NULL;
    }

    free(manager);
}

bool object_layers_authenticate(ObjectLayersManager* manager) {
    if (!manager) return false;

    // Re-trigger authentication via JS
    js_init_engine_api(API_BASE_URL, AUTH_EMAIL, AUTH_PASSWORD);
    manager->authenticated = true;
    return true;
}

ObjectLayer* get_or_fetch_object_layer(ObjectLayersManager* manager, const char* item_id) {
    if (!manager || !item_id) return NULL;

    // 1. Check cache
    ObjectLayer* cached = lookup_cached_layer(manager, item_id);
    if (cached) return cached;

    // 2. Fetch from API via JS
    char* response_json = js_fetch_object_layer(item_id);
    if (response_json) {
        char* item_json = extract_first_item_from_response(response_json);
        free(response_json);
        if (item_json) {
            ObjectLayer* layer = parse_object_layer_json(item_json);
            free(item_json);
            if (layer) {
                cache_object_layer(manager, item_id, layer);
                return layer;
            }
        }
    }

    // 3. Create placeholder entry to avoid repeated failed fetches
    ObjectLayerEntry* new_entry = (ObjectLayerEntry*)malloc(sizeof(ObjectLayerEntry));
    if (new_entry) {
        new_entry->key = strdup(item_id);
        new_entry->layer = NULL;
        new_entry->next = manager->layer_buckets[hash_string(item_id) % HASH_TABLE_SIZE];
        manager->layer_buckets[hash_string(item_id) % HASH_TABLE_SIZE] = new_entry;
    }

    return NULL;
}

AtlasSpriteSheetData* get_or_fetch_atlas_data(ObjectLayersManager* manager, const char* item_key) {
    if (!manager || !item_key) return NULL;

    // 1. Check cache
    AtlasSpriteSheetData* cached = lookup_cached_atlas(manager, item_key);
    if (cached) return cached;

    // 2. Fetch from atlas-sprite-sheet API via JS
    char* response_json = js_fetch_atlas_sprite_sheet(item_key);
    if (!response_json) {
        fprintf(stderr, "[WARN] Failed to fetch atlas sprite sheet for: %s\n", item_key);
        return NULL;
    }

    // 3. Parse the envelope response
    cJSON* root = cJSON_Parse(response_json);
    free(response_json);

    if (!root) {
        fprintf(stderr, "[ERROR] Failed to parse atlas sprite sheet JSON for: %s\n", item_key);
        return NULL;
    }

    cJSON* first_item = extract_first_item_from_envelope(root);
    if (!first_item) {
        fprintf(stderr, "[WARN] No atlas sprite sheet found for item: %s\n", item_key);
        cJSON_Delete(root);
        return NULL;
    }

    // 4. Parse the atlas sprite sheet data
    AtlasSpriteSheetData* atlas = parse_atlas_sprite_sheet_json(first_item);
    cJSON_Delete(root);

    if (!atlas) {
        fprintf(stderr, "[ERROR] Failed to parse atlas data for: %s\n", item_key);
        return NULL;
    }

    // 5. Ensure item_key is set
    if (atlas->item_key[0] == '\0') {
        strncpy(atlas->item_key, item_key, MAX_ITEM_ID_LENGTH - 1);
    }

    // 6. Cache the atlas data
    cache_atlas_data(manager, item_key, atlas);

    // 7. Start async loading of the atlas texture (if file_id is available)
    if (atlas->file_id[0] != '\0') {
        load_or_poll_atlas_texture(manager, atlas->file_id);
    }

    fprintf(stderr, "[INFO] Atlas sprite sheet cached for: %s (file_id: %s, %dx%d)\n",
            item_key, atlas->file_id, atlas->atlas_width, atlas->atlas_height);

    return atlas;
}

Texture2D get_atlas_texture(ObjectLayersManager* manager, const char* file_id) {
    if (!manager || !file_id || file_id[0] == '\0') return (Texture2D){0};

    return load_or_poll_atlas_texture(manager, file_id);
}