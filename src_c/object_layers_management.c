#include "object_layers_management.h"
#include "config.h"
#include "direction_converter.h"
#include "game_state.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#if defined(PLATFORM_WEB)
    #include <emscripten.h>
    
    // External JavaScript functions for HTTP requests
    // Implemented in js/fetch_helper.js
    extern char* js_fetch_object_layer(const char* item_id);
#else
    #include <curl/curl.h>
#endif

#include "../lib/cJSON/cJSON.h"

#define HASH_TABLE_SIZE 256
#define MAX_LAYER_ENTRIES 256

// ============================================================================
// Data Structures
// ============================================================================

/**
 * @brief Cache entry for object layers
 */
typedef struct ObjectLayerEntry {
    char* key;                          ///< Item ID (cache key)
    ObjectLayer* layer;
    struct ObjectLayerEntry* next;      ///< For hash collision chaining
} ObjectLayerEntry;

/**
 * @brief Queue node for texture pre-caching
 */
typedef struct QueueNode {
    ObjectLayer* layer;
    struct QueueNode* next;
} QueueNode;

/**
 * @brief Main ObjectLayersManager structure
 */
struct ObjectLayersManager {
    ObjectLayerEntry* buckets[HASH_TABLE_SIZE];    ///< Hash table for layers
    TextureManager* texture_manager;               ///< Reference to texture manager
    QueueNode* queue_head;                         ///< Head of texture caching queue
    QueueNode* queue_tail;                         ///< Tail of texture caching queue
    int layer_count;                               ///< Current number of cached layers
};

#if !defined(PLATFORM_WEB)
/**
 * @brief Memory buffer for curl response (native platforms only)
 */
typedef struct {
    char* memory;
    size_t size;
} MemoryStruct;
#endif

// ============================================================================
// Helper Functions - HTTP and Memory
// ============================================================================

#if !defined(PLATFORM_WEB)
/**
 * @brief Callback for curl to write response data to memory buffer (native platforms only)
 */
static size_t write_memory_callback(void* contents, size_t size, size_t nmemb, void* userp) {
    size_t realsize = size * nmemb;
    MemoryStruct* mem = (MemoryStruct*)userp;

    char* ptr = realloc(mem->memory, mem->size + realsize + 1);
    if (!ptr) {
        fprintf(stderr, "[ERROR] Not enough memory for HTTP response\n");
        return 0;
    }

    mem->memory = ptr;
    memcpy(&(mem->memory[mem->size]), contents, realsize);
    mem->size += realsize;
    mem->memory[mem->size] = 0;

    return realsize;
}
#endif

/**
 * @brief DJB2 hash function for string keys
 */
static unsigned long hash_string(const char* str) {
    unsigned long hash = 5381;
    int c;
    while ((c = *str++)) {
        hash = ((hash << 5) + hash) + c;
    }
    return hash;
}

// ============================================================================
// Helper Functions - JSON Parsing with cJSON
// ============================================================================

/**
 * @brief Safely get a string from JSON object
 */
static char* json_get_string_safe(cJSON* obj, const char* key) {
    if (!obj || !key) return NULL;
    cJSON* item = cJSON_GetObjectItemCaseSensitive(obj, key);
    if (!item || !cJSON_IsString(item)) return NULL;
    
    if (!item->valuestring) return NULL;
    return strdup(item->valuestring);
}

/**
 * @brief Safely get an integer from JSON object
 */
static int json_get_int_safe(cJSON* obj, const char* key, int default_val) {
    if (!obj || !key) return default_val;
    cJSON* item = cJSON_GetObjectItemCaseSensitive(obj, key);
    if (!item || !cJSON_IsNumber(item)) return default_val;
    return item->valueint;
}

/**
 * @brief Safely get a boolean from JSON object
 */
static bool json_get_bool_safe(cJSON* obj, const char* key, bool default_val) {
    if (!obj || !key) return default_val;
    cJSON* item = cJSON_GetObjectItemCaseSensitive(obj, key);
    if (!item) return default_val;
    if (cJSON_IsBool(item)) return item->type == cJSON_True;
    if (cJSON_IsNumber(item)) return item->valueint != 0;
    return default_val;
}

