#ifndef OBJECT_LAYER_H
#define OBJECT_LAYER_H

#include "game_state.h"
#include <stdbool.h>

/**
 * @file object_layer.h
 * @brief Object layer structures and functions
 * 
 * This module defines object layer data structures used for rendering
 * and managing entity appearance layers (skins, weapons, armor, etc.).
 * 
 * Note: Direction, ObjectLayerMode, Stats, Item, Render, RenderFrames,
 * ObjectLayerData, and ObjectLayerState are defined in game_state.h
 * to avoid circular dependencies and duplicate definitions.
 */

// ============================================================================
// Function Prototypes
// ============================================================================

/**
 * Note: ObjectLayer is defined in game_state.h
 * It contains ObjectLayerData and sha256 hash field.
 */

/**
 * @brief Create a new ObjectLayer with default values
 * @return Pointer to new ObjectLayer, or NULL on allocation failure
 */
ObjectLayer* create_object_layer(void);

/**
 * @brief Free an ObjectLayer and all its resources
 * @param layer The ObjectLayer to free (may be NULL)
 */
void free_object_layer(ObjectLayer* layer);

/**
 * @brief Create a new ObjectLayerState with default values
 * @return Pointer to new ObjectLayerState, or NULL on allocation failure
 */
ObjectLayerState* create_object_layer_state(void);

/**
 * @brief Free an ObjectLayerState and all its resources
 * @param state The ObjectLayerState to free (may be NULL)
 */
void free_object_layer_state(ObjectLayerState* state);

#endif // OBJECT_LAYER_H