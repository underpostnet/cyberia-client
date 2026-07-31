/**
 * sum_stat — shared "Total Stats" summary: computes and draws the signed sum
 * of Effect/Resistance/Agility/Range/Intelligence/Utility across one or more
 * ObjectLayer entries.
 *
 * Used by the interact modal's Stats tab (a stack of active layers) and the
 * inventory modal (a single item, array[0]) so both share one computation and
 * one container/style instead of duplicating the header draw.
 */

#ifndef UI_SUM_STAT_H
#define UI_SUM_STAT_H

#include "object_layer.h"
#include "object_layers_management.h"

#include <raylib.h>

/* Height of the header container drawn by sum_stat_draw. */
#define SUM_STAT_HEADER_H 56.0f

/* Sums Stats across `layers[0..count)`, resolved via `mgr`. A single item
 * passes count=1 (array[0]); an active-layer stack passes its full count.
 * Entries with no cached ObjectLayer contribute zero. */
Stats sum_stat_compute(const ObjectLayerState* layers, int count, ObjectLayersManager* mgr);

/* Draws the shared "Total Stats" header (icon + signed sum + label) at
 * (x, y, width, SUM_STAT_HEADER_H), inset by `pad`. Returns the height
 * consumed so callers advance their cursor by the result. */
float sum_stat_draw(float x, float y, float width, float pad, int sum);

#endif /* UI_SUM_STAT_H */
