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

typedef enum {
    OBJECT_LAYER_TYPE_UNKNOWN    = 0,
    OBJECT_LAYER_TYPE_FLOOR      = 1,
    OBJECT_LAYER_TYPE_OBSTACLE   = 2,
    OBJECT_LAYER_TYPE_PORTAL     = 3,
    OBJECT_LAYER_TYPE_FOREGROUND = 4,
    OBJECT_LAYER_TYPE_RESOURCE   = 5,
    OBJECT_LAYER_TYPE_SKIN       = 6,
    OBJECT_LAYER_TYPE_WEAPON     = 7,
    OBJECT_LAYER_TYPE_ICON       = 8,
    OBJECT_LAYER_TYPE_OTHER      = 9,
    OBJECT_LAYER_TYPE_STATIC     = 10,
} ObjectLayerType;

/* Economic classification. Mirrors the engine enum ['ERC20', 'ERC721',
 * 'OFF_CHAIN']. */
typedef enum {
    LEDGER_TYPE_OFF_CHAIN = 0,
    LEDGER_TYPE_ERC20 = 1,
    LEDGER_TYPE_ERC721 = 2
} LedgerType;

typedef struct {
    char item_id[MAX_ITEM_ID_LENGTH];
    bool active;
    int quantity;
} ObjectLayerState;

typedef struct {
    int effect;
    int resistance;
    int agility;
    int range;
    int intelligence;
    int utility;
} Stats;

/* Position and size of one frame inside the atlas PNG. The renderer clips
 * this sub-region out of the single atlas texture. */
typedef struct {
    int x;
    int y;
    int width;
    int height;
    int frame_index;
} FrameMetadata;

/* All frames of one direction and mode (e.g. "down_idle", "right_walking"). */
typedef struct {
    FrameMetadata frames[MAX_FRAMES_PER_DIRECTION];
    int count;
} DirectionFrameData;

/* Atlas sprite sheet of one object-layer item. `file_id` points at the
 * consolidated atlas PNG in the File API; the per-direction arrays clip the
 * animation frames out of that one texture. */
typedef struct {
    char item_key[MAX_ITEM_ID_LENGTH];
    char file_id[MAX_FILE_ID_LENGTH];   /* MongoDB ObjectId hex of the PNG */
    int atlas_width;                    /* pixels */
    int atlas_height;                   /* pixels */
    int cell_pixel_dim;                 /* pixel size of one cell */
    int frame_duration;                 /* ms per frame */

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

/* IPFS content identifiers of the atlas sprite sheet. Frame-level animation
 * data lives in AtlasSpriteSheetData, fetched at runtime. */
typedef struct {
    char cid[MAX_CID_LENGTH];           /* atlas PNG */
    char metadata_cid[MAX_CID_LENGTH];  /* atlas metadata JSON */
} Render;

/* Blockchain metadata that binds the visual prefab to its economic reality.
 * `address` is empty for OFF_CHAIN. */
typedef struct {
    LedgerType type;
    char address[MAX_ADDRESS_LENGTH];   /* Solidity contract address */
} Ledger;

/* Item.type is a string, to round-trip arbitrary engine-side categories;
 * type_kind is the parsed enum for hot-path comparisons. */
typedef struct {
    char            id[MAX_ITEM_ID_LENGTH];
    char            type[MAX_TYPE_LENGTH];
    ObjectLayerType type_kind;
    char            description[MAX_DESCRIPTION_LENGTH];
    bool            activable;
} Item;

typedef struct {
    Stats stats;
    Item item;
    Ledger ledger;
    Render render;
} ObjectLayerData;

/* The subset of the engine ObjectLayer document that the client needs. */
typedef struct {
    ObjectLayerData data;
    char sha256[65];    /* 64 hex chars plus the terminator */
} ObjectLayer;

/* Both create functions return NULL on allocation failure. Both free
 * functions accept NULL. */
ObjectLayer* create_object_layer(void);
void free_object_layer(ObjectLayer* layer);
AtlasSpriteSheetData* create_atlas_sprite_sheet_data(void);
void free_atlas_sprite_sheet_data(AtlasSpriteSheetData* data);

/* Frames for a direction and mode string (e.g. "down_idle"). NULL when the
 * atlas is NULL or the string is unknown. */
const DirectionFrameData* atlas_get_direction_frames(
    const AtlasSpriteSheetData* atlas,
    const char* dir_str
);

/* Parse "ERC20", "ERC721", or "OFF_CHAIN". Unknown values give OFF_CHAIN. */
LedgerType ledger_type_from_string(const char* type_str);

#endif // OBJECT_LAYER_H
