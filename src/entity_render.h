#ifndef ENTITY_RENDER_H
#define ENTITY_RENDER_H

#include "object_layers_management.h"
#include "object_layer.h"
#include <stdbool.h>

/**
 * @file entity_render.h
 * @brief Entity rendering system for animating and drawing object layers
 *
 * This module manages the rendering of game entities with animated object layers.
 * It handles:
 * - Animation state tracking per entity-item combination
 * - Frame progression and timing
 * - Direction and mode-based frame selection with fallbacks
 * - Texture caching and URL construction
 * - Proper z-order rendering based on item type priority
 * - Debug visualization when dev_ui is enabled
 *
 * The rendering pipeline:
 * 1. Collect all active layers for an entity
 * 2. Sort layers by item type priority (floor -> skin -> weapon -> skill -> coin)
 * 3. For each layer:
 *    - Select appropriate animation frame based on direction/mode
 *    - Update frame timing
 *    - Load and render texture to screen
 *
 * Animation State Lifecycle:
 * - Created on first render of an entity-item pair
 * - Persists across frames to maintain continuous animations
 * - Tracks last facing direction for idle fallback behavior
 * - Detects animation state changes and resets frame counter
 */

// ============================================================================
// Forward Declarations
// ============================================================================

/**
 * @struct EntityRender
 * @brief Opaque handle to the entity rendering system
 *
 * Contains animation state cache, texture manager reference,
 * and object layers manager reference.
 */
typedef struct EntityRender EntityRender;

// ============================================================================
// Public API - Lifecycle Management
// ============================================================================

/**
 * @brief Creates a new EntityRender system
 *
 * Initializes the rendering system with references to the object layers
 * manager and texture manager. Animation state cache is initialized empty.
 *
 * @param object_layers_manager Pointer to the ObjectLayersManager
 *                              (must remain valid for lifetime of EntityRender)
 * @param texture_manager Pointer to the TextureManager
 *                        (must remain valid for lifetime of EntityRender)
 * @return Pointer to new EntityRender system, or NULL on memory allocation failure
 *
 * @note The caller does NOT own the manager pointers - they must be freed
 *       separately and must outlive the EntityRender instance
 * @endcode
 */
EntityRender* create_entity_render(
    ObjectLayersManager* object_layers_manager,
    TextureManager* texture_manager
);

/**
 * @brief Destroys the EntityRender system and frees all resources
 *
 * Frees all animation state entries in the cache and the main structure.
 * After this call, the pointer must not be used.
 *
 * @param render Pointer to EntityRender to destroy (may be NULL)
 *
 * @note This does NOT destroy the object layers manager or texture manager
 *       - those must be freed separately
 */
void destroy_entity_render(EntityRender* render);

// ============================================================================
// Public API - Rendering
// ============================================================================

/**
 * @brief Renders all animated object layers for a single entity
 *
 * This is the main entry point for entity rendering. It:
 * 1. Draws a debug box if dev_ui is enabled (shows entity boundaries)
 * 2. Skips object layer rendering if dev_ui is enabled
 * 3. If dev_ui is disabled, renders all active layers with animations
 *
 * The rendering respects the following ordering:
 * - Lower-priority types (floor) render first (bottom of screen)
 * - Higher-priority types (coin) render last (top of screen)
 * - This ensures proper visual layering (z-order)
 *
 * Animation Behavior:
 * - Each unique (entity_id, item_id) pair has persistent animation state
 * - Frame advances automatically based on frame_duration from object layer
 * - Animation resets when state string (direction_mode combo) changes
 * - When idle with no direction, uses last known facing direction
 *
 * Dev UI Mode:
 * - When dev_ui=true: draws colored bounding boxes
 *   - Cyan: self/player
 *   - Orange: other players
 *   - Green: friendly bots
 *   - Red: hostile bots
 *   - Dark gray: floor
 * - When dev_ui=true: SKIPS object layer rendering
 * - When dev_ui=false: ONLY renders object layers
 *
 * @param render Pointer to EntityRender system
 * @param entity_id Unique identifier for this entity (e.g., "player123", "bot456")
 * @param pos_x World X position in grid coordinates (will be scaled by cell_size)
 * @param pos_y World Y position in grid coordinates (will be scaled by cell_size)
 * @param width Entity width in grid units (will be scaled by cell_size)
 * @param height Entity height in grid units (will be scaled by cell_size)
 * @param direction Current facing direction (from Direction enum)
 * @param mode Current animation mode (from ObjectLayerMode enum)
 * @param layers_state Array of ObjectLayerState pointers (may be NULL if layers_count=0)
 * @param layers_count Number of layers in the array (0 is valid, means no layers)
 * @param entity_type String type identifier:
 *                    - "self": local player
 *                    - "other": another player
 *                    - "bot": NPC/bot entity
 *                    - "floor": environment layer
 *                    (only used for dev_ui coloring)
 * @param dev_ui Whether developer UI mode is enabled
 *               - true: draw debug boxes, skip layer rendering
 *               - false: skip debug boxes, render layers
 * @param cell_size Size of a grid cell in pixels
 *                  (used to scale grid coordinates to screen coordinates)
 *                  If <= 0, defaults to 12.0 pixels per cell
 *
 * Return: void (always succeeds, invalid inputs are silently skipped)
 *
 * Example Usage:
 * @code
 *   // Define object layer state for an entity
 *   ObjectLayerState layer1, layer2;
 *   layer1.itemId = "anon";  // skin
 *   layer1.active = true;
 *   layer1.quantity = 1;
 *
 *   layer2.itemId = "sword";  // weapon
 *   layer2.active = true;
 *   layer2.quantity = 1;
 *
 *   ObjectLayerState* layers[] = { &layer1, &layer2 };
 *
 *   // Render entity at grid position (10, 15), size 1x2, facing right, walking
 *   draw_entity_layers(
 *       render_system,
 *       "player123",          // entity_id
 *       10.0f, 15.0f,         // pos_x, pos_y in grid cells
 *       1.0f, 2.0f,           // width, height in grid cells
 *       DIRECTION_RIGHT,      // facing right
 *       MODE_WALKING,         // walking animation
 *       layers,               // array of layer states
 *       2,                    // 2 layers
 *       "self",               // it's the player
 *       false,                // dev_ui disabled (render normally)
 *       32.0f                 // 32 pixels per grid cell
 *   );
 * @endcode
 *
 * Implementation Notes:
 * - Animation state is cached per (entity_id, item_id) pair
 * - Direction memory persists so idle entities retain facing direction
 * - Frame index is automatically clamped to valid range
 * - Missing layers are silently skipped (no error messages)
 * - Textures are loaded through the TextureManager and cached there
 * - All rendering uses raylib's DrawTexturePro for flexible positioning
 *
 * Performance Considerations:
 * - Hash table lookup for animation state: O(1) average, O(n) worst case
 * - Layer sorting: O(n log n) where n = number of active layers
 * - Typical entity has 1-4 layers, so sorting is negligible
 * - Texture loading uses cache (hit = immediate, miss = network request)
 *
 * Thread Safety:
 * - NOT thread-safe! Call only from main render thread
 * - Animation state cache is not protected by locks
 * - ObjectLayersManager and TextureManager may have their own thread safety
 */
void draw_entity_layers(
    EntityRender* render,
    const char* entity_id,
    float pos_x,
    float pos_y,
    float width,
    float height,
    Direction direction,
    ObjectLayerMode mode,
    ObjectLayerState** layers_state,
    int layers_count,
    const char* entity_type,
    bool dev_ui,
    float cell_size
);

#endif // ENTITY_RENDER_H