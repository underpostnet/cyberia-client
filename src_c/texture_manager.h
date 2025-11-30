#ifndef TEXTURE_MANAGER_H
#define TEXTURE_MANAGER_H

#include "raylib.h"
#include <stdbool.h>

// Forward declaration for the manager struct
// This hides the implementation details (like the cache structure) from the user
typedef struct TextureManager TextureManager;

/**
 * @brief Creates a new TextureManager instance.
 * @return A pointer to the new TextureManager.
 */
TextureManager* create_texture_manager(void);

/**
 * @brief Destroys the TextureManager and unloads all textures.
 * @param manager The TextureManager to destroy.
 */
void destroy_texture_manager(TextureManager* manager);

/**
 * @brief Loads a texture from a file path.
 * If cached, returns the cached texture.
 * @param manager The TextureManager.
 * @param path The file path.
 * @return The loaded Texture2D. Returns an empty texture (id=0) on failure.
 */
Texture2D load_texture_from_path(TextureManager* manager, const char* path);

/**
 * @brief Loads a texture from a URL.
 * If cached, returns the cached texture.
 * @param manager The TextureManager.
 * @param url The URL.
 * @return The loaded Texture2D. Returns an empty texture (id=0) on failure.
 */
Texture2D load_texture_from_url(TextureManager* manager, const char* url);

/**
 * @brief Loads a UI icon from the assets server.
 * @param manager The TextureManager.
 * @param icon_name The name of the icon file (e.g., "close-yellow.png").
 * @return The loaded Texture2D.
 */
Texture2D load_ui_icon(TextureManager* manager, const char* icon_name);

/**
 * @brief Retrieves a texture from the cache.
 * @param manager The TextureManager.
 * @param identifier The path or URL used as key.
 * @return The Texture2D. Returns an empty texture (id=0) if not found.
 */
Texture2D get_texture(TextureManager* manager, const char* identifier);

/**
 * @brief Unloads a specific texture.
 * @param manager The TextureManager.
 * @param identifier The path or URL used as key.
 */
void unload_texture(TextureManager* manager, const char* identifier);

/**
 * @brief Unloads all textures in the cache.
 * @param manager The TextureManager.
 */
void unload_all_textures(TextureManager* manager);

#endif // TEXTURE_MANAGER_H