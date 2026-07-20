/**
 * @file inventory_bar.h
 * @brief Horizontal inventory bottom bar for the Cyberia client.
 *
 * Renders a fixed-height scrollable/paginated strip of item slots at the
 * bottom of the screen, showing every ObjectLayer owned by the self-player
 * (active AND inactive).  Active slots glow with a colored border.
 * Stackable items display a quantity badge in the bottom-right corner.
 *
 * Design philosophy — clean separation of concerns:
 *   Server:  stores state (ObjectLayers + quantities), validates activation.
 *   Client:  pure display + intent; never mutates state locally.
 *
 * Slot layout (per cell):
 *   ┌──────────────────────────┐
 *   │  [item sprite / texture] │  ← atlas sprite (down_idle frame 0)
 *   │                          │
 *   │                    [qty] │  ← quantity badge, bottom-right (if qty > 1)
 *   └──────────────────────────┘
 *     active → bright colored border + glow
 *     inactive → dim grey border
 *
 * User interaction:
 *   Press and slide left/right (finger or mouse) scrolls the strip 1:1 and
 *   releases into an inertial glide. A press that never became a drag taps the
 *   slot under it, so scrolling never opens a modal.
 *
 * Call sequence (each frame):
 *   1. inventory_bar_update(dt);       — advance drag, glide and animations
 *   2. inventory_bar_draw();           — render the bar (screen space)
 *   3. inventory_bar_handle_click(mx, my, NULL);  — on press: arm the gesture
 *   4. int idx = -1;
 *      if (inventory_bar_take_tap(&idx) && idx >= 0) {
 *          ... open modal ...
 *      }
 */

#ifndef INVENTORY_BAR_H
#define INVENTORY_BAR_H

#include "object_layers_management.h"

#include <raylib.h>
#include <stdbool.h>

/* ── Layout constants (screen pixels) ────────────────────────────────── */

/** Height of the inventory bar in pixels. */
#define INV_BAR_HEIGHT      72

/** Square slot size (width = height) in pixels. */
#define INV_SLOT_SIZE       60

/** Horizontal gap between slots in pixels. */
#define INV_SLOT_GAP        6

/** Vertical padding above/below sprite inside each slot. */
#define INV_SLOT_PADDING    4

/** Border thickness for the active-item glow in pixels. */
#define INV_ACTIVE_BORDER   3

/** Alpha of the bar backing rectangle (0–255). */
#define INV_BAR_ALPHA       210

/** Font size for the quantity badge label. */
#define INV_QTY_FONT_SIZE   10

/* ── Public API ───────────────────────────────────────────────────────── */

/**
 * @brief Initialise the inventory bar.
 *
 * Called once after the ObjectLayersManager is available.
 * Safe to call multiple times (idempotent).
 *
 * @param ol_manager Pointer to the shared ObjectLayersManager for texture
 *                   and metadata lookups.  Must remain valid for the bar's
 *                   lifetime.
 */
void inventory_bar_init(ObjectLayersManager* ol_manager);

/**
 * @brief Advance scroll / page animation.
 *
 * Call once per frame before inventory_bar_draw().
 *
 * @param dt Delta-time in seconds since last frame.
 */
void inventory_bar_update(float dt);

/**
 * @brief Draw the inventory bar in screen space.
 *
 * Must be called OUTSIDE BeginMode2D (in screen / UI space).
 * Reads the current slot list from g_game_state.full_inventory.
 */
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

/**
 * @brief Test whether a screen-space point lands inside a slot.
 *
 * Returns the full_inventory index of the hit slot, or -1 for miss.
 * Use the returned index to open the inventory modal.
 *
 * @param mx  Screen X in pixels.
 * @param my  Screen Y in pixels.
 * @return    full_inventory index (0-based), or -1.
 */
int inventory_bar_get_tapped_slot(int mx, int my);

/**
 * @brief Get the screen-space center of the slot holding a given item.
 *
 * Used by loot_fx to position the delivery particle stream endpoint.
 *
 * @param item_id  The item identifier to look up.
 * @param out      Receives the screen-space center (pixels).
 * @return         true if the slot was found, false otherwise.
 */
bool inventory_bar_item_slot_center(const char* item_id, Vector2* out);

#endif /* INVENTORY_BAR_H */
