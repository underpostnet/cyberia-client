#ifndef INVENTORY_BAR_H
#define INVENTORY_BAR_H

#include "world/object_layers_management.h"

#include <raylib.h>
#include <stdbool.h>

/* Inventory bottom bar — a scrollable strip of item slots holding every
 * ObjectLayer the self-player owns, active and inactive. An active slot gets
 * a bright border; a stack shows its quantity badge.
 *
 * Display and intent only: the bar sends the tap to the server and never
 * changes state locally.
 *
 * Each strip cell is a bag slot of the server, so a stack stays where it was
 * dropped. A drag from a stack moves it to another cell, or to a cell of the
 * open container, and it slides into its cell. A drag elsewhere on the strip,
 * or the wheel, scrolls it. A press that never travelled taps the slot below
 * it, so a drag never opens a modal. */

/* The layout metrics live in inventory_bar.c — only the bar itself lays them
 * out. Ask `inventory_bar_visible_height` for the height on screen. */

/* ── Public API ───────────────────────────────────────────────────────── */

/* Call once the ObjectLayersManager exists. Idempotent. The manager must
 * stay valid for the lifetime of the bar. */
void inventory_bar_init(ObjectLayersManager* ol_manager);

/* Advance drag, glide, and animation. Call once per frame before the draw. */
void inventory_bar_update(float dt);

/* Draw in screen space — outside BeginMode2D. Reads the slot list from
 * g_local_player.inventory. */
void inventory_bar_draw(void);

/* Current on-screen height while the bar slides between shown and hidden. */
float inventory_bar_visible_height(void);

/* Full height of the current responsive bar when expanded. */
float inventory_bar_full_height(void);

/* Bounds of the persistent bottom toggle, used by neighboring UI to avoid
 * overlapping it while the bar is shown or hidden. */
Rectangle inventory_bar_toggle_bounds(void);

/* Toggle-only input for modal overlays that expose the bar as a read-only
 * companion surface. */
bool inventory_bar_handle_toggle_click(int mx, int my);

/* Handle a press on the bar or its persistent toggle. Returns true when the UI
 * consumed it. A press on a stack arms its drag, and anywhere else on the
 * strip the scroll, so `out_slot` is always -1 — slot activation arrives later
 * through inventory_bar_take_tap. */
bool inventory_bar_handle_click(int mx, int my, int* out_slot);

/* Scroll the strip by the wheel while the pointer is over the bar. True when
 * the bar took the wheel. */
bool inventory_bar_handle_wheel(float wheel_delta);

/* One-shot: the deferred slot activation from a clean (drag-less) release.
 * `out_slot` receives a full_inventory index, or -1 when the tap hit no slot. */
bool inventory_bar_take_tap(int* out_slot);

/* True for the visible bar or its persistent bottom toggle. */
bool inventory_bar_point_covered(int mx, int my);

/* full_inventory index of the slot below the screen point, or -1 on a miss. */
int inventory_bar_get_tapped_slot(int mx, int my);

/* The bag slot of the bar cell under the screen point: a strip cell, or the
 * wallet for the coin slot. False off the settled bar. */
bool inventory_bar_cell_at(int mx, int my, int* slot);

/* The screen rect of the bar cell of a bag slot, the coin slot for the wallet. */
Rectangle inventory_bar_cell_rect(int slot);

/* Screen-space centre of the bar cell of a bag slot; the bag toggle while the
 * bar is hidden. */
Vector2 inventory_bar_cell_center(int slot);

/* Slide the stack of bag slot `slot` in from `origin`, a screen rect. */
void inventory_bar_animate_from(int slot, Rectangle origin);

/* Slot rectangle where an item would land after an inventory gain. */
bool inventory_bar_predicted_item_slot_rect(const char* item_id, Rectangle* out);

/* Screen-space centre of the slot holding `item_id` — loot_fx aims its
 * delivery stream at it. False when the item has no slot. */
bool inventory_bar_item_slot_center(const char* item_id, Vector2* out);

#endif /* INVENTORY_BAR_H */
