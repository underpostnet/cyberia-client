/**
 * @file ol_stack_ico.c
 * @brief Centralized OL-stack-as-icon renderer implementation.
 *
 * Draws active layers sorted by the canonical z-order (skin first,
 * weapon on top) via layer_z_sort(), then composites each layer
 * with ol_as_ico_draw at the same (x,y).
 */

#include "ol_stack_ico.h"
#include "ol_as_animated_ico.h"
#include "layer_z_order.h"

#define OL_STACK_ICO_MAX_LAYERS 32

void ol_stack_ico_draw(ObjectLayersManager* mgr,
                       const ObjectLayerState* layers, int count,
                       int x, int y, int icon_size,
                       const char* dir_str,
                       int frame_ms,
                       Color tint) {
    if (!mgr || !layers || count <= 0) return;

    LayerZEntry sorted[OL_STACK_ICO_MAX_LAYERS];
    int n = layer_z_sort(mgr, layers, count, sorted, OL_STACK_ICO_MAX_LAYERS);

    for (int i = 0; i < n; i++) {
        const ObjectLayerState* s = &layers[sorted[i].index];
        ol_as_ico_draw(mgr, s->item_id, x, y, icon_size,
                       dir_str, frame_ms, tint);
    }
}
