#ifndef OBJECT_LAYER_H
#define OBJECT_LAYER_H

#include <stdbool.h>

#define MAX_ITEM_ID_LENGTH 64
#define MAX_TYPE_LENGTH 64
#define MAX_DESCRIPTION_LENGTH 256

// Enums corresponding to Python enums
typedef enum {
    DIRECTION_UP = 0,
    DIRECTION_UP_RIGHT = 1,
    DIRECTION_RIGHT = 2,
    DIRECTION_DOWN_RIGHT = 3,
    DIRECTION_DOWN = 4,
    DIRECTION_DOWN_LEFT = 5,
    DIRECTION_LEFT = 6,
    DIRECTION_UP_LEFT = 7,
    DIRECTION_NONE = 8
} Direction;

typedef enum {
    MODE_IDLE = 0,
    MODE_WALKING = 1,
    MODE_TELEPORTING = 2
} ObjectLayerMode;

// Object layer state
typedef struct {
    char item_id[MAX_ITEM_ID_LENGTH];
    bool active;
    int quantity;
} ObjectLayerState;

// Stats structure
typedef struct {
    int effect;
    int resistance;
    int agility;
    int range;
    int intelligence;
    int utility;
} Stats;

// Render frames structure (simplified for C)
// Stores the count of frames for each animation state.
// The actual frame data (3D matrices) is not stored here as we fetch textures.
typedef struct {
    int up_idle_count;
    int down_idle_count;
    int right_idle_count;
    int left_idle_count;
    int up_right_idle_count;
    int down_right_idle_count;
    int up_left_idle_count;
    int down_left_idle_count;
    int default_idle_count;
    int up_walking_count;
    int down_walking_count;
    int right_walking_count;
    int left_walking_count;
    int up_right_walking_count;
    int down_right_walking_count;
    int up_left_walking_count;
    int down_left_walking_count;
    int none_idle_count;
} RenderFrames;

// Render structure
typedef struct {
    RenderFrames frames;
    // colors omitted for now as we use textures
    int frame_duration;
    bool is_stateless;
} Render;

// Item structure
typedef struct {
    char id[MAX_ITEM_ID_LENGTH];
    char type[MAX_TYPE_LENGTH];
    char description[MAX_DESCRIPTION_LENGTH];
    bool activable;
} Item;

// Object layer data
typedef struct {
    Stats stats;
    Render render;
    Item item;
} ObjectLayerData;

// Object layer
typedef struct {
    ObjectLayerData data;
    char sha256[65]; // 64 chars + null terminator
} ObjectLayer;

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