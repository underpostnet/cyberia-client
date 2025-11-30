#ifndef OBJECT_LAYERS_MANAGEMENT_H
#define OBJECT_LAYERS_MANAGEMENT_H

#include "object_layer.h"
#include "texture_manager.h"
#include <stddef.h>
#include <stdbool.h>

/**
 * @file object_layers_management.h
 * @brief Object layers management system with API fetching and caching
 *
 * This module handles:
 * - Fetching object layer definitions from the REST API
 * - Caching parsed object layers in memory
 * - Pre-caching textures for smooth rendering
 * - Building asset URIs for texture loading
 *
 * API Endpoints:
 * - GET {API_BASE_URL}/object-layers?item_id={item_id}&page=1&page_size=1
 *   Returns: { "items": [ { ObjectLayer } ] }
 *
 * Asset URLs:
 * - Format: {ASSETS_BASE_URL}/{item_type}/{item_id}/{direction_code}/{frame}.png
 *   Example: https://server.cyberiaonline.com/assets/skin/anon/08/0.png
 *
 * JSON Response Structure:
 * {
 *   "items": [
 *     {
 *       "id": "anon",
 *       "type": "skin",
 *       "description": "Anonymous player skin",
 *       "activable": false,
 *       "data": {
 *         "stats": {
 *           "effect": 0,
 *           "resistance": 0,
 *           "agility": 0,
 *           "range": 0,
 *           "intelligence": 0,
 *           "utility": 0
 *         },
 *         "render": {
 *           "frames": {
 *             "up_idle": [ ... ],
 *             "down_idle": [ ... ],
 *             "left_idle": [ ... ],
 *             "right_idle": [ ... ],
 *             "up_left_idle": [ ... ],
 *             "up_right_idle": [ ... ],
 *             "down_left_idle": [ ... ],
 *             "down_right_idle": [ ... ],
 *             "default_idle": [ ... ],
 *             "none_idle": [ ... ],
 *             "up_walking": [ ... ],
 *             "down_walking": [ ... ],
 *             "left_walking": [ ... ],
 *             "right_walking": [ ... ],
 *             "up_left_walking": [ ... ],
 *             "up_right_walking": [ ... ],
 *             "down_left_walking": [ ... ],
 *             "down_right_walking": [ ... ]
 *           },
 *           "frame_duration": 100,
 *           "is_stateless": false
 *         },
 *         "item": {
 *           "id": "anon",
 *           "type": "skin",
 *           "description": "Anonymous player skin",
 *           "activable": false
 *         }
 *       },
 *       "sha256": "abc123def456..."
 *     }
 *   ]
 * }
 *
 * Caching Strategy:
 * - First fetch: Network request → JSON parsing → Store in cache → Queue textures
 * - Subsequent fetches: Return from cache immediately
 * - Texture pre-caching: Happens on main thread via process_texture_caching_queue()
 * - Cache limits: MAX_LAYER_CACHE_SIZE, TEXTURE_QUEUE_SIZE (from config.h)
 *
 * Item Types Supported:
 * - "skin": Player/bot appearance
 * - "weapon": Equipped weapon
 * - "armor": Protective gear
 * - "accessory": Cosmetic items
 * - "skill": Active skill visual effect
 * - "coin": Currency/loot
 * - "floor": Environmental/background layer
 * - Any other string is treated as a custom item type
 */

// ============================================================================
// Forward Declarations
// ============================================================================

/**
 * @brief Opaque handle to the object layers manager
 *
 * Manages caching and fetching of object layer data.
 * Use create_object_layers_manager() and destroy_object_layers_manager()
 * for lifecycle management.
 */
typedef struct ObjectLayersManager ObjectLayersManager;

// ============================================================================
// Public API - Lifecycle Management
// ============================================================================

/**
 * @brief Creates a new ObjectLayersManager instance
 *
 * Initializes an empty cache and texture queue. The texture manager is stored
 * but not owned - it must remain valid for the lifetime of this manager.
 *
 * @param texture_manager Pointer to TextureManager for pre-caching textures
 *                        (must remain valid, not owned)
 * @return Pointer to new manager, or NULL on allocation failure
 *
 * Example:
 * @code
 *   TextureManager* tm = create_texture_manager();
 *   ObjectLayersManager* manager = create_object_layers_manager(tm);
 * @endcode
 */
ObjectLayersManager* create_object_layers_manager(TextureManager* texture_manager);

/**
 * @brief Destroys an ObjectLayersManager and frees all resources
 *
 * Frees all cached object layers, the texture queue, and the manager itself.
 * After this call, the pointer must not be used.
 *
 * @param manager The manager to destroy (may be NULL)
 *
 * Example:
 * @code
 *   destroy_object_layers_manager(manager);
 *   destroy_texture_manager(tm);
 * @endcode
 */
