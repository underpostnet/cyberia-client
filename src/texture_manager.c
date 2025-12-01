#include "texture_manager.h"
#include "config.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#if defined(PLATFORM_WEB)
    #include <emscripten.h>
    
    // External JavaScript functions for HTTP requests
    extern unsigned char* js_fetch_binary(const char* url, size_t* size);
    extern void js_start_fetch_binary(const char* url, int request_id);
    extern unsigned char* js_get_fetch_result(int request_id, int* size);
#else
    #include <curl/curl.h>
    #include <pthread.h>
    #include <unistd.h>
#endif

#define HASH_TABLE_SIZE 256

// --- Data Structures ---

typedef enum {
    TEXTURE_STATE_NONE,
    TEXTURE_STATE_LOADING,
    TEXTURE_STATE_READY,
    TEXTURE_STATE_ERROR
} TextureState;

#if !defined(PLATFORM_WEB)
// Structure to handle async download state on native platforms
typedef struct {
    char* url;
    unsigned char* data;
    size_t size;
    size_t capacity;
    volatile int status; // 0: pending, 1: complete, 2: error
    pthread_t thread;
} AsyncRequest;
#endif

typedef struct TextureEntry {
    char* key;
    Texture2D texture;
    TextureState state;
    int request_id; // For Web async tracking
    #if !defined(PLATFORM_WEB)
    AsyncRequest* async_req; // For Native async tracking
    #endif
    struct TextureEntry* next;
} TextureEntry;

struct TextureManager {
    TextureEntry* buckets[HASH_TABLE_SIZE];
    int next_request_id;
};

// --- Helper Functions ---

// Simple string hash function (djb2)
static unsigned long hash_string(const char* str) {
    unsigned long hash = 5381;
    int c;
    while ((c = *str++)) {
        hash = ((hash << 5) + hash) + c; /* hash * 33 + c */
    }
    return hash;
}

#if !defined(PLATFORM_WEB)
// Callback for curl to write data into our buffer
static size_t WriteMemoryCallback(void* contents, size_t size, size_t nmemb, void* userp) {
    size_t realsize = size * nmemb;
    AsyncRequest* req = (AsyncRequest*)userp;

    // Ensure we have enough memory
    if (req->size + realsize > req->capacity) {
        size_t new_capacity = req->capacity == 0 ? (realsize + 4096) : (req->capacity * 2 + realsize);
        unsigned char* ptr = realloc(req->data, new_capacity);
        if (!ptr) {
            fprintf(stderr, "[ERROR] Not enough memory (realloc returned NULL)\n");
            return 0;
        }
        req->data = ptr;
        req->capacity = new_capacity;
    }

    memcpy(&(req->data[req->size]), contents, realsize);
    req->size += realsize;
    
    return realsize;
}

// Worker thread function
static void* download_worker(void* arg) {
    AsyncRequest* req = (AsyncRequest*)arg;
    
    CURL* curl_handle = curl_easy_init();
    if (!curl_handle) {
        req->status = 2; // Error
        return NULL;
    }

    curl_easy_setopt(curl_handle, CURLOPT_URL, req->url);
    curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, WriteMemoryCallback);
    curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, (void*)req);
    curl_easy_setopt(curl_handle, CURLOPT_USERAGENT, "libcurl-agent/1.0");
    curl_easy_setopt(curl_handle, CURLOPT_TIMEOUT, 10L);
    curl_easy_setopt(curl_handle, CURLOPT_FOLLOWLOCATION, 1L);
    
    // Perform the request
    CURLcode res = curl_easy_perform(curl_handle);

    if (res != CURLE_OK) {
        fprintf(stderr, "[WARN] curl_easy_perform() failed: %s (URL: %s)\n", 
                curl_easy_strerror(res), req->url);
        req->status = 2; // Error
    } else {
        long response_code;
        curl_easy_getinfo(curl_handle, CURLINFO_RESPONSE_CODE, &response_code);
        if (response_code == 200) {
            req->status = 1; // Complete
        } else {
            fprintf(stderr, "[WARN] HTTP error %ld for URL: %s\n", response_code, req->url);
            req->status = 2; // Error
        }
    }

    curl_easy_cleanup(curl_handle);
    return NULL;
}
#endif

