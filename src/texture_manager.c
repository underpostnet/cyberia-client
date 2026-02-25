#include "texture_manager.h"
#include <string.h>
#include <stdio.h>

#include <emscripten.h>

// External JavaScript functions for async HTTP requests
extern void js_start_fetch_binary(const char* url, int request_id);
extern unsigned char* js_get_fetch_result(int request_id, int* size);

#define HASH_TABLE_SIZE 256

// --- Data Structures ---

typedef enum {
    TEXTURE_STATE_NONE,
    TEXTURE_STATE_LOADING,
    TEXTURE_STATE_READY,
    TEXTURE_STATE_ERROR
} TextureState;


typedef struct TextureEntry {
    char* key;
    Texture2D texture;
    TextureState state;
    int request_id; // For Web async tracking
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

// Get file extension from URL/Path
static const char* get_file_extension(const char* url) {
    if (strstr(url, ".png") || strstr(url, ".PNG")) return ".png";
    if (strstr(url, ".jpg") || strstr(url, ".JPG") || strstr(url, ".jpeg")) return ".jpg";
    return ".png"; // Default fallback
}

// --- Implementation ---

TextureManager* create_texture_manager(void) {
    TextureManager* manager = malloc(sizeof(TextureManager));
    if (manager) {
        for (int i = 0; i < HASH_TABLE_SIZE; i++) {
            manager->buckets[i] = NULL;
        }
        manager->next_request_id = 1;
    }
    return manager;
}

void destroy_texture_manager(TextureManager* manager) {
    if (manager) {
        unload_all_textures(manager);
        free(manager);
    }
}

Texture2D get_texture(TextureManager* manager, const char* identifier) {
    if (!manager || !identifier)
    {
        return (Texture2D){0};
    }

    unsigned long index = hash_string(identifier) % HASH_TABLE_SIZE;
    TextureEntry* entry = manager->buckets[index];

    while (entry) {
        if (strcmp(entry->key, identifier) == 0) {

            // --- WEB ASYNC CHECK ---
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
    return (Texture2D){0};
}

// Async JavaScript fetch implementation for web platform
Texture2D load_texture_from_url(TextureManager* manager, const char* url) {
    if (!manager || !url)
    {
        return (Texture2D){0};
    }

    // Check cache first
    Texture2D cached = get_texture(manager, url);
    if (cached.id > 0)
    {
        return cached;
    }

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
    TextureEntry* new_entry = malloc(sizeof(TextureEntry));
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

    return (Texture2D){0};
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

            free(entry->key);
            free(entry);
            entry = next;
        }
        manager->buckets[i] = NULL;
    }
}