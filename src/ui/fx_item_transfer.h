#ifndef CYBERIA_UI_FX_ITEM_TRANSFER_H
#define CYBERIA_UI_FX_ITEM_TRANSFER_H

#include "object_layer.h"
#include "object_layers_management.h"

#include <raylib.h>

/* Slot-to-slot transfer flight.
 *
 * When a stack crosses between two containers the player can see at once — the
 * storage vault and the inventory bar — neither the gain nor the loss FX apply:
 * nothing was earned or destroyed, it moved. This flies the item's slot from
 * where it left to where it lands so the move is legible.
 *
 * Drawn in screen space above every surface, so the host calls it last in the
 * frame, after the inventory bar.
 *
 * Contract per frame: fx_item_transfer_update(dt) once, then
 * fx_item_transfer_draw(mgr) after the topmost UI. */

void fx_item_transfer_reset(void);

/* Fly `ols` from `from` to `to`, both screen-space slot rects. */
void fx_item_transfer_spawn(const ObjectLayerState* ols, Rectangle from, Rectangle to);

/* Fly into inventory and reveal its target slot when the flight lands. */
void fx_item_transfer_spawn_to_inventory(const ObjectLayerState* ols,
										 Rectangle from, Rectangle to);

void fx_item_transfer_update(float dt);
void fx_item_transfer_draw(ObjectLayersManager* mgr);

#endif /* CYBERIA_UI_FX_ITEM_TRANSFER_H */
