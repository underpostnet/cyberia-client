#ifndef OBJECT_LAYER_H
#define OBJECT_LAYER_H

#include <stdbool.h>

#define MAX_ITEM_ID_LENGTH 64
#define MAX_TYPE_LENGTH 64
#define MAX_DESCRIPTION_LENGTH 256
#define MAX_FRAMES_PER_DIRECTION 64
#define MAX_FILE_ID_LENGTH 128
#define MAX_CID_LENGTH 128
#define MAX_ADDRESS_LENGTH 128

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

/**
 * @brief Blockchain ledger type for economic classification.
 *
 * Maps to the Mongoose enum: ['ERC20', 'ERC721', 'OFF_CHAIN']
 */
typedef enum {
    LEDGER_TYPE_OFF_CHAIN = 0,
    LEDGER_TYPE_ERC20 = 1,
    LEDGER_TYPE_ERC721 = 2
} LedgerType;

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

// ============================================================================
// Atlas Sprite Sheet Structures (FrameMetadataSchema)
// ============================================================================

/**
 * @brief Metadata for a single frame within an atlas sprite sheet.
 *
 * Corresponds to the FrameMetadataSchema in the engine's
 * AtlasSpriteSheetModel. Each frame stores its position and
 * dimensions inside the consolidated atlas PNG so the renderer
 * can clip (crop) the correct sub-region.
 */
typedef struct {
    int x;           /**< X position in the atlas (pixels) */
    int y;           /**< Y position in the atlas (pixels) */
    int width;       /**< Frame width (pixels) */
    int height;      /**< Frame height (pixels) */
    int frame_index; /**< Frame index in animation sequence */
} FrameMetadata;

/**
 * @brief Array of FrameMetadata for a single animation direction.
 *
 * Groups all frames belonging to one direction/mode combination
 * (e.g. "down_idle", "right_walking") together with a count.
 */
typedef struct {
    FrameMetadata frames[MAX_FRAMES_PER_DIRECTION];
    int count;
} DirectionFrameData;

/**
 * @brief Consolidated atlas sprite sheet data for one object layer item.
 *
 * Mirrors the engine's AtlasSpriteSheetModel. Contains the fileId
 * referencing the consolidated atlas PNG stored via the File API,
 * overall atlas dimensions, and per-direction frame metadata arrays
 * used to clip individual animation frames from the single texture.
 *
 * Workflow:
 *  1. Fetch AtlasSpriteSheet doc by metadata.itemKey
 *  2. Extract fileId and frame metadata
 *  3. Fetch atlas PNG binary via GET /api/file/blob/{fileId}
 *  4. Load the single atlas image as a GPU texture
 *  5. On each render frame, use the DirectionFrameData source rects
 */
typedef struct {
    char item_key[MAX_ITEM_ID_LENGTH];     /**< Item identifier (metadata.itemKey) */
    char file_id[MAX_FILE_ID_LENGTH];      /**< MongoDB ObjectId hex of the atlas PNG file */
    int atlas_width;                        /**< Total atlas width in pixels */
    int atlas_height;                       /**< Total atlas height in pixels */
    int cell_pixel_dim;                     /**< Pixel dimension of each cell */

    /* Per-direction frame metadata arrays (DirectionFramesSchema) */
    DirectionFrameData up_idle;
    DirectionFrameData down_idle;
    DirectionFrameData right_idle;
    DirectionFrameData left_idle;
    DirectionFrameData up_right_idle;
    DirectionFrameData down_right_idle;
    DirectionFrameData up_left_idle;
    DirectionFrameData down_left_idle;
    DirectionFrameData default_idle;
    DirectionFrameData up_walking;
    DirectionFrameData down_walking;
    DirectionFrameData right_walking;
    DirectionFrameData left_walking;
    DirectionFrameData up_right_walking;
    DirectionFrameData down_right_walking;
    DirectionFrameData up_left_walking;
    DirectionFrameData down_left_walking;
    DirectionFrameData none_idle;
} AtlasSpriteSheetData;

// ============================================================================
// Render Structure (new: IPFS content identifiers for atlas sprite sheet)
// ============================================================================

/**
 * @brief IPFS content identifiers for the consolidated atlas sprite sheet.
 *
 * Corresponds to the RenderSchema in the new ObjectLayer model:
 *   - cid:         IPFS CID for the consolidated atlas sprite sheet PNG
 *   - metadataCid: IPFS CID for the atlas sprite sheet metadata JSON
 *
 * These replace the legacy RenderFrames / frame_duration / is_stateless
 * fields that previously lived directly on the ObjectLayer. Frame-level
 * data (frame_duration, is_stateless, per-direction frame counts) now
 * lives exclusively on the ObjectLayerRenderFrames collection and on
 * the AtlasSpriteSheetData metadata fetched at runtime.
 */
