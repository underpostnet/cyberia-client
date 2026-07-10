#ifndef CYBERIA_UI_FX_INVENTORY_BAR_QTY_H
#define CYBERIA_UI_FX_INVENTORY_BAR_QTY_H

#include <raylib.h>
#include <stdbool.h>

/* Inventory-bar quantity transition FX.
 *
 * Watches every inventory item's quantity and, on a change, plays a transition
 * for that slot: a floating green "+N" (gained) or red "-N" (lost) popup with a
 * bold black border (the sum-of-stats capability-value style), and a delayed,
 * gradual count of the badge number toward its new value.
 *
 * Presentation-only, driven off g_game_state.full_inventory. Contract:
 *   1. fx_inventory_bar_qty_update(dt) once per frame (detects changes, advances
 *      the count tween and popups);
 *   2. the inventory bar draws each slot's badge with the tweened value from
 *      fx_inventory_bar_qty_display(item_id, actual);
 *   3. and calls fx_inventory_bar_qty_draw(slot_rect, item_id) to layer the
 *      +/- popup above that slot. */

void fx_inventory_bar_qty_init(void);
void fx_inventory_bar_qty_reset(void);
void fx_inventory_bar_qty_update(float dt);

/* Release a held +/- popup the moment the loot pickup particle reaches this
 * item's slot (called by loot_fx when its slot-delivery lands). Changes with no
 * pickup animation release on a short fallback timer instead. */
void fx_inventory_bar_qty_notify_arrival(const char* item_id);

/* Gradually-animated badge value for an item (falls back to `actual`). */
int  fx_inventory_bar_qty_display(const char* item_id, int actual);

/* False only while a first-copy slot is held hidden awaiting its pickup
 * flight; the bar skips drawing/hit-testing the slot until it reveals. */
bool fx_inventory_bar_qty_slot_visible(const char* item_id);

/* Current pulse scale for a slot (1.0 at rest). Pulses on every released
 * quantity gain and on first-copy reveal, in sync with the +N popup. */
float fx_inventory_bar_qty_slot_scale(const char* item_id);

/* Draw the active +/- transition popup above a slot, if any. */
void fx_inventory_bar_qty_draw(Rectangle slot, const char* item_id);

/* Draw active transition popups around the inventory bar's visible bottom
 * anchor while its slots are hidden. */
void fx_inventory_bar_qty_draw_bottom(Rectangle anchor);

#endif /* CYBERIA_UI_FX_INVENTORY_BAR_QTY_H */
