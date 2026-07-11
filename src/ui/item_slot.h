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
 * `highlight` by `highlight_t` (0 = normal, 1 = fully highlight-colored), and
 * `sprite_full_color` renders the item sprite untinted (full colour) instead
 * of the muted inactive tone — the sprite only, never the frame/background
 * (used while a slot's arrival pulse plays). */
void item_slot_draw_ex(Rectangle r, const ObjectLayerState* ols, ObjectLayersManager* mgr,
                       Color highlight, float highlight_t, bool sprite_full_color);

bool item_slot_hit(Rectangle r, int mx, int my);

#endif /* ITEM_SLOT_H */
