/**
 * @file layer_z_order.h
 * @brief Centralized z-order controller for Object Layer rendering.
 *
 * Defines the canonical render priority for every OL type so that all
 * rendering contexts (grid entities, interaction bubbles, overlay previews)
 * draw layers in the same order: skin first, weapon on top.
 *
 * Priority values (lower = drawn first = behind):
 *   skin/body  10  →  eyes  11  →  hair  12  →  clothes/armor  20
 *   hat/helmet  30  →  weapon  40  →  shield  41  →  unknown  50
 */

#ifndef LAYER_Z_ORDER_H
#define LAYER_Z_ORDER_H

#include "object_layer.h"
#include "object_layers_management.h"

/**
 * @brief Return the render priority for an OL type string.
 *
 * Lower values are drawn first (behind higher values).
 * Returns 50 for unknown/NULL types.
 */
int layer_z_priority(const char* type);

/**
 * @brief Index + priority pair used by the sorting helpers.
 *
 * Stores the original array index so callers can reorder
 * their layer arrays without copying full structs.
 */
typedef struct {
    int index;
    int priority;
} LayerZEntry;

/**
 * @brief Build a z-sorted index array for an ObjectLayerState array.
 *
 * Looks up each active layer's type via the ObjectLayersManager
 * and fills @p out with (index, priority) pairs sorted by ascending
 * priority.  Inactive / empty layers are skipped.
 *
 * @param mgr       ObjectLayersManager for type lookups (may be NULL).
 * @param layers    Source ObjectLayerState array.
 * @param count     Number of entries in @p layers.
 * @param out       Caller-provided buffer for sorted entries.
 * @param out_cap   Capacity of @p out.
 * @return          Number of entries written to @p out.
 */
int layer_z_sort(ObjectLayersManager* mgr,
                 const ObjectLayerState* layers, int count,
                 LayerZEntry* out, int out_cap);

#endif /* LAYER_Z_ORDER_H */