/**
 * @brief Count elements in a JSON array
 */
static int json_array_count(cJSON* obj, const char* key) {
    if (!obj || !key) return 0;
    cJSON* arr = cJSON_GetObjectItemCaseSensitive(obj, key);
    if (!arr || !cJSON_IsArray(arr)) return 0;
    return cJSON_GetArraySize(arr);
}

/**
 * @brief Parse Stats from JSON
 */
static void parse_stats(cJSON* stats_obj, Stats* stats) {
    if (!stats_obj || !stats) return;
    
    stats->effect = json_get_int_safe(stats_obj, "effect", 0);
    stats->resistance = json_get_int_safe(stats_obj, "resistance", 0);
    stats->agility = json_get_int_safe(stats_obj, "agility", 0);
    stats->range = json_get_int_safe(stats_obj, "range", 0);
    stats->intelligence = json_get_int_safe(stats_obj, "intelligence", 0);
    stats->utility = json_get_int_safe(stats_obj, "utility", 0);
}

/**
 * @brief Parse RenderFrames from JSON
 */
static void parse_render_frames(cJSON* frames_obj, RenderFrames* frames) {
    if (!frames_obj || !frames) return;
    
    memset(frames, 0, sizeof(RenderFrames));
    
    // Idle animations
    frames->up_idle_count = json_array_count(frames_obj, "up_idle");
    frames->down_idle_count = json_array_count(frames_obj, "down_idle");
    frames->left_idle_count = json_array_count(frames_obj, "left_idle");
    frames->right_idle_count = json_array_count(frames_obj, "right_idle");
    
    frames->up_left_idle_count = json_array_count(frames_obj, "up_left_idle");
    frames->up_right_idle_count = json_array_count(frames_obj, "up_right_idle");
    frames->down_left_idle_count = json_array_count(frames_obj, "down_left_idle");
    frames->down_right_idle_count = json_array_count(frames_obj, "down_right_idle");
    
    frames->default_idle_count = json_array_count(frames_obj, "default_idle");
    frames->none_idle_count = json_array_count(frames_obj, "none_idle");
    
    // Walking animations
    frames->up_walking_count = json_array_count(frames_obj, "up_walking");
    frames->down_walking_count = json_array_count(frames_obj, "down_walking");
    frames->left_walking_count = json_array_count(frames_obj, "left_walking");
    frames->right_walking_count = json_array_count(frames_obj, "right_walking");
    
    frames->up_left_walking_count = json_array_count(frames_obj, "up_left_walking");
    frames->up_right_walking_count = json_array_count(frames_obj, "up_right_walking");
    frames->down_left_walking_count = json_array_count(frames_obj, "down_left_walking");
    frames->down_right_walking_count = json_array_count(frames_obj, "down_right_walking");
}

/**
 * @brief Parse Render metadata from JSON
 */
static void parse_render(cJSON* render_obj, Render* render) {
    if (!render_obj || !render) return;
    
    memset(render, 0, sizeof(Render));
    
    // Parse frames
    cJSON* frames_obj = cJSON_GetObjectItemCaseSensitive(render_obj, "frames");
    if (frames_obj && cJSON_IsObject(frames_obj)) {
        parse_render_frames(frames_obj, &render->frames);
    }
    
    // Parse frame duration
    render->frame_duration = json_get_int_safe(render_obj, "frame_duration", 100);
    
    // Parse stateless flag
    render->is_stateless = json_get_bool_safe(render_obj, "is_stateless", false);
}

/**
 * @brief Parse Item metadata from JSON
 */
static void parse_item(cJSON* item_obj, Item* item) {
    if (!item_obj || !item) return;
    
    memset(item, 0, sizeof(Item));
    
    // Get strings safely and copy to fixed-size arrays
    char* id_str = json_get_string_safe(item_obj, "id");
    if (id_str) {
        strncpy(item->id, id_str, MAX_ITEM_ID_LENGTH - 1);
        item->id[MAX_ITEM_ID_LENGTH - 1] = '\0';
        free(id_str);
    }
    
    char* type_str = json_get_string_safe(item_obj, "type");
    if (type_str) {
        strncpy(item->type, type_str, MAX_TYPE_LENGTH - 1);
        item->type[MAX_TYPE_LENGTH - 1] = '\0';
        free(type_str);
    }
    
    char* desc_str = json_get_string_safe(item_obj, "description");
    if (desc_str) {
        strncpy(item->description, desc_str, MAX_DESCRIPTION_LENGTH - 1);
        item->description[MAX_DESCRIPTION_LENGTH - 1] = '\0';
        free(desc_str);
    }
    
    item->activable = json_get_bool_safe(item_obj, "activable", false);
}

