/**
 * item_slot — reusable inventory item slot.
 *
 * One drawing + hit-test primitive for an ObjectLayer slot, shared by the
 * inventory bar and the interaction modal. Renders the item sprite, active
 * glow border, quantity badge, and a lock badge for non-activable items.
 * Inner padding and badge font scale with the slot size.
 */

#ifndef ITEM_SLOT_H
#define ITEM_SLOT_H

#include "object_layer.h"
#include "object_layers_management.h"

#include <raylib.h>
#include <stdbool.h>

void item_slot_draw(Rectangle r, const ObjectLayerState* ols, ObjectLayersManager* mgr);

/* Same as item_slot_draw, but blends the slot's background/border toward
 * `highlight` by `highlight_t` (0 = normal, 1 = fully highlight-colored). Lets
 * a caller flash/transition a slot's color (e.g. a freshly-arrived reward)
 * without any other item_slot_draw call site being affected. */
void item_slot_draw_ex(Rectangle r, const ObjectLayerState* ols, ObjectLayersManager* mgr,
                       Color highlight, float highlight_t);

bool item_slot_hit(Rectangle r, int mx, int my);

#endif /* ITEM_SLOT_H */
