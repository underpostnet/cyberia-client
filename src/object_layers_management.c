#include "object_layers_management.h"
#include "config.h"
#include "cJSON.h"
#include "direction_converter.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(PLATFORM_WEB)
    #include <emscripten.h>
    // External JS function
    extern char* js_fetch_object_layer(const char* item_id);
#else
    #include <curl/curl.h>
    #include <pthread.h>
    #include <unistd.h>
#endif

#define HASH_TABLE_SIZE 256
#define MAX_LAYER_ENTRIES 1000
#define TEXTURE_QUEUE_SIZE 1024

// --- Data Structures ---

#if !defined(PLATFORM_WEB)
typedef struct {
    char* url;
    char* data;
    size_t size;
    size_t capacity;
    volatile int status; // 0: pending, 1: complete, 2: error
    pthread_t thread;
} AsyncLayerRequest;
#endif

typedef struct ObjectLayerEntry {
    char* key;
    ObjectLayer* layer;
    struct ObjectLayerEntry* next;
    #if !defined(PLATFORM_WEB)
    AsyncLayerRequest* async_req;
    #endif
} ObjectLayerEntry;

typedef struct QueueNode {
    ObjectLayer* layer;
    struct QueueNode* next;
} QueueNode;

struct ObjectLayersManager {
    ObjectLayerEntry* buckets[HASH_TABLE_SIZE];
    TextureManager* texture_manager;

    // Texture pre-caching queue
    QueueNode* queue_head;
    QueueNode* queue_tail;
    int queue_size;
};

// --- Helper Functions ---

#if !defined(PLATFORM_WEB)
// Memory struct for curl response
static size_t write_memory_callback(void* contents, size_t size, size_t nmemb, void* userp) {
    size_t realsize = size * nmemb;
    AsyncLayerRequest* req = (AsyncLayerRequest*)userp;

    if (req->size + realsize + 1 > req->capacity) {
        size_t new_capacity = req->capacity == 0 ? (realsize + 4096) : (req->capacity * 2 + realsize);
        char* ptr = realloc(req->data, new_capacity);
        if (!ptr) {
            fprintf(stderr, "[ERROR] Not enough memory (realloc returned NULL)\n");
            return 0;
        }
        req->data = ptr;
        req->capacity = new_capacity;
    }

    memcpy(&(req->data[req->size]), contents, realsize);
    req->size += realsize;
    req->data[req->size] = 0; // Null terminate

    return realsize;
}

static void* fetch_layer_worker(void* arg) {
    AsyncLayerRequest* req = (AsyncLayerRequest*)arg;

    CURL* curl_handle = curl_easy_init();
    if (!curl_handle) {
        req->status = 2;
        return NULL;
    }

    curl_easy_setopt(curl_handle, CURLOPT_URL, req->url);
    curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, write_memory_callback);
    curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, (void*)req);
    curl_easy_setopt(curl_handle, CURLOPT_USERAGENT, "libcurl-agent/1.0");
    curl_easy_setopt(curl_handle, CURLOPT_TIMEOUT, 10L);
    curl_easy_setopt(curl_handle, CURLOPT_FOLLOWLOCATION, 1L);

    CURLcode res = curl_easy_perform(curl_handle);

    if (res != CURLE_OK) {
        fprintf(stderr, "[WARN] curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
        req->status = 2;
    } else {
        long response_code;
        curl_easy_getinfo(curl_handle, CURLINFO_RESPONSE_CODE, &response_code);
        if (response_code == 200) {
            req->status = 1;
        } else {
            fprintf(stderr, "[WARN] HTTP error %ld for URL: %s\n", response_code, req->url);
            req->status = 2;
        }
    }

    curl_easy_cleanup(curl_handle);
    return NULL;
}
#endif

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

// --- Parsing Logic ---

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

// --- Caching & Queueing ---

