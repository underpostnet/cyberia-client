#ifndef INVENTORY_BAR_H
#define INVENTORY_BAR_H

#include "object_layers_management.h"

#include <raylib.h>
#include <stdbool.h>

/* Inventory bottom bar — a scrollable strip of item slots holding every
 * ObjectLayer the self-player owns, active and inactive. An active slot gets
 * a bright border; a stack shows its quantity badge.
 *
 * Display and intent only: the bar sends the tap to the server and never
 * changes state locally.
 *
 * A press-and-slide scrolls the strip 1:1 and releases into an inertial
 * glide. A press that never became a drag taps the slot below it, so
 * scrolling never opens a modal. */

/* ── Layout constants (screen pixels) ────────────────────────────────── */

#define INV_BAR_HEIGHT      72
#define INV_SLOT_SIZE       60      /* square */
#define INV_SLOT_GAP        6
#define INV_SLOT_PADDING    4       /* around the sprite, inside the slot */
#define INV_ACTIVE_BORDER   3
#define INV_BAR_ALPHA       210     /* backing rectangle, 0-255 */
#define INV_QTY_FONT_SIZE   10

/* ── Public API ───────────────────────────────────────────────────────── */

/* Call once the ObjectLayersManager exists. Idempotent. The manager must
 * stay valid for the lifetime of the bar. */
void inventory_bar_init(ObjectLayersManager* ol_manager);

/* Advance drag, glide, and animation. Call once per frame before the draw. */
void inventory_bar_update(float dt);

/* Draw in screen space — outside BeginMode2D. Reads the slot list from
 * g_game_state.full_inventory. */
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
 * consumed it. A press on the strip arms the horizontal drag, so `out_slot` is
 * always -1 — slot activation arrives later through inventory_bar_take_tap. */
bool inventory_bar_handle_click(int mx, int my, int* out_slot);

/* One-shot: the deferred slot activation from a clean (drag-less) release.
 * `out_slot` receives a full_inventory index, or -1 when the tap hit no slot. */
bool inventory_bar_take_tap(int* out_slot);

/* True for the visible bar or its persistent bottom toggle. */
bool inventory_bar_point_covered(int mx, int my);

/* full_inventory index of the slot below the screen point, or -1 on a miss. */
int inventory_bar_get_tapped_slot(int mx, int my);

/* Slot rectangle where an item would land after an inventory gain. */
bool inventory_bar_predicted_item_slot_rect(const char* item_id, Rectangle* out);

/* Screen-space centre of the slot holding `item_id` — loot_fx aims its
 * delivery stream at it. False when the item has no slot. */
bool inventory_bar_item_slot_center(const char* item_id, Vector2* out);

#endif /* INVENTORY_BAR_H */
