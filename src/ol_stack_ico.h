/**
 * @file ol_stack_ico.h
 * @brief Centralized OL-stack-as-icon renderer for UX/UI outside the grid.
 *
 * Renders a full active ObjectLayer stack as composited animated icons —
 * each layer drawn on top of the previous one, producing the same visual
 * as the entity's in-world appearance but at arbitrary screen position/size.
 *
 * This is the single entry point for all "OL as icon" usage:
 *   - Interaction bubbles (screen-space entity list)
 *   - Inventory modal previews
 *   - Overlay entity preview panels
 *   - Any future UI that needs to show an entity's OL stack outside the grid
 *
 * Dependencies: ol_as_animated_ico.h (per-item icon), object_layers_management.h
 */

#ifndef OL_STACK_ICO_H
#define OL_STACK_ICO_H

#include "object_layers_management.h"
#include "object_layer.h"
#include <raylib.h>

/**
 * @brief Draw a stack of active ObjectLayer icons composited at (x,y).
 *
 * Iterates the layers array, skips inactive/empty entries, and draws each
 * remaining layer via ol_as_ico_draw at the same position so the result
 * matches the entity's in-world appearance.
 *
 * @param mgr       ObjectLayersManager for atlas + texture lookup.
 * @param layers    Array of ObjectLayerState (may contain inactive entries).
 * @param count     Number of entries in the layers array.
 * @param x         Left edge in screen pixels.
 * @param y         Top edge in screen pixels.
 * @param icon_size Width = Height of the drawn square in pixels.
 * @param dir_str   Atlas direction string (e.g. "down_idle"); NULL → default.
 * @param frame_ms  Milliseconds per animation frame; 0 → default (100 ms).
 * @param tint      Colour tint applied to every layer (WHITE = no tint).
 */
void ol_stack_ico_draw(ObjectLayersManager* mgr,
                       const ObjectLayerState* layers, int count,
                       int x, int y, int icon_size,
                       const char* dir_str,
                       int frame_ms,
                       Color tint);

#endif /* OL_STACK_ICO_H */
