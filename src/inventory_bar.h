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
 *   Touch / click a slot → opens the inventory modal (inventory_modal.h).
 *   Scrolling: swipe left/right or use PageLeft / PageRight buttons when
 *   the inventory is wider than the visible area.
 *
 * Call sequence (each frame):
 *   1. inventory_bar_update(dt);       — advance scroll animation
 *   2. inventory_bar_draw();           — render the bar (screen space)
 *   3. int idx = inventory_bar_get_tapped_slot(mx, my);
 *      if (idx >= 0) { ... open modal ... }
 */

#ifndef INVENTORY_BAR_H
#define INVENTORY_BAR_H

#include <stdbool.h>
#include <raylib.h>
#include "object_layers_management.h"

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
 * @brief Scroll the bar by a signed number of slots.
 *
 * Positive = scroll right (show later items); negative = left.
 * Clamped to valid range.
 *
 * @param delta Number of slots to scroll.
 */
void inventory_bar_scroll(int delta);

#endif /* INVENTORY_BAR_H */
