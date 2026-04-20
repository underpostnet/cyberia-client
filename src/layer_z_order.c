/**
 * @file layer_z_order.c
 * @brief Centralized z-order controller implementation.
 */

#include "layer_z_order.h"
#include <stdlib.h>
#include <string.h>

int layer_z_priority(const char* type, bool facing_up) {
    if (!type || type[0] == '\0') return 50;
    if (strcmp(type, "skin") == 0)   return facing_up ? 40 : 10;
    if (strcmp(type, "weapon") == 0) return facing_up ? 10 : 40;
    return 50;
}

static int cmp_z_entry(const void* a, const void* b) {
    const LayerZEntry* ea = (const LayerZEntry*)a;
    const LayerZEntry* eb = (const LayerZEntry*)b;
    return ea->priority - eb->priority;
}

int layer_z_sort(ObjectLayersManager* mgr,
                 const ObjectLayerState* layers, int count,
                 LayerZEntry* out, int out_cap,
                 bool facing_up) {
    if (!layers || count <= 0 || !out || out_cap <= 0) return 0;

    int n = 0;
    for (int i = 0; i < count && n < out_cap; i++) {
        if (!layers[i].active || layers[i].item_id[0] == '\0') continue;

        const char* type = NULL;
        if (mgr) {
            ObjectLayer* ol = get_or_fetch_object_layer(mgr, layers[i].item_id);
            if (ol && ol->data.item.type[0] != '\0')
                type = ol->data.item.type;
        }

        out[n].index    = i;
        out[n].priority = layer_z_priority(type, facing_up);
        n++;
    }

    if (n > 1)
        qsort(out, (size_t)n, sizeof(LayerZEntry), cmp_z_entry);

    return n;
}
