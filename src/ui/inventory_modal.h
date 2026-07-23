/**
 * @file inventory_modal.h
 * @brief Full-screen inventory item detail modal for the Cyberia client.
 *
 * Displays when the player taps a slot in the inventory bottom bar.
 * The modal is a semi-transparent overlay showing:
 *
 *   ┌─────────────────────────────────────────────────────────┐
 *   │  [X]                                         (close btn) │
 *   │                                                          │
 *   │         [item sprite — large render]                     │
 *   │                                                          │
 *   │   Item name / type                                       │
 *   │   Description text (data.item.description)              │
 *   │                                                          │
 *   │   Stats:                                                 │
 *   │     Effect      +12    Resistance  +8                    │
 *   │     Agility     +5     Range       +0                    │
 *   │     Intelligence+3     Utility     +2                    │
 *   │                                                          │
 *   │   Quantity: 45                    (if stackable)         │
 *   │                                                          │
 *   │          [ Activate ] / [ Deactivate ]                   │
 *   └─────────────────────────────────────────────────────────┘
 *
 * Interaction model:
 *   1. Caller opens the modal by providing the full_inventory index.
 *   2. User taps Activate / Deactivate → modal calls network_send() with
 *      a JSON "item_activation" message and closes itself.
 *   3. User taps X or outside the card → modal closes with no action.
 *   4. Server responds with the next AOI frame reflecting the change.
 *
 * The modal never mutates local game state — it sends an intent to the
 * server and the updated state arrives on the next AOI push.
 *
 * Call sequence (each frame while modal is open):
 *   inventory_modal_update(dt);    — advance animation
 *   inventory_modal_draw();        — render overlay
 *   inventory_modal_handle_click(mx, my);  — process input
 */

#ifndef INVENTORY_MODAL_H
#define INVENTORY_MODAL_H

#include "object_layer.h"
#include "object_layers_management.h"

#include <raylib.h>
#include <stdbool.h>

/* ── Public API ───────────────────────────────────────────────────────── */

/**
 * @brief Initialise the modal subsystem.
 *
 * @param ol_manager Pointer to the shared ObjectLayersManager.
 */
void inventory_modal_init(ObjectLayersManager* ol_manager);

/**
 * @brief Open the modal for a specific inventory slot.
 *
 * @param inv_idx  Index into g_game_state.full_inventory.
 */
void inventory_modal_open(int inv_idx);

/* Switch an already-open modal to a different inventory slot (the inventory
 * bar remains live under the modal). Replays the pop-in transition and closes
 * any opener chain so the modal becomes a standalone view of the new slot. */
void inventory_modal_switch_slot(int inv_idx);

/**
 * @brief Open a read-only inspection view for an item (another entity's or
 *        the player's own layer seen from the interaction modal). No
 *        activate/lore controls; no freeze.
 */
void inventory_modal_open_external(const ObjectLayerState* ols);

/* One-shot callback fired when the modal closes — lets the opener restore
 * its own context (e.g. the interaction modal reopening itself). Cleared
 * after it fires. */
typedef void (*InventoryModalOnClose)(void);
void inventory_modal_set_on_close(InventoryModalOnClose cb);

/**
 * @brief Close (hide) the modal without any action.
 */
void inventory_modal_close(void);

/**
 * @brief Returns true when the modal overlay is visible.
 */
bool inventory_modal_is_open(void);

/**
 * @brief Advance fade / pop animation.
 * Call once per frame before inventory_modal_draw().
 *
 * @param dt Delta-time in seconds.
 */
void inventory_modal_update(float dt);

/**
 * @brief Draw the modal overlay in screen space.
 * Must be called OUTSIDE BeginMode2D.
 */
void inventory_modal_draw(void);

bool inventory_modal_handle_click(int mx, int my);
bool inventory_modal_handle_wheel(float wheel_delta);

#endif /* INVENTORY_MODAL_H */
