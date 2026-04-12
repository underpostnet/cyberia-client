/**
 * @file ol_stack_ico.c
 * @brief Centralized OL-stack-as-icon renderer implementation.
 *
 * Single loop: filter active layers, draw each via ol_as_ico_draw
 * at the same (x,y) so they composite into the entity appearance.
 */

#include "ol_stack_ico.h"
#include "ol_as_animated_ico.h"

void ol_stack_ico_draw(ObjectLayersManager* mgr,
                       const ObjectLayerState* layers, int count,
                       int x, int y, int icon_size,
                       const char* dir_str,
                       int frame_ms,
                       Color tint) {
    if (!mgr || !layers || count <= 0) return;

    for (int i = 0; i < count; i++) {
        if (!layers[i].active || layers[i].item_id[0] == '\0') continue;
        ol_as_ico_draw(mgr, layers[i].item_id, x, y, icon_size,
                       dir_str, frame_ms, tint);
    }
}