// Get file extension from URL/Path
static const char* get_file_extension(const char* url) {
    if (strstr(url, ".png") || strstr(url, ".PNG")) return ".png";
    if (strstr(url, ".jpg") || strstr(url, ".JPG") || strstr(url, ".jpeg")) return ".jpg";
    return ".png"; // Default fallback
}

// --- Implementation ---

TextureManager* create_texture_manager(void) {
    TextureManager* manager = (TextureManager*)malloc(sizeof(TextureManager));
    if (manager) {
        for (int i = 0; i < HASH_TABLE_SIZE; i++) {
            manager->buckets[i] = NULL;
        }
        manager->next_request_id = 1;
        #if !defined(PLATFORM_WEB)
        curl_global_init(CURL_GLOBAL_ALL);
        #endif
    }
    return manager;
}

void destroy_texture_manager(TextureManager* manager) {
    if (manager) {
        unload_all_textures(manager);
        free(manager);
        #if !defined(PLATFORM_WEB)
        curl_global_cleanup();
        #endif
    }
}

Texture2D get_texture(TextureManager* manager, const char* identifier) {
    if (!manager || !identifier) {
        Texture2D empty = {0};
        empty.id = 0;
        return empty;
    }

    unsigned long index = hash_string(identifier) % HASH_TABLE_SIZE;
    TextureEntry* entry = manager->buckets[index];

    while (entry) {
        if (strcmp(entry->key, identifier) == 0) {
            
            // --- WEB ASYNC CHECK ---
            #if defined(PLATFORM_WEB)
            if (entry->state == TEXTURE_STATE_LOADING) {
                int size = 0;
                unsigned char* data = js_get_fetch_result(entry->request_id, &size);
                
                if (data && size > 0) {
                    const char* ext = get_file_extension(identifier);
                    Image image = LoadImageFromMemory(ext, data, size);
                    
                    if (image.data != NULL) {
                        entry->texture = LoadTextureFromImage(image);
                        UnloadImage(image);
                        entry->state = TEXTURE_STATE_READY;
                    } else {
                        entry->state = TEXTURE_STATE_ERROR;
                        fprintf(stderr, "[WARN] Failed to load image from async data: %s\n", identifier);
                    }
                    free(data);
                } else if (size == -1) {
                    entry->state = TEXTURE_STATE_ERROR;
                    fprintf(stderr, "[WARN] Async fetch failed for: %s\n", identifier);
                }
            }
            #endif

            // --- NATIVE ASYNC CHECK ---
            #if !defined(PLATFORM_WEB)
            if (entry->state == TEXTURE_STATE_LOADING && entry->async_req) {
                if (entry->async_req->status == 1) { // Complete
                    // Load texture on main thread
                    const char* ext = get_file_extension(identifier);
                    Image image = LoadImageFromMemory(ext, entry->async_req->data, (int)entry->async_req->size);
                    
                    if (image.data != NULL) {
                        entry->texture = LoadTextureFromImage(image);
                        UnloadImage(image);
                        entry->state = TEXTURE_STATE_READY;
                    } else {
                        entry->state = TEXTURE_STATE_ERROR;
                        fprintf(stderr, "[WARN] Failed to decode image: %s\n", identifier);
                    }
                    
                    // Cleanup thread and data
                    pthread_join(entry->async_req->thread, NULL);
                    if (entry->async_req->data) free(entry->async_req->data);
                    if (entry->async_req->url) free(entry->async_req->url);
                    free(entry->async_req);
                    entry->async_req = NULL;
                    
                } else if (entry->async_req->status == 2) { // Error
                    entry->state = TEXTURE_STATE_ERROR;
                    
                    // Cleanup
                    pthread_join(entry->async_req->thread, NULL);
                    if (entry->async_req->data) free(entry->async_req->data);
                    if (entry->async_req->url) free(entry->async_req->url);
                    free(entry->async_req);
                    entry->async_req = NULL;
                }
            }
            #endif

            // Return texture if ready
            if (entry->state == TEXTURE_STATE_READY && entry->texture.id > 0) {
                return entry->texture;
            }
            
            // Return empty if loading or error
            Texture2D empty = {0};
            empty.id = 0;
            return empty;
        }
        entry = entry->next;
    }

    // Not found
    Texture2D empty = {0};
    empty.id = 0;
    return empty;
}

