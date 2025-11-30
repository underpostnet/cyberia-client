#include "texture_manager.h"
#include "config.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#if defined(PLATFORM_WEB)
    #include <emscripten.h>
    
    // External JavaScript functions for HTTP requests
    // Implemented in js/fetch_helper.js
    extern unsigned char* js_fetch_binary(const char* url, size_t* size);
#else
    #include <curl/curl.h>
#endif

#define HASH_TABLE_SIZE 256

// --- Data Structures ---

typedef struct TextureEntry {
    char* key;
    Texture2D texture;
    struct TextureEntry* next;
} TextureEntry;

struct TextureManager {
    TextureEntry* buckets[HASH_TABLE_SIZE];
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
// Memory struct for curl response
typedef struct {
    unsigned char* memory;
    size_t size;
} MemoryStruct;

static size_t WriteMemoryCallback(void* contents, size_t size, size_t nmemb, void* userp) {
    size_t realsize = size * nmemb;
    MemoryStruct* mem = (MemoryStruct*)userp;

    unsigned char* ptr = realloc(mem->memory, mem->size + realsize + 1);
    if (!ptr) {
        fprintf(stderr, "[ERROR] Not enough memory (realloc returned NULL)\n");
        return 0;
    }

    mem->memory = ptr;
    memcpy(&(mem->memory[mem->size]), contents, realsize);
    mem->size += realsize;
    mem->memory[mem->size] = 0;

    return realsize;
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
    if (!manager || !identifier) return (Texture2D){0};

    unsigned long index = hash_string(identifier) % HASH_TABLE_SIZE;
    TextureEntry* entry = manager->buckets[index];

    while (entry) {
        if (strcmp(entry->key, identifier) == 0) {
            return entry->texture;
        }
        entry = entry->next;
    }

    return (Texture2D){0}; // Return empty texture (id = 0)
}

static void cache_texture(TextureManager* manager, const char* identifier, Texture2D texture) {
    if (!manager || !identifier) return;

    unsigned long index = hash_string(identifier) % HASH_TABLE_SIZE;
    
    TextureEntry* new_entry = (TextureEntry*)malloc(sizeof(TextureEntry));
    new_entry->key = strdup(identifier);
    new_entry->texture = texture;
    new_entry->next = manager->buckets[index];
    manager->buckets[index] = new_entry;
}

Texture2D load_texture_from_path(TextureManager* manager, const char* path) {
    Texture2D cached = get_texture(manager, path);
    if (cached.id > 0) return cached;

    Texture2D texture = LoadTexture(path);
    if (texture.id > 0) {
        cache_texture(manager, path, texture);
    } else {
        fprintf(stderr, "[ERROR] Failed to load texture from path: %s\n", path);
    }
    return texture;
}

#if defined(PLATFORM_WEB)
// JavaScript fetch implementation for web platform
Texture2D load_texture_from_url(TextureManager* manager, const char* url) {
    Texture2D cached = get_texture(manager, url);
    if (cached.id > 0) return cached;

    // Use JavaScript fetch via Asyncify
    size_t data_size = 0;
    unsigned char* data = js_fetch_binary(url, &data_size);
    
    Texture2D texture = {0};
    
    if (data && data_size > 0) {
        // Determine file extension
        const char* ext = get_file_extension(url);
        
        // Load image from memory
        Image image = LoadImageFromMemory(ext, data, (int)data_size);
        
        if (image.data != NULL) {
            texture = LoadTextureFromImage(image);
            UnloadImage(image);
            
            if (texture.id > 0) {
                cache_texture(manager, url, texture);
            }
        }
        
        // Free the data allocated by JavaScript
        free(data);
    }

    return texture;
}
#else
// cURL implementation for native platforms
Texture2D load_texture_from_url(TextureManager* manager, const char* url) {
    Texture2D cached = get_texture(manager, url);
    if (cached.id > 0) return cached;

    CURL* curl_handle = curl_easy_init();
    if (!curl_handle) return (Texture2D){0};

    MemoryStruct chunk;
    chunk.memory = malloc(1);
    chunk.size = 0;

    curl_easy_setopt(curl_handle, CURLOPT_URL, url);
    curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, WriteMemoryCallback);
    curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, (void*)&chunk);
    curl_easy_setopt(curl_handle, CURLOPT_USERAGENT, "libcurl-agent/1.0");
    curl_easy_setopt(curl_handle, CURLOPT_TIMEOUT, 10L);
    // Follow redirects
    curl_easy_setopt(curl_handle, CURLOPT_FOLLOWLOCATION, 1L);

    CURLcode res = curl_easy_perform(curl_handle);

    Texture2D texture = {0};
    
if (res != CURLE_OK) {
    fprintf(stderr, "[ERROR] curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
} else {
        // Determine file extension
        const char* ext = get_file_extension(url);
        
        // Load image from memory
        Image image = LoadImageFromMemory(ext, chunk.memory, (int)chunk.size);
        
        if (image.data != NULL) {
            texture = LoadTextureFromImage(image);
            UnloadImage(image);
            
            if (texture.id > 0) {
                cache_texture(manager, url, texture);
            }
        }
    }

    curl_easy_cleanup(curl_handle);
    free(chunk.memory);

    return texture;
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
            
            UnloadTexture(entry->texture);
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
            UnloadTexture(entry->texture);
            free(entry->key);
            free(entry);
            entry = next;
        }
        manager->buckets[i] = NULL;
    }
}