void destroy_object_layers_manager(ObjectLayersManager* manager);

// ============================================================================
// Public API - Object Layer Fetching and Caching
// ============================================================================

/**
 * @brief Retrieves an ObjectLayer by item ID, fetching from API if not cached
 *
 * This is the main entry point for getting object layer data. It:
 * 1. Checks the in-memory cache first (O(1) hash lookup)
 * 2. If not found, fetches from API via HTTP GET
 * 3. Parses JSON response to extract ObjectLayer data
 * 4. Stores in cache for future requests
 * 5. Queues textures for pre-caching on main thread
 *
 * API Request:
 * - Method: GET
 * - URL: {API_BASE_URL}/object-layers?item_id={item_id}&page=1&page_size=1
 * - Timeout: HTTP_TIMEOUT_SECONDS (10s default)
 * - Response: { "items": [ { ObjectLayer } ] }
 *
 * JSON Parsing:
 * - Uses cJSON library for robust JSON handling
 * - Extracts first item from "items" array
 * - Parses all frame arrays (up_idle, down_idle, etc.)
 * - Counts frames to determine animation length
 * - Preserves item type for correct asset URL construction
 *
 * Error Handling:
 * - Network errors: Returns NULL, logs to stderr
 * - HTTP errors (non-200): Returns NULL, logs status code
 * - JSON parse errors: Returns NULL, logs error
 * - Empty response: Returns NULL, logs warning
 *
 * @param manager The object layers manager instance
 * @param item_id The unique identifier for the item (e.g., "anon", "sword_basic")
 * @return Pointer to ObjectLayer on success, NULL on failure
 *         On success, the ObjectLayer is cached and textures are queued
 *         On failure, NULL is returned and error is logged
 *
 * Performance:
 * - Cache hit: O(1) hash table lookup, immediate return
 * - Cache miss: Network latency + JSON parsing (typically 50-500ms)
 * - Typical item size: 1-10 KB JSON
 *
 * Thread Safety:
 * - NOT thread-safe! Must be called from main thread only
 * - API fetch blocks the calling thread
 *
 * Example:
 * @code
 *   ObjectLayer* layer = get_or_fetch_object_layer(manager, "anon");
 *   if (layer) {
 *       printf("Item type: %s\n", layer->data.item.type);
 *       printf("Frame count: %d\n", layer->data.render.frames.down_idle_count);
 *   } else {
 *       printf("Failed to fetch layer\n");
 *   }
 * @endcode
 *
 * Caching Behavior:
 * - First call for "anon": Fetches from API, stores, returns
 * - Second call for "anon": Returns cached version immediately
 * - Cache size limited to MAX_LAYER_CACHE_SIZE (256)
 * - Old entries may be evicted when limit reached
 *
 * Frame Array Structure:
 * Each frame array is a list of frame indices (0, 1, 2, ...).
 * The count of frames in each array determines animation length.
 * Example for "down_idle": [0, 1, 2] means 3 frames
 *
 * Item Type Handling:
 * - The item type ("skin", "weapon", etc.) is critical
 * - It's used to construct asset URLs: /assets/{type}/{id}/{direction}/{frame}
 * - Different types may have different animation frame counts
 * - Unknown types are treated as generic (no special handling)
 */
ObjectLayer* get_or_fetch_object_layer(ObjectLayersManager* manager, const char* item_id);

// ============================================================================
// Public API - Texture Pre-caching
// ============================================================================

