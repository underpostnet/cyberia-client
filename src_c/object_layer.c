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