/**
 * @brief Parse ObjectLayerData from JSON
 */
static void parse_object_layer_data(cJSON* data_obj, ObjectLayerData* data) {
    if (!data_obj || !data) return;
    
    // Parse stats
    cJSON* stats_obj = cJSON_GetObjectItemCaseSensitive(data_obj, "stats");
    if (stats_obj && cJSON_IsObject(stats_obj)) {
        parse_stats(stats_obj, &data->stats);
    }
    
    // Parse render
    cJSON* render_obj = cJSON_GetObjectItemCaseSensitive(data_obj, "render");
    if (render_obj && cJSON_IsObject(render_obj)) {
        parse_render(render_obj, &data->render);
    }
    
    // Parse item
    cJSON* item_obj = cJSON_GetObjectItemCaseSensitive(data_obj, "item");
    if (item_obj && cJSON_IsObject(item_obj)) {
        parse_item(item_obj, &data->item);
    }
}

/**
 * @brief Parse complete ObjectLayer from JSON response
 */
static ObjectLayer* parse_object_layer_json(const char* json_string) {
    if (!json_string) return NULL;
    
    cJSON* root = cJSON_Parse(json_string);
    if (!root) {
        fprintf(stderr, "[ERROR] Failed to parse JSON response\n");
        return NULL;
    }
    
    ObjectLayer* layer = create_object_layer();
    if (!layer) {
        cJSON_Delete(root);
        return NULL;
    }
    
    // Parse data object
    cJSON* data_obj = cJSON_GetObjectItemCaseSensitive(root, "data");
    if (data_obj && cJSON_IsObject(data_obj)) {
        parse_object_layer_data(data_obj, &layer->data);
    }
    
    // Parse sha256
    char* sha256_str = json_get_string_safe(root, "sha256");
    if (sha256_str) {
        strncpy(layer->sha256, sha256_str, 64);
        layer->sha256[64] = '\0';
        free(sha256_str);
    }
    
    cJSON_Delete(root);
    return layer;
}

// ============================================================================
// Helper Functions - Cache Management
// ============================================================================

/**
 * @brief Add an ObjectLayer to the cache
 */
static void cache_object_layer(
    ObjectLayersManager* manager,
    const char* item_id,
    ObjectLayer* layer
) {
    if (!manager || !item_id) return;
    
    unsigned long index = hash_string(item_id) % HASH_TABLE_SIZE;
    
    ObjectLayerEntry* new_entry = (ObjectLayerEntry*)malloc(sizeof(ObjectLayerEntry));
    if (!new_entry) return;
    
    new_entry->key = strdup(item_id);
    new_entry->layer = layer;  // Can be NULL to mark failed fetch
    new_entry->next = manager->buckets[index];
    manager->buckets[index] = new_entry;
    manager->layer_count++;
}

/**
 * @brief Retrieve an ObjectLayer from the cache
 */
/**
 * @brief Add an ObjectLayer to the texture pre-caching queue
 */
static void enqueue_for_texture_caching(
    ObjectLayersManager* manager,
    ObjectLayer* layer
) {
    if (!manager || !layer) return;
    
    QueueNode* node = (QueueNode*)malloc(sizeof(QueueNode));
    if (!node) return;
    
    node->layer = layer;
    node->next = NULL;
    
    if (manager->queue_tail) {
        manager->queue_tail->next = node;
        manager->queue_tail = node;
    } else {
        manager->queue_head = node;
        manager->queue_tail = node;
    }
    
    printf("[QUEUE] Queued textures for pre-caching: %s\n", layer->data.item.id);
}