static void cache_object_layer(ObjectLayersManager* manager, const char* item_id, ObjectLayer* layer) {
    if (!manager || !item_id) return;

    unsigned long index = hash_string(item_id) % HASH_TABLE_SIZE;

    // Check if exists
    ObjectLayerEntry* entry = manager->buckets[index];
    while (entry) {
        if (strcmp(entry->key, item_id) == 0) {
            // Update existing
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
    new_entry->next = manager->buckets[index];
    #if !defined(PLATFORM_WEB)
    new_entry->async_req = NULL;
    #endif

    manager->buckets[index] = new_entry;
}

static void enqueue_for_texture_caching(ObjectLayersManager* manager, ObjectLayer* layer) {
    if (!manager || !layer || manager->queue_size >= TEXTURE_QUEUE_SIZE) return;

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

    manager->queue_size++;
}

// --- Fetching Logic ---

static char* extract_first_item_from_response(const char* response_json) {
    if (!response_json) return NULL;

    cJSON* root = cJSON_Parse(response_json);
    if (!root) return NULL;

    cJSON* items = cJSON_GetObjectItem(root, "items");
    if (!cJSON_IsArray(items) || cJSON_GetArraySize(items) == 0) {
        cJSON_Delete(root);
        return NULL;
    }

    cJSON* first_item = cJSON_GetArrayItem(items, 0);
    char* item_str = cJSON_Print(first_item);

    cJSON_Delete(root);
    return item_str;
}

// --- Public API ---

ObjectLayersManager* create_object_layers_manager(TextureManager* texture_manager) {
    ObjectLayersManager* manager = (ObjectLayersManager*)malloc(sizeof(ObjectLayersManager));
    if (!manager) return NULL;

    for (int i = 0; i < HASH_TABLE_SIZE; i++) {
        manager->buckets[i] = NULL;
    }

    manager->texture_manager = texture_manager;
    manager->queue_head = NULL;
    manager->queue_tail = NULL;
    manager->queue_size = 0;

    #if !defined(PLATFORM_WEB)
    curl_global_init(CURL_GLOBAL_ALL);
    #endif

    return manager;
}

void destroy_object_layers_manager(ObjectLayersManager* manager) {
    if (!manager) return;

    // Free cache
    for (int i = 0; i < HASH_TABLE_SIZE; i++) {
        ObjectLayerEntry* entry = manager->buckets[i];
        while (entry) {
            ObjectLayerEntry* next = entry->next;
            if (entry->layer) free_object_layer(entry->layer);

            #if !defined(PLATFORM_WEB)
            if (entry->async_req) {
                pthread_join(entry->async_req->thread, NULL);
                if (entry->async_req->data) free(entry->async_req->data);
                if (entry->async_req->url) free(entry->async_req->url);
                free(entry->async_req);
            }
            #endif

            free(entry->key);
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

    free(manager);

    #if !defined(PLATFORM_WEB)
    curl_global_cleanup();
    #endif
}

ObjectLayer* get_or_fetch_object_layer(ObjectLayersManager* manager, const char* item_id) {
    if (!manager || !item_id) return NULL;

    unsigned long index = hash_string(item_id) % HASH_TABLE_SIZE;
    ObjectLayerEntry* entry = manager->buckets[index];

    // 1. Check cache
    while (entry) {
        if (strcmp(entry->key, item_id) == 0) {
            // Found entry

            // If we have a layer, return it
            if (entry->layer) return entry->layer;

            // If no layer, check async status (Native)
            #if !defined(PLATFORM_WEB)
            if (entry->async_req) {
                if (entry->async_req->status == 1) { // Complete
                    // Parse data
                    char* item_json = extract_first_item_from_response(entry->async_req->data);
                    if (item_json) {
                        ObjectLayer* layer = parse_object_layer_json(item_json);
                        free(item_json);

                        if (layer) {
                            entry->layer = layer;
                            enqueue_for_texture_caching(manager, layer);
                        }
                    }

                    // Cleanup request
                    pthread_join(entry->async_req->thread, NULL);
                    if (entry->async_req->data) free(entry->async_req->data);
                    if (entry->async_req->url) free(entry->async_req->url);
                    free(entry->async_req);
                    entry->async_req = NULL;

                    return entry->layer;

                } else if (entry->async_req->status == 2) { // Error
                    // Cleanup and leave layer as NULL (failed)
                    pthread_join(entry->async_req->thread, NULL);
                    if (entry->async_req->data) free(entry->async_req->data);
                    if (entry->async_req->url) free(entry->async_req->url);
                    free(entry->async_req);
                    entry->async_req = NULL;
                    return NULL;
                } else {
                    // Still pending
                    return NULL;
                }
            }
            #endif

            // If we are here, it means we have an entry but no layer and no active request.
            // This could mean a previously failed request.
            // For now, we return NULL. To retry, we'd need logic to clear the entry.
            return NULL;
        }
        entry = entry->next;
    }

    // 2. Not in cache - Start fetch

    // Create entry placeholder
    ObjectLayerEntry* new_entry = (ObjectLayerEntry*)malloc(sizeof(ObjectLayerEntry));
    if (!new_entry) return NULL;

    new_entry->key = strdup(item_id);
    new_entry->layer = NULL;
    new_entry->next = manager->buckets[index];
    #if !defined(PLATFORM_WEB)
    new_entry->async_req = NULL;
    #endif
    manager->buckets[index] = new_entry;

    #if defined(PLATFORM_WEB)
    // Web Sync Fetch (Legacy/Simple) - Blocking for now as JS async is complex to bridge here without callback hell
    // Ideally this should also be async, but for now we keep the sync behavior or use the JS fetcher if available
    char* response_json = js_fetch_object_layer(item_id);
    if (response_json) {
        char* item_json = extract_first_item_from_response(response_json);
        free(response_json);
        if (item_json) {
            ObjectLayer* layer = parse_object_layer_json(item_json);
            free(item_json);
            if (layer) {
                new_entry->layer = layer;
                enqueue_for_texture_caching(manager, layer);
                return layer;
            }
        }
    }
    return NULL;

    #else
    // Native Async Fetch
    char url[512];
    snprintf(url, sizeof(url), "%s/object-layers?item_id=%s&page=1&page_size=1", API_BASE_URL, item_id);

    AsyncLayerRequest* req = (AsyncLayerRequest*)malloc(sizeof(AsyncLayerRequest));
    if (req) {
        req->url = strdup(url);
        req->data = NULL;
        req->size = 0;
        req->capacity = 0;
        req->status = 0; // Pending

        if (pthread_create(&req->thread, NULL, fetch_layer_worker, req) == 0) {
            new_entry->async_req = req;
        } else {
            free(req->url);
            free(req);
        }
    }
    return NULL; // Return NULL immediately while loading
    #endif
}

void process_texture_caching_queue(ObjectLayersManager* manager) {
    if (!manager || !manager->queue_head) return;

    // Process up to 5 layers per frame to avoid stalling
    int processed = 0;
    while (manager->queue_head && processed < 5) {
        QueueNode* node = manager->queue_head;
        ObjectLayer* layer = node->layer;

        // Pre-cache textures for this layer
        // We iterate through common directions
        const char* directions[] = {"08", "02", "04", "06", "18", "12", "14", "16"};
        int dir_count = 8;

        for (int d = 0; d < dir_count; d++) {
            // Just try to load frame 0 for each direction to get things started
            // The render loop will request specific frames as needed
            char uri[512];
            build_object_layer_uri(uri, sizeof(uri), layer->data.item.type, layer->data.item.id, directions[d], 0);
            load_texture_from_url(manager->texture_manager, uri);
        }

        // Remove from queue
        manager->queue_head = node->next;
        if (!manager->queue_head) manager->queue_tail = NULL;

        free(node);
        manager->queue_size--;
        processed++;
    }
}

void build_object_layer_uri(
    char* buffer,
    size_t buffer_size,
    const char* item_type,
    const char* item_id,
    const char* direction_code,
    int frame
) {
    if (!buffer || buffer_size == 0) return;

    snprintf(buffer, buffer_size, "%s/%s/%s/%s/%d.png",
             ASSETS_BASE_URL, item_type, item_id, direction_code, frame);
}
