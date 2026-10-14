#ifndef CYBERIA_FX_FX_INVENTORY_BAR_QTY_H
#define CYBERIA_FX_FX_INVENTORY_BAR_QTY_H

#include <raylib.h>
#include <stdbool.h>

/* Inventory-bar quantity transition FX.
 *
 * Watches every inventory item's quantity and, on a change, plays a transition
 * for that slot: a floating green "+N" (gained) or red "-N" (lost) popup with a
 * bold black border (the sum-of-stats capability-value style). The badge holds
 * the pre-change count until the delivery lands, then snaps to the new total —
 * the arriving item and the number it produces are one event.
 *
 * Presentation-only, driven off g_local_player.inventory. Contract:
 *   1. fx_inventory_bar_qty_update(dt) once per frame (detects changes, advances
 *      the popups);
 *   2. the inventory bar draws each stack's badge with the held value from
 *      fx_inventory_bar_qty_display(item_id, slot, actual);
 *   3. and calls fx_inventory_bar_qty_draw(rect, item_id, slot) to layer the
 *      +/- popup above that stack.
 *
 * The effect watches the total of all stacks of an item and plays on the one
 * stack that changed: `slot` is the bag slot of a stack, the wallet for the
 * coin. */

void fx_inventory_bar_qty_init(void);
void fx_inventory_bar_qty_update(float dt);

/* Release a held +/- popup the moment the loot pickup particle reaches this
 * item's slot (called by loot_fx when its slot-delivery lands). Changes with no
 * pickup animation release on a short fallback timer instead. */
void fx_inventory_bar_qty_notify_arrival(const char* item_id);

/* Badge value for the stack of an item in `slot`: its pre-change count while a
 * change to it is held for its delivery, `actual` otherwise. */
int  fx_inventory_bar_qty_display(const char* item_id, int slot, int actual);

/* False only while a first-copy slot is held hidden awaiting its pickup
 * flight; the bar skips drawing/hit-testing the slot until it reveals. */
bool fx_inventory_bar_qty_slot_visible(const char* item_id);

/* Current pulse scale for the stack in `slot` (1.0 at rest). Pulses on every
 * released quantity gain and on first-copy reveal, in sync with the +N popup. */
float fx_inventory_bar_qty_slot_scale(const char* item_id, int slot);

/* True while the stack's arrival pulse is playing — the bar renders the item
 * sprite in full colour for its duration. */
bool fx_inventory_bar_qty_slot_pulsing(const char* item_id, int slot);

/* Apply this item's next quantity change with no popup, pulse or spray — the
 * badge simply lands on the new total. Used when the item is moving between two
 * surfaces the player can see (inventory bar ↔ storage vault), where a loss FX
 * would read as destruction rather than a transfer. */
void fx_inventory_bar_qty_suppress(const char* item_id);

/* Hold this item's next quantity change (and, for a first copy, the slot
 * itself) until a delivery lands (notify_arrival). The holder must call this
 * every frame while waiting — e.g. the notification modal with a pending
 * reward — so the hold outlives the ordinary release fallback. */
void fx_inventory_bar_qty_hold_for_delivery(const char* item_id);

/* Draw the active +/- transition popup above the stack in `slot`, if any. */
void fx_inventory_bar_qty_draw(Rectangle rect, const char* item_id, int slot);

/* Draw active transition popups around the inventory bar's visible bottom
 * anchor while its slots are hidden. */
void fx_inventory_bar_qty_draw_bottom(Rectangle anchor);

#endif /* CYBERIA_FX_FX_INVENTORY_BAR_QTY_H */
