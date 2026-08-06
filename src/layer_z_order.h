#ifndef LAYER_Z_ORDER_H
#define LAYER_Z_ORDER_H

#include "object_layer.h"
#include "object_layers_management.h"

/* One render priority for the 'weapon' and 'skin' object-layer types, so
 * every context (grid entities, interaction bubbles, overlay previews) draws
 * them in the same order.
 *
 *   default:    skin 10 → weapon 40 → unknown 50
 *   facing up:  weapon 10 → skin 40 → unknown 50   (the skin occludes) */

/* Lower draws first, behind. Unknown and NULL types give 50. */
int layer_z_priority(const char* type, bool facing_up);

/* Keeps the original array index, so callers reorder their layer array
 * without copying whole structs. */
typedef struct {
    int index;
    int priority;
} LayerZEntry;

/* Fill `out` with the active layers of `layers`, sorted by ascending
 * priority. Inactive and empty layers are skipped. Types come from the
 * ObjectLayer singleton. Returns the number of entries written. */
int layer_z_sort(const ObjectLayerState* layers, int count,
                 LayerZEntry* out, int out_cap,
                 bool facing_up);

#endif /* LAYER_Z_ORDER_H */
