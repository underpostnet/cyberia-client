#ifndef OBJECT_LAYERS_MANAGEMENT_H
#define OBJECT_LAYERS_MANAGEMENT_H

#include "object_layer.h"
#include <raylib.h>
#include <cJSON.h>
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
 * - Fetching ObjectLayer metadata for item type, stats, ledger, render CIDs, etc.
 *
 * All endpoints are public GET requests (no authentication required).
 *
 * Atlas Sprite Sheet Flow:
 *   1. GET /api/atlas-sprite-sheet/?filterModel=...&limit=1
 *      Returns: { status, data: { data: [ { fileId, metadata: { itemKey, frames, ... } } ] } }
 *   2. Extract fileId._id and FrameMetadata per direction
 *   3. GET /api/file/blob/{fileId}
 *      Returns: raw PNG binary buffer
 *   4. Load single atlas texture into GPU
 *   5. On render, clip individual frames using FrameMetadata source rects
 *
 * Object Layer Metadata Flow:
 *   1. GET /api/object-layer/?filterModel=...&limit=1
 *      Returns: { status, data: { data: [ { data: { item, stats, ledger, render, ... }, sha256 } ] } }
 *   2. Parse item type, stats, ledger (blockchain metadata), render CIDs (IPFS)
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

void create_object_layers_manager(void);
void destroy_object_layers_manager(void);
ObjectLayersManager* obj_layers_mgr_get(void);

// ============================================================================
// Public API - Object Layer Fetching and Caching
// ============================================================================

ObjectLayer* lookup_cached_layer(const char* item_id);

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
 * - Atlas metadata: GET /api/atlas-sprite-sheet/?filterModel=...&limit=1
 * - Atlas PNG blob: GET /api/file/blob/{fileId}
 *
 * @param item_key The item identifier key (e.g., "anon", "sword_basic")
 * @return Pointer to AtlasSpriteSheetData on success, NULL on failure
 *
 * Performance:
 * - Cache hit: O(1) hash table lookup, immediate return
 * - Cache miss: Two network requests + PNG decode (typically 100-1000ms)
 * - Single atlas texture per item replaces N individual frame textures
 */
AtlasSpriteSheetData* get_or_fetch_atlas_data(const char* item_key);

/**
 * @brief Retrieves the cached atlas GPU texture for a given item key
 *
 * Returns the Texture2D for the consolidated atlas PNG. The texture
 * is fetched asynchronously from the atlas-sprite-sheet blob API
 * using the item key and cached for subsequent calls.
 *
 * @param item_key The item identifier key (e.g., "anon", "lain")
 * @return The atlas Texture2D. Returns empty texture (id=0) if not found or not loaded.
 */
Texture2D get_atlas_texture(const char* item_key);

// ============================================================================
// Public API - Cache Population from WebSocket Metadata
// ============================================================================

/**
 * @brief Parse and cache an ObjectLayer from a JSON object (from WS metadata message).
 *
 * The JSON object has the same shape as a single OL item:
 *   { "sha256": "...", "data": { "stats": {...}, "item": {...}, ... } }
 *
 * @param item_id The item ID key for this ObjectLayer
 * @param ol_json cJSON object representing the ObjectLayer (caller retains ownership)
 */
void populate_object_layer_from_json(const char* item_id, const cJSON* ol_json);

/* Dead — no external callers. Atlas population flows through
 * obj_layers_mgr_schedule_atlas_fetch + on_atlas_meta_fetched (REST path).
 *
 * void populate_atlas_from_json(ObjectLayersManager* manager, const char* item_key, const cJSON* atlas_json);
 */

/**
 * @brief Schedule a REST fetch for atlas sprite sheet metadata by item_key.
 *
 * Triggers: GET /api/atlas-sprite-sheet/metadata/{item_key}
 * Response: { status, data: { metadata: { itemKey, atlasWidth, atlasHeight,
 *                                         cellPixelDim, frame_duration, frames }, cid } }
 *
 * After metadata is received, get_atlas_texture() automatically fetches
 * and caches the PNG blob on the next call.
 *
 * Safe to call multiple times — repeated calls for the same item_key are no-ops.
 *
 * @param item_key The item identifier key (= metadata.itemKey from the atlas doc)
 */
void obj_layers_mgr_schedule_atlas_fetch(const char* item_key);

#endif // OBJECT_LAYERS_MANAGEMENT_H