static void cache_texture(TextureManager* manager, const char* identifier, Texture2D texture, TextureState state) {
    if (!manager || !identifier) return;
    
    unsigned long index = hash_string(identifier) % HASH_TABLE_SIZE;
    
    // Check if already cached
    TextureEntry* entry = manager->buckets[index];
    while (entry) {
        if (strcmp(entry->key, identifier) == 0) {
            // Update existing entry
            if (entry->texture.id > 0 && entry->texture.id != texture.id) {
                UnloadTexture(entry->texture);
            }
            entry->texture = texture;
            entry->state = state;
            return;
        }
        entry = entry->next;
    }
    
    // Create new entry
    TextureEntry* new_entry = (TextureEntry*)malloc(sizeof(TextureEntry));
    if (!new_entry) return;
    
    new_entry->key = strdup(identifier);
    if (!new_entry->key) {
        free(new_entry);
        return;
    }
    
    new_entry->texture = texture;
    new_entry->state = state;
    new_entry->request_id = 0;
    #if !defined(PLATFORM_WEB)
    new_entry->async_req = NULL;
    #endif
    new_entry->next = manager->buckets[index];
    manager->buckets[index] = new_entry;
}

Texture2D load_texture_from_path(TextureManager* manager, const char* path) {
    if (!manager || !path) {
        Texture2D empty = {0};
        empty.id = 0;
        return empty;
    }
    
    // Check cache first
    Texture2D cached = get_texture(manager, path);
    if (cached.id > 0) return cached;

    // Load synchronously from filesystem
    Texture2D texture = LoadTexture(path);
    if (texture.id > 0) {
        cache_texture(manager, path, texture, TEXTURE_STATE_READY);
    } else {
        fprintf(stderr, "[ERROR] Failed to load texture from path: %s\n", path);
        Texture2D empty = {0};
        empty.id = 0;
        return empty;
    }
    return texture;
}

