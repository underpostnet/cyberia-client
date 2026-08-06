#ifndef OL_STACK_ICO_H
#define OL_STACK_ICO_H

#include "object_layers_management.h"
#include "object_layer.h"
#include <raylib.h>

/* Draws a full active ObjectLayer stack as one composited animated icon, so
 * UI outside the grid (bubbles, modal previews, overlay panels) shows an
 * entity exactly as it looks in the world. The single entry point for
 * "ObjectLayer stack as icon". */

/* Draw every active layer at (x, y), icon_size square, in screen pixels.
 * Inactive and empty entries are skipped. `dir_str` NULL and frame_ms 0 take
 * the ol_as_animated_ico defaults; WHITE means no tint. */
void ol_stack_ico_draw(ObjectLayersManager* mgr,
                       const ObjectLayerState* layers, int count,
                       int x, int y, int icon_size,
                       const char* dir_str,
                       int frame_ms,
                       Color tint);

#endif /* OL_STACK_ICO_H */
