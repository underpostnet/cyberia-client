#ifndef INVENTORY_MODAL_H
#define INVENTORY_MODAL_H

#include "object_layer.h"
#include "object_layers_management.h"

#include <raylib.h>
#include <stdbool.h>

/* Inventory item detail modal, opened from a slot in the bottom bar. Shows
 * the item sprite, name, description, stats, quantity, and an
 * Activate/Deactivate button.
 *
 * Intent only: Activate sends an "item_activation" message and closes. The
 * modal never changes local state — the next AOI push carries the result. */

/* The manager must stay valid for the lifetime of the modal. */
void inventory_modal_init(ObjectLayersManager* ol_manager);

/* `inv_idx` indexes g_game_state.full_inventory. */
void inventory_modal_open(int inv_idx);

/* Switch an already-open modal to a different inventory slot (the inventory
 * bar remains live under the modal). Replays the pop-in transition and closes
 * any opener chain so the modal becomes a standalone view of the new slot. */
void inventory_modal_switch_slot(int inv_idx);

/* Read-only inspection of an item the player does not hold (another entity's
 * layer, a shop row, an assembler recipe line). No activate or lore control,
 * but it takes the same modal freeze as any other open — the player is reading
 * a panel with the world still running. */
void inventory_modal_open_external(const ObjectLayerState* ols);

/* Entity the card is anchored over on large screens (see modal_anchor.h).
 * NULL or an empty id anchors over the local player, which is the default
 * every open resets to — set it right after opening. */
void inventory_modal_set_anchor_entity(const char* entity_id);

/* One-shot callback fired when the modal closes — lets the opener restore
 * its own context (e.g. the interaction modal reopening itself). Cleared
 * after it fires. */
typedef void (*InventoryModalOnClose)(void);
void inventory_modal_set_on_close(InventoryModalOnClose cb);

/* Close with no action. */
void inventory_modal_close(void);

bool inventory_modal_is_open(void);

/* Advance the fade and pop animation. Call once per frame before the draw. */
void inventory_modal_update(float dt);

/* Draw in screen space — outside BeginMode2D. */
void inventory_modal_draw(void);

bool inventory_modal_handle_click(int mx, int my);
bool inventory_modal_handle_wheel(float wheel_delta);

#endif /* INVENTORY_MODAL_H */
