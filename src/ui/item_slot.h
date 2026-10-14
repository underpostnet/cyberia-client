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

#include "world/object_layer.h"
#include "world/object_layers_management.h"

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

/* Quantity badge in the bottom-right corner of `r`, for more than one unit. */
void item_slot_draw_quantity(Rectangle r, int quantity);

/* An open slot that holds nothing. */
void item_slot_draw_empty(Rectangle r);

/* A slot the player cannot use yet: the lock icon, and the level that opens
 * it under the lock. `unlock_level` 0 shows the lock alone. */
void item_slot_draw_locked(Rectangle r, int unlock_level);

#endif /* ITEM_SLOT_H */