// ============================================================================
// API Fetching
// ============================================================================

#if defined(PLATFORM_WEB)
/**
 * @brief Fetch ObjectLayer data from REST API (JavaScript version for web platform)
 * 
 * Uses JavaScript fetch API via Emscripten library function for better
 * CORS handling and reliability in browsers.
 * 
 * Correct API endpoint based on Python implementation:
 * GET {API_BASE_URL}/object-layers?item_id={item_id}&page=1&page_size=1
 * 
 * Returns: { "items": [ { ObjectLayer } ] }
 */
static char* fetch_object_layer_from_api(const char* item_id) {
    if (!item_id) return NULL;
    
    // Call JavaScript function to perform the fetch
    // This uses Asyncify to handle the async operation
    char* response = js_fetch_object_layer(item_id);
    
    if (!response) {
        fprintf(stderr, "[ERROR] Failed to fetch object layer for item: %s (check CORS or use localhost API)\n", item_id);
        return NULL;
    }
    
    return response;
}
#else
/**
 * @brief Fetch ObjectLayer data from REST API (cURL version for native platforms)
 * 
 * Correct API endpoint based on Python implementation:
 * GET {API_BASE_URL}/object-layers?item_id={item_id}&page=1&page_size=1
 * 
 * Returns: { "items": [ { ObjectLayer } ] }
 */
static char* fetch_object_layer_from_api(const char* item_id) {
    if (!item_id) return NULL;
    
    CURL* curl = curl_easy_init();
    if (!curl) {
        fprintf(stderr, "[ERROR] Failed to initialize curl\n");
        return NULL;
    }
    
    // Build URL with query parameters
    char url[512];
    snprintf(url, sizeof(url),
             "%s/object-layers?item_id=%s&page=1&page_size=1",
             API_BASE_URL, item_id);
    
    MemoryStruct chunk;
    chunk.memory = malloc(1);
    chunk.size = 0;
    
    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_memory_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void*)&chunk);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, HTTP_TIMEOUT_SECONDS);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    
    // Add SSL verification for HTTPS
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 2L);
    
    CURLcode res = curl_easy_perform(curl);
    
    if (res != CURLE_OK) {
        fprintf(stderr, "[ERROR] Curl request failed: %s\n", curl_easy_strerror(res));
        curl_easy_cleanup(curl);
        free(chunk.memory);
        return NULL;
    }
    
    // Check HTTP response code
    long http_code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
    
    if (http_code != 200) {
        fprintf(stderr, "[ERROR] HTTP error %ld for item %s\n", http_code, item_id);
        curl_easy_cleanup(curl);
        free(chunk.memory);
        return NULL;
    }
    
    curl_easy_cleanup(curl);
    
    return chunk.memory;
}
#endif

/**
 * @brief Extract the first item from API response JSON
 */
static char* extract_first_item_from_response(const char* response_json) {
    if (!response_json) return NULL;
    
    cJSON* root = cJSON_Parse(response_json);
    if (!root) {
        fprintf(stderr, "[ERROR] Failed to parse API response JSON\n");
        return NULL;
    }
    
    // Get "items" array
    cJSON* items = cJSON_GetObjectItemCaseSensitive(root, "items");
    if (!items || !cJSON_IsArray(items)) {
        fprintf(stderr, "[ERROR] 'items' not found or not an array in response\n");
        cJSON_Delete(root);
        return NULL;
    }
    
    int item_count = cJSON_GetArraySize(items);
    if (item_count == 0) {
        fprintf(stderr, "[ERROR] Empty items array in response\n");
        cJSON_Delete(root);
        return NULL;
    }
    
    // Get first item
    cJSON* first_item = cJSON_GetArrayItem(items, 0);
    if (!first_item) {
        fprintf(stderr, "[ERROR] Failed to get first item from array\n");
        cJSON_Delete(root);
        return NULL;
    }
    
    // Convert to string and return
    char* item_string = cJSON_Print(first_item);
    
    cJSON_Delete(root);
    
    return item_string;
}

// ============================================================================
// Public API - Lifecycle Management
// ============================================================================