#if defined(PLATFORM_WEB)
// Async JavaScript fetch implementation for web platform
Texture2D load_texture_from_url(TextureManager* manager, const char* url) {
    if (!manager || !url) {
        Texture2D empty = {0};
        empty.id = 0;
        return empty;
    }
    
    // Check cache first
    Texture2D cached = get_texture(manager, url);
    if (cached.id > 0) return cached;

    // Check if entry exists but is loading/error
    unsigned long index = hash_string(url) % HASH_TABLE_SIZE;
    TextureEntry* entry = manager->buckets[index];
    while (entry) {
        if (strcmp(entry->key, url) == 0) {
            return entry->texture;
        }
        entry = entry->next;
    }

    // Start new async fetch
    int req_id = manager->next_request_id++;
    
    // Create entry in LOADING state
    TextureEntry* new_entry = (TextureEntry*)malloc(sizeof(TextureEntry));
    if (new_entry) {
        new_entry->key = strdup(url);
        new_entry->texture = (Texture2D){0};
        new_entry->state = TEXTURE_STATE_LOADING;
        new_entry->request_id = req_id;
        new_entry->next = manager->buckets[index];
        manager->buckets[index] = new_entry;
        
        // Trigger JS fetch
        js_start_fetch_binary(url, req_id);
    }

    Texture2D empty = {0};
    empty.id = 0;
    return empty;
}
#else
// Native implementation using pthreads and libcurl
Texture2D load_texture_from_url(TextureManager* manager, const char* url) {
    if (!manager || !url) {
        Texture2D empty = {0};
        empty.id = 0;
        return empty;
    }
    
    // Check cache first (this will also check completion of pending requests)
    Texture2D cached = get_texture(manager, url);
    if (cached.id > 0) return cached;

    // Check if entry exists (could be loading or error)
    unsigned long index = hash_string(url) % HASH_TABLE_SIZE;
    TextureEntry* entry = manager->buckets[index];
    while (entry) {
        if (strcmp(entry->key, url) == 0) {
            // If it's loading, return empty
            // If it's error, we might want to retry? For now, just return empty.
            return entry->texture;
        }
        entry = entry->next;
    }

    // Create new async request
    AsyncRequest* req = (AsyncRequest*)malloc(sizeof(AsyncRequest));
    if (!req) return (Texture2D){0};

    req->url = strdup(url);
    req->data = NULL;
    req->size = 0;
    req->capacity = 0;
    req->status = 0; // Pending

    // Create entry in LOADING state
    TextureEntry* new_entry = (TextureEntry*)malloc(sizeof(TextureEntry));
    if (new_entry) {
        new_entry->key = strdup(url);
        new_entry->texture = (Texture2D){0};
        new_entry->state = TEXTURE_STATE_LOADING;
        new_entry->request_id = 0;
        new_entry->async_req = req;
        new_entry->next = manager->buckets[index];
        manager->buckets[index] = new_entry;
        
        // Start worker thread
        if (pthread_create(&req->thread, NULL, download_worker, req) != 0) {
            fprintf(stderr, "[ERROR] Failed to create download thread for %s\n", url);
            req->status = 2; // Error
            new_entry->state = TEXTURE_STATE_ERROR;
        }
    } else {
        free(req->url);
        free(req);
    }

    Texture2D empty = {0};
    empty.id = 0;
    return empty;
}
#endif

Texture2D load_ui_icon(TextureManager* manager, const char* icon_name) {
    char url[512];
    snprintf(url, sizeof(url), "%s/ui-icons/%s", ASSETS_BASE_URL, icon_name);
    return load_texture_from_url(manager, url);
}

void unload_texture(TextureManager* manager, const char* identifier) {
    if (!manager || !identifier) return;

    unsigned long index = hash_string(identifier) % HASH_TABLE_SIZE;
    TextureEntry* entry = manager->buckets[index];
    TextureEntry* prev = NULL;

    while (entry) {
        if (strcmp(entry->key, identifier) == 0) {
            if (prev) {
                prev->next = entry->next;
            } else {
                manager->buckets[index] = entry->next;
            }
            
            // Cleanup texture
            if (entry->texture.id > 0) {
                UnloadTexture(entry->texture);
            }
            
            // Cleanup async request if still pending/active
            #if !defined(PLATFORM_WEB)
            if (entry->async_req) {
                // If thread is running, this is dangerous. 
                // Ideally we should cancel or join.
                // For now, we join (blocking) to ensure safety.
                pthread_join(entry->async_req->thread, NULL);
                if (entry->async_req->data) free(entry->async_req->data);
                if (entry->async_req->url) free(entry->async_req->url);
                free(entry->async_req);
            }
            #endif

            free(entry->key);
            free(entry);
            return;
        }
        prev = entry;
        entry = entry->next;
    }
}

void unload_all_textures(TextureManager* manager) {
    if (!manager) return;

    for (int i = 0; i < HASH_TABLE_SIZE; i++) {
        TextureEntry* entry = manager->buckets[i];
        while (entry) {
            TextureEntry* next = entry->next;
            
            if (entry->texture.id > 0) {
                UnloadTexture(entry->texture);
            }
            
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
}