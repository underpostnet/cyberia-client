#include "object_layer.h"
#include <stdlib.h>
#include <string.h>

ObjectLayer* create_object_layer(void) {
    ObjectLayer* layer = (ObjectLayer*)malloc(sizeof(ObjectLayer));
    if (layer) {
        // Initialize with zeros
        memset(layer, 0, sizeof(ObjectLayer));
        
        // Set default values for stats
        layer->data.stats.effect = 0;
        layer->data.stats.resistance = 0;
        layer->data.stats.agility = 0;
        layer->data.stats.range = 0;
        layer->data.stats.intelligence = 0;
        layer->data.stats.utility = 0;
        
        // Set default values for render
        layer->data.render.frame_duration = 100;
        layer->data.render.is_stateless = false;
        
        // Set default values for item (empty strings)
        layer->data.item.id[0] = '\0';
        layer->data.item.type[0] = '\0';
        layer->data.item.description[0] = '\0';
        layer->data.item.activable = false;
        
        // Initialize sha256 to empty string
        layer->sha256[0] = '\0';
    }
    return layer;
}

void free_object_layer(ObjectLayer* layer) {
    if (layer) {
        // No dynamic allocations to free since we use fixed-size arrays
        free(layer);
    }
}

ObjectLayerState* create_object_layer_state(void) {
    ObjectLayerState* state = (ObjectLayerState*)malloc(sizeof(ObjectLayerState));
    if (state) {
        memset(state, 0, sizeof(ObjectLayerState));
        state->item_id[0] = '\0';
        state->active = false;
        state->quantity = 1; // Default quantity
    }
    return state;
}

void free_object_layer_state(ObjectLayerState* state) {
    if (state) {
        // No dynamic allocations to free since we use fixed-size arrays
        free(state);
    }
}

AtlasSpriteSheetData* create_atlas_sprite_sheet_data(void) {
    AtlasSpriteSheetData* data = (AtlasSpriteSheetData*)malloc(sizeof(AtlasSpriteSheetData));
    if (data) {
        memset(data, 0, sizeof(AtlasSpriteSheetData));
        data->item_key[0] = '\0';
        data->file_id[0] = '\0';
        data->atlas_width = 0;
        data->atlas_height = 0;
        data->cell_pixel_dim = 20; // Default from engine schema
    }
    return data;
}

void free_atlas_sprite_sheet_data(AtlasSpriteSheetData* data) {
    if (data) {
        // No dynamic allocations to free since we use fixed-size arrays
        free(data);
    }
}

const DirectionFrameData* atlas_get_direction_frames(
    const AtlasSpriteSheetData* atlas,
    const char* dir_str
) {
    if (!atlas || !dir_str) return NULL;

    if (strcmp(dir_str, "up_idle") == 0)             return &atlas->up_idle;
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