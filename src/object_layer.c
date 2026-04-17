#include "object_layer.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>

ObjectLayer* create_object_layer(void) {
    ObjectLayer* layer = (ObjectLayer*)calloc(1, sizeof(ObjectLayer));
    if (layer) {
        layer->data.ledger.type = LEDGER_TYPE_OFF_CHAIN;
        layer->frame_duration = 100; // default 100ms per frame
    }
    return layer;
}

void free_object_layer(ObjectLayer* layer) {
    free(layer);
}

AtlasSpriteSheetData* create_atlas_sprite_sheet_data(void) {
    AtlasSpriteSheetData* data = (AtlasSpriteSheetData*)calloc(1, sizeof(AtlasSpriteSheetData));
    if (data) {
        data->cell_pixel_dim = 20; // Default from engine schema
    }
    return data;
}

void free_atlas_sprite_sheet_data(AtlasSpriteSheetData* data) {
    free(data);
}

const DirectionFrameData* atlas_get_direction_frames(const AtlasSpriteSheetData* atlas, const char* dir_str) {
    assert(atlas && dir_str);

    if (strcmp(dir_str, "up_idle") == 0)              return &atlas->up_idle;
    if (strcmp(dir_str, "down_idle") == 0)            return &atlas->down_idle;
    if (strcmp(dir_str, "right_idle") == 0)           return &atlas->right_idle;
    if (strcmp(dir_str, "left_idle") == 0)            return &atlas->left_idle;
    if (strcmp(dir_str, "up_right_idle") == 0)        return &atlas->up_right_idle;
    if (strcmp(dir_str, "down_right_idle") == 0)      return &atlas->down_right_idle;
    if (strcmp(dir_str, "up_left_idle") == 0)         return &atlas->up_left_idle;
    if (strcmp(dir_str, "down_left_idle") == 0)       return &atlas->down_left_idle;
    if (strcmp(dir_str, "default_idle") == 0)         return &atlas->default_idle;
    if (strcmp(dir_str, "up_walking") == 0)           return &atlas->up_walking;
    if (strcmp(dir_str, "down_walking") == 0)         return &atlas->down_walking;
    if (strcmp(dir_str, "right_walking") == 0)        return &atlas->right_walking;
    if (strcmp(dir_str, "left_walking") == 0)         return &atlas->left_walking;
    if (strcmp(dir_str, "up_right_walking") == 0)     return &atlas->up_right_walking;
    if (strcmp(dir_str, "down_right_walking") == 0)   return &atlas->down_right_walking;
    if (strcmp(dir_str, "up_left_walking") == 0)      return &atlas->up_left_walking;
    if (strcmp(dir_str, "down_left_walking") == 0)    return &atlas->down_left_walking;
    if (strcmp(dir_str, "none_idle") == 0)            return &atlas->none_idle;

    return NULL;
}

LedgerType ledger_type_from_string(const char* type_str) {
    assert(type_str);

    if (strcmp(type_str, "ERC20") == 0)      return LEDGER_TYPE_ERC20;
    if (strcmp(type_str, "ERC721") == 0)     return LEDGER_TYPE_ERC721;
    if (strcmp(type_str, "OFF_CHAIN") == 0)  return LEDGER_TYPE_OFF_CHAIN;

    return LEDGER_TYPE_OFF_CHAIN;
}

