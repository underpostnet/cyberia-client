#ifndef OBJECT_LAYER_H
#define OBJECT_LAYER_H
#include "domain/stat_contract_generated.h"

#include <stdbool.h>
#include <stdint.h>

#define MAX_ITEM_ID_LENGTH 64
#define MAX_TYPE_LENGTH 64
#define MAX_DESCRIPTION_LENGTH 256
#define MAX_FRAMES_PER_DIRECTION 64
#define MAX_CID_LENGTH 128
#define MAX_ADDRESS_LENGTH 128
#define MAX_TOKEN_ID_LENGTH 80   /* uint256 in decimal is at most 78 digits */

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
    OBJECT_LAYER_TYPE_STATIC     = 10,
} ObjectLayerType;

/* Token standard of an ItemLedger binding. */
typedef enum {
    LEDGER_UNREGISTERED = 0,
    LEDGER_ERC1155 = 1
} LedgerStandard;

typedef struct {
    char item_id[MAX_ITEM_ID_LENGTH];
    bool active;
    int quantity;
} ObjectLayerState;



/* Position and size of one frame in the atlas, in cells. The atlas texture
 * holds one pixel per cell, so the renderer clips this box out of it as is. */
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

/* The render metadata of one item: the layout of its primary render. The
 * per-direction arrays clip the animation frames out of that one texture. */
typedef struct {
    char item_key[MAX_ITEM_ID_LENGTH];
    int atlas_width;                    /* cells */
    int atlas_height;                   /* cells */
    int cell_pixel_dim;                 /* pixels per cell of the primary render PNG */
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

/* The render contract of a definition. Both CIDs are empty when it names no
 * render. The layout itself lives in AtlasSpriteSheetData, fetched at runtime. */
typedef struct {
    char cid[MAX_CID_LENGTH];           /* canonical render CID: the primary render PNG */
    char metadata_cid[MAX_CID_LENGTH];  /* canonical metadata CID: its layout */
} Render;

/* ItemLedger binding of the definition: a projection of chain state. Empty
 * (LEDGER_UNREGISTERED) when the definition is not registered. */
typedef struct {
    LedgerStandard standard;
    uint64_t chain_id;
    char contract_address[MAX_ADDRESS_LENGTH];
    char token_id[MAX_TOKEN_ID_LENGTH];   /* uint256, decimal */
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

/* One immutable Object Layer definition. `cid` is its canonical identity; the
 * item label the world maps it under is data.item.id. */
typedef struct {
    ObjectLayerData data;
    char cid[MAX_CID_LENGTH];
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

/* Parse "ERC1155". Any other value, the empty string included, is unregistered. */
LedgerStandard ledger_standard_from_string(const char* standard_str);

#endif // OBJECT_LAYER_H