ObjectLayersManager* create_object_layers_manager(TextureManager* texture_manager) {
    ObjectLayersManager* manager = (ObjectLayersManager*)malloc(sizeof(ObjectLayersManager));
    if (!manager) return NULL;
    
    memset(manager, 0, sizeof(ObjectLayersManager));
    manager->texture_manager = texture_manager;
    manager->layer_count = 0;
    
    printf("[MANAGER] Created ObjectLayersManager\n");
    
    return manager;
}

void destroy_object_layers_manager(ObjectLayersManager* manager) {
    if (!manager) return;
    
    // Free all cache entries
    for (int i = 0; i < HASH_TABLE_SIZE; i++) {
        ObjectLayerEntry* entry = manager->buckets[i];
        while (entry) {
            ObjectLayerEntry* next = entry->next;
            if (entry->key) free(entry->key);
            if (entry->layer) free_object_layer(entry->layer);
            free(entry);
            entry = next;
        }
        manager->buckets[i] = NULL;
    }
    
    // Free queue
    QueueNode* node = manager->queue_head;
    while (node) {
        QueueNode* next = node->next;
        free(node);
        node = next;
    }
    manager->queue_head = NULL;
    manager->queue_tail = NULL;
    
    free(manager);
    printf("[MANAGER] Destroyed ObjectLayersManager\n");
}

// ============================================================================
// Public API - Object Layer Fetching
// ============================================================================

ObjectLayer* get_or_fetch_object_layer(ObjectLayersManager* manager, const char* item_id) {
    if (!manager || !item_id) {
        return NULL;
    }
    
    // 1. Check cache (including failed fetches)
    unsigned long index = hash_string(item_id) % HASH_TABLE_SIZE;
    ObjectLayerEntry* entry = manager->buckets[index];
    while (entry) {
        if (strcmp(entry->key, item_id) == 0) {
            // Found in cache - could be valid layer or NULL (failed fetch)
            return entry->layer;
        }
        entry = entry->next;
    }
    
    // 2. Fetch from API
    char* response_json = fetch_object_layer_from_api(item_id);
    if (!response_json) {
        // Cache the NULL result to avoid repeated failed fetches
        cache_object_layer(manager, item_id, NULL);
        return NULL;
    }
    
    // 3. Extract first item from response array
    char* item_json = extract_first_item_from_response(response_json);
    free(response_json);
    
    if (!item_json) {
        cache_object_layer(manager, item_id, NULL);
        return NULL;
    }
    
    // 4. Parse JSON to ObjectLayer
    ObjectLayer* layer = parse_object_layer_json(item_json);
    free(item_json);
    
    if (!layer) {
        cache_object_layer(manager, item_id, NULL);
        return NULL;
    }
    
    // 5. Cache and queue for texture pre-caching
    cache_object_layer(manager, item_id, layer);
    enqueue_for_texture_caching(manager, layer);
    
    return layer;
}

// ============================================================================
// Public API - Texture Pre-caching
// ============================================================================

/**
 * @brief Process texture caching queue (now a no-op for performance)
 * 
 * Pre-caching is disabled because it causes significant performance drops.
 * Textures are now loaded on-demand when entities need them.
 * The texture cache already prevents duplicate loads.
 */
void process_texture_caching_queue(ObjectLayersManager* manager) {
    // Clear queue without processing to free memory
    if (!manager) return;
    
    while (manager->queue_head) {
        QueueNode* node = manager->queue_head;
        manager->queue_head = node->next;
        free(node);
    }
    manager->queue_tail = NULL;
}

// ============================================================================
// Public API - URI Building
// ============================================================================

void build_object_layer_uri(
    char* buffer,
    size_t buffer_size,
    const char* item_type,
    const char* item_id,
    const char* direction_code,
    int frame
) {
    if (!buffer || buffer_size == 0 || !item_type || !item_id || !direction_code) {
        return;
    }
    
    // Format: ASSETS_BASE_URL/item_type/item_id/direction_code/frame.png
    // Example: https://server.cyberiaonline.com/assets/skin/anon/08/0.png
    snprintf(buffer, buffer_size,
             "%s/%s/%s/%s/%d.png",
             ASSETS_BASE_URL, item_type, item_id, direction_code, frame);
}