/**
 * @brief Processes the texture pre-caching queue
 *
 * This function should be called from the main render thread after graphics
 * initialization. It processes layers queued by get_or_fetch_object_layer()
 * and pre-loads all their textures into the TextureManager cache.
 *
 * What it does:
 * 1. Takes layers from the texture caching queue (FIFO order)
 * 2. For each layer, iterates through all animation directions
 * 3. Builds asset URLs for each frame
 * 4. Calls load_texture_from_url() to cache the texture
 * 5. Continues until queue is empty or max iterations reached
 *
 * Queue Processing:
 * - Max 100 layers processed per call (to avoid frame stuttering)
 * - Remaining items stay in queue for next call
 * - Call repeatedly until queue is empty
 *
 * Texture Loading:
 * - For each direction with frames: loads all frame images
 * - Uses build_object_layer_uri() to construct URLs
 * - Textures are cached by TextureManager
 * - Network requests happen here (blocking)
 * - Failed loads are logged but don't halt processing
 *
 * Performance Considerations:
 * - First frame after fetch may show unloaded textures
 * - Subsequent calls load more textures progressively
 * - Typical layer: 10-50 textures (0.5-2.5 MB total)
 * - Pre-caching prevents stuttering during gameplay
 * - Call early (during loading screen) for best results
 *
 * GPU Context Requirements:
 * - MUST be called from main thread with active OpenGL context
 * - TextureManager uses raylib (OpenGL) for texture creation
 * - Calling from other thread will cause GPU errors
 *
 * @param manager The object layers manager instance
 *
 * Example Usage:
 * @code
 *   // During initialization/loading
 *   while (!loading_complete) {
 *       // ... do other loading ...
 *       process_texture_caching_queue(manager);  // Process a batch
 *       // ... render loading screen ...
 *   }
 *
 *   // Or during gameplay for lazy loading
 *   void game_loop() {
 *       while (game_running) {
 *           process_texture_caching_queue(manager);  // 100 max per frame
 *           // ... game logic ...
 *           // ... rendering ...
 *       }
 *   }
 * @endcode
 *
 * Queue Behavior:
 * - Layers queued after each fetch
 * - Queue size limited to TEXTURE_QUEUE_SIZE (1024)
 * - If queue fills, new fetches skip texture queueing
 * - Call regularly to keep queue from backing up
 *
 * Logging:
 * - Prints count of textures cached per layer
 * - Logs item ID and type for debugging
 * - No output if queue is empty
 */
void process_texture_caching_queue(ObjectLayersManager* manager);

// ============================================================================
// Public API - Asset URI Building
// ============================================================================

/**
 * @brief Builds an asset URI for a specific object layer frame
 *
 * Constructs a complete URL for downloading a texture image from the
 * assets server. Used internally by texture loading and pre-caching.
 *
 * URI Format:
 * {ASSETS_BASE_URL}/{item_type}/{item_id}/{direction_code}/{frame}.png
 *
 * URI Components:
 * - ASSETS_BASE_URL: From config (e.g., https://server.cyberiaonline.com/assets)
 * - item_type: "skin", "weapon", "skill", "floor", etc.
 * - item_id: Item identifier (e.g., "anon", "sword_basic")
 * - direction_code: Two-digit code (e.g., "08" for down, "02" for up)
 * - frame: Frame number (0-indexed, e.g., 0, 1, 2, ...)
 *
 * Example Outputs:
 * - https://server.cyberiaonline.com/assets/skin/anon/08/0.png
 * - https://server.cyberiaonline.com/assets/weapon/sword/06/2.png
 * - https://server.cyberiaonline.com/assets/skill/fireball/04/5.png
 *
 * Direction Codes (from direction_converter.h):
 * - "02" = UP
 * - "04" = LEFT
 * - "06" = RIGHT
 * - "08" = DOWN (also "default_idle", "none_idle")
 * - "12" = UP walking
 * - "14" = LEFT walking
 * - "16" = RIGHT walking
 * - "18" = DOWN walking
 * - And diagonal combinations (e.g., "01" = UP_RIGHT, "03" = DOWN_RIGHT)
 *
 * @param buffer Output buffer for the URI string
 * @param buffer_size Size of the output buffer (must be at least 512 bytes)
 * @param item_type The type of item (e.g., "skin", "weapon")
 * @param item_id The unique item identifier
 * @param direction_code The two-digit direction code (e.g., "08")
 * @param frame The frame number (0-indexed)
 *
 * Return: void (always succeeds if inputs are valid)
 *         Writes URI to buffer as null-terminated string
 *         If inputs are invalid, writes error URI to buffer
 *
 * Buffer Requirements:
 * - Must be at least 512 bytes (typical URIs are 100-150 chars)
 * - Function uses snprintf with buffer bounds checking
 * - Never writes past buffer_size
 *
 * Preconditions:
 * - buffer must not be NULL
 * - buffer_size must be > 0 (typically 512)
 * - item_type must not be NULL or empty
 * - item_id must not be NULL or empty
 * - direction_code must not be NULL or empty
 * - frame must be >= 0
 *
 * Example:
 * @code
 *   char uri[512];
 *   build_object_layer_uri(uri, sizeof(uri), "skin", "anon", "08", 0);
 *   // uri now contains: https://server.cyberiaonline.com/assets/skin/anon/08/0.png
 *
 *   Texture2D texture = load_texture_from_url(texture_manager, uri);
 * @endcode
 *
 * Usage in Rendering:
 * - Called by entity_render.c to load textures for display
 * - Called by process_texture_caching_queue() for pre-caching
 * - Direction code comes from direction_converter
 * - Frame number from animation state
 * - Item type/id from ObjectLayer.data.item
 */
void build_object_layer_uri(
    char* buffer,
    size_t buffer_size,
    const char* item_type,
    const char* item_id,
    const char* direction_code,
    int frame
);

#endif // OBJECT_LAYERS_MANAGEMENT_H