typedef struct {
    char cid[MAX_CID_LENGTH];              /**< IPFS CID for the atlas PNG */
    char metadata_cid[MAX_CID_LENGTH];     /**< IPFS CID for the atlas metadata JSON */
} Render;

// ============================================================================
// Ledger Structure (blockchain protocol metadata)
// ============================================================================

/**
 * @brief Blockchain protocol metadata linking the visual object-layer
 *        prefab to its economic reality.
 *
 * Corresponds to the LedgerSchema in the ObjectLayer model:
 *   - type:    Token standard or off-chain designation (ERC20, ERC721, OFF_CHAIN)
 *   - address: Solidity smart-contract address (may be empty for OFF_CHAIN)
 */
typedef struct {
    LedgerType type;                        /**< ERC20, ERC721, or OFF_CHAIN */
    char address[MAX_ADDRESS_LENGTH];       /**< Solidity contract address */
} Ledger;

// ============================================================================
// Item Structure
// ============================================================================

// Item structure
typedef struct {
    char id[MAX_ITEM_ID_LENGTH];
    char type[MAX_TYPE_LENGTH];
    char description[MAX_DESCRIPTION_LENGTH];
    bool activable;
} Item;

// ============================================================================
// ObjectLayer Structures
// ============================================================================

/**
 * @brief Core data payload of an ObjectLayer document.
 *
 * New schema (matches object-layer.model.js):
 *   data.stats  — mechanical attributes
 *   data.item   — human-readable item info
 *   data.ledger — blockchain / economic metadata
 *   data.render — IPFS CIDs for the atlas sprite sheet
 */
typedef struct {
    Stats stats;
    Item item;
    Ledger ledger;
    Render render;
} ObjectLayerData;

/**
 * @brief Top-level ObjectLayer document.
 *
 * The C client only needs a subset of the full Mongoose document.
 *
 * frame_duration and is_stateless are NOT part of the ObjectLayer Mongoose
 * schema (data.render now holds IPFS CIDs only).  They live on the separate
 * ObjectLayerRenderFrames collection, but the engine API populates them
 * into the response via:
 *
 *   .populate('objectLayerRenderFramesId', { _id:1, frame_duration:1, is_stateless:1 })
 *
 * The C parser extracts them from the populated reference and stores them
 * here as first-class animation metadata so the render loop can use them
 * without an extra fetch.
 */
typedef struct {
    ObjectLayerData data;
    char sha256[65];    // 64 hex chars + null terminator

    /* Animation metadata – from populated objectLayerRenderFramesId */
    int  frame_duration; /**< ms per frame (from ObjectLayerRenderFrames) */
    bool is_stateless;   /**< true ⇒ always use default_idle direction  */
} ObjectLayer;

// ============================================================================
// Function Prototypes
// ============================================================================

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

/**
 * @brief Create a new AtlasSpriteSheetData with default (zeroed) values
 * @return Pointer to new AtlasSpriteSheetData, or NULL on allocation failure
 */
AtlasSpriteSheetData* create_atlas_sprite_sheet_data(void);

/**
 * @brief Free an AtlasSpriteSheetData and all its resources
 * @param data The AtlasSpriteSheetData to free (may be NULL)
 */
void free_atlas_sprite_sheet_data(AtlasSpriteSheetData* data);

/**
 * @brief Look up the DirectionFrameData for a given direction string.
 *
 * Maps animation state names (e.g. "down_idle", "right_walking",
 * "default_idle") to the corresponding DirectionFrameData inside
 * an AtlasSpriteSheetData struct.
 *
 * @param atlas   The atlas sprite sheet data to search
 * @param dir_str The direction/mode string (e.g. "down_idle")
 * @return Pointer to the matching DirectionFrameData, or NULL if
 *         atlas is NULL or dir_str is unrecognised
 */
const DirectionFrameData* atlas_get_direction_frames(
    const AtlasSpriteSheetData* atlas,
    const char* dir_str
);

/**
 * @brief Parse a LedgerType enum value from a JSON string.
 *
 * @param type_str  The string to parse ("ERC20", "ERC721", "OFF_CHAIN")
 * @return The corresponding LedgerType, defaulting to LEDGER_TYPE_OFF_CHAIN
 */
LedgerType ledger_type_from_string(const char* type_str);

/**
 * @brief Convert a LedgerType enum value to its canonical string.
 *
 * @param type  The LedgerType value
 * @return Static string ("ERC20", "ERC721", or "OFF_CHAIN")
 */
const char* ledger_type_to_string(LedgerType type);

#endif // OBJECT_LAYER_H