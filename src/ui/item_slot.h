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
bool item_slot_hit(Rectangle r, int mx, int my);

#endif /* ITEM_SLOT_H */
