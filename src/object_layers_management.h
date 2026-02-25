#ifndef OBJECT_LAYERS_MANAGEMENT_H
#define OBJECT_LAYERS_MANAGEMENT_H

#include "object_layer.h"
#include "texture_manager.h"
#include <stddef.h>
#include <stdbool.h>

/**
 * @file object_layers_management.h
 * @brief Object layers management system with atlas sprite sheet caching
 *
 * This module handles:
 * - Fetching AtlasSpriteSheet metadata (frame positions in consolidated atlas PNG)
 * - Fetching atlas PNG binary via the File blob API
 * - Caching atlas textures and frame metadata for efficient rendering
 * - Fetching ObjectLayer metadata for item type, stats, frame_duration, etc.
 *
 * All endpoints are public GET requests (no authentication required).
 *
 * Atlas Sprite Sheet Flow:
 *   1. GET {API_BASE_URL}/api/atlas-sprite-sheet/?filterModel=...&limit=1
 *      Returns: { status, data: { data: [ { fileId, metadata: { itemKey, frames, ... } } ] } }
 *   2. Extract fileId._id and FrameMetadata per direction
 *   3. GET {API_BASE_URL}/api/file/blob/{fileId}
 *      Returns: raw PNG binary buffer
 *   4. Load single atlas texture into GPU
 *   5. On render, clip individual frames using FrameMetadata source rects
 *
 * Object Layer Metadata Flow:
 *   1. GET {API_BASE_URL}/api/object-layer/?filterModel=...&limit=1
 *      Returns: { status, data: { data: [ { data: { item, stats, ... }, sha256 } ] } }
 *   2. Parse item type, stats, frame_duration, is_stateless
 *   3. Cache for rendering priority and animation timing
 *
 * Caching Strategy:
 * - First request for item: fetch atlas sprite sheet + file blob + object layer
 * - Subsequent requests: return cached data immediately
 * - Atlas texture: one GPU texture per item (loaded from consolidated PNG)
 * - Cache limits: MAX_LAYER_CACHE_SIZE, MAX_ATLAS_CACHE_SIZE (from config.h)
 */

// ============================================================================
// Forward Declarations
// ============================================================================

/**
 * @brief Opaque handle to the object layers manager
 *
 * Manages caching and fetching of atlas sprite sheet data, atlas textures,
 * and object layer metadata. Use create_object_layers_manager() and
 * destroy_object_layers_manager() for lifecycle management.
 */
typedef struct ObjectLayersManager ObjectLayersManager;

// ============================================================================
// Public API - Lifecycle Management
// ============================================================================

/**
 * @brief Creates a new ObjectLayersManager instance
 *
 * Initializes empty caches for object layers, atlas sprite sheets,
 * and atlas textures. Triggers authentication to the engine API.
 *
 * @param texture_manager Pointer to TextureManager for atlas texture caching
 *                        (must remain valid, not owned)
 * @return Pointer to new manager, or NULL on allocation failure
 */
ObjectLayersManager* create_object_layers_manager(TextureManager* texture_manager);

/**
 * @brief Destroys an ObjectLayersManager and frees all resources
 *
 * Frees all cached object layers, atlas data, and the manager itself.
 * Atlas textures cached in the TextureManager are NOT freed here;
 * they are owned by the TextureManager.
 *
 * @param manager The manager to destroy (may be NULL)
 */
void destroy_object_layers_manager(ObjectLayersManager* manager);

// ============================================================================
// Public API - Object Layer Fetching and Caching
// ============================================================================

/**
 * @brief Retrieves an ObjectLayer by item ID, fetching from API if not cached
 *
 * Fetches object layer metadata from the engine API. This provides:
 * - Item type (skin, weapon, etc.) for rendering priority
 * - Stats (effect, resistance, etc.)
 * - frame_duration for animation timing
 * - is_stateless flag
 *
 * Uses the engine API: GET {API_BASE_URL}/api/object-layer/
 * with filterModel for data.item.id matching.
 *
 * @param manager The object layers manager instance
 * @param item_id The unique identifier for the item (e.g., "anon")
 * @return Pointer to ObjectLayer on success, NULL on failure
 */
ObjectLayer* get_or_fetch_object_layer(ObjectLayersManager* manager, const char* item_id);

// ============================================================================
// Public API - Atlas Sprite Sheet Fetching and Caching
// ============================================================================

/**
 * @brief Retrieves AtlasSpriteSheetData by item key, fetching from API if not cached
 *
 * This is the primary function for obtaining rendering data. It:
 * 1. Checks the in-memory cache first (O(1) hash lookup)
 * 2. If not found, fetches from atlas-sprite-sheet API
 * 3. Parses frame metadata (FrameMetadataSchema) for all directions
 * 4. Extracts fileId for the consolidated atlas PNG
 * 5. Fetches the atlas PNG blob via the file API
 * 6. Loads the atlas as a single GPU texture
 * 7. Caches everything for future requests
 *
 * API Requests:
 * - Atlas metadata: GET {API_BASE_URL}/api/atlas-sprite-sheet/?filterModel=...&limit=1
 * - Atlas PNG blob: GET {API_BASE_URL}/api/file/blob/{fileId}
 *
 * @param manager The object layers manager instance
 * @param item_key The item identifier key (e.g., "anon", "sword_basic")
 * @return Pointer to AtlasSpriteSheetData on success, NULL on failure
 *
 * Performance:
 * - Cache hit: O(1) hash table lookup, immediate return
 * - Cache miss: Two network requests + PNG decode (typically 100-1000ms)
 * - Single atlas texture per item replaces N individual frame textures
 */
AtlasSpriteSheetData* get_or_fetch_atlas_data(ObjectLayersManager* manager, const char* item_key);

/**
 * @brief Retrieves the cached atlas GPU texture for a given file ID
 *
 * Returns the Texture2D for the consolidated atlas PNG that was loaded
 * when get_or_fetch_atlas_data() first fetched the item. The texture
 * is cached in the TextureManager by file_id key.
 *
 * @param manager The object layers manager instance
 * @param file_id The MongoDB ObjectId hex string of the atlas file
 * @return The atlas Texture2D. Returns empty texture (id=0) if not found or not loaded.
 */
Texture2D get_atlas_texture(ObjectLayersManager* manager, const char* file_id);

#endif // OBJECT_LAYERS_MANAGEMENT_H