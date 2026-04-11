/**
 * @file dialogue_bubble.h
 * @brief Screen-space dialogue bubble column on the left side.
 *
 * Scans all entities (BOTs and other PLAYERs) in the player's AOI each frame.
 * For each entity that has an active ObjectLayer whose itemId has dialogue
 * data available (via dialogue_data module), a 50×50 animated icon is
 * stacked in a vertical column at the top-left of the screen.
 *
 * Clicking an icon opens the dialogue modal for that entity+item pair and
 * sends "dialogue_start" to the server (granting damage immunity).
 *
 *   ┌──┐
 *   │🗨│  ← entity 1 (e.g. lain skin bot)
 *   └──┘
 *   ┌──┐
 *   │🗨│  ← entity 2 (e.g. eiri skin player)
 *   └──┘
 *    ...
 *
 * Requirements:
 *   - dialogue_data_init() must be called before use.
 *   - dialogue_data_poll() must be called each frame (in render.c).
 *   - An ObjectLayersManager* is needed for the animated icon draw.
 */

#ifndef DIALOGUE_BUBBLE_H
#define DIALOGUE_BUBBLE_H

#include <stdbool.h>
#include "object_layers_management.h"

/** Fixed icon size in screen pixels. */
#define DBUBBLE_ICON_SIZE   50

/** Vertical gap between stacked icons. */
#define DBUBBLE_GAP         6

/** Left margin from screen edge. */
#define DBUBBLE_MARGIN_X    8

/** Top margin from screen edge. */
#define DBUBBLE_MARGIN_Y    8

/** Maximum number of visible bubble slots at once. */
#define DBUBBLE_MAX_SLOTS   16

/** Minimum time (seconds) a bubble stays visible once it appears. */
#define DBUBBLE_MIN_DISPLAY_SEC  3.0

/**
 * @brief One visible bubble slot — tracks which entity+item it represents.
 */
typedef struct {
    char entity_id[64];
    char item_id[128];
    bool active;     /* entity still present in AOI this frame */
    double appeared_at;  /* GetTime() when slot first appeared */
} DialogueBubbleSlot;

/**
 * @brief Initialise the bubble column system.
 * @param ol_manager Shared ObjectLayersManager for icon rendering.
 */
void dialogue_bubble_init(ObjectLayersManager* ol_manager);

/**
 * @brief Rebuild the bubble list from current AOI entities.
 *
 * Scans g_game_state for other_players and bots, checks each active
 * ObjectLayer against dialogue_data_available(), and populates the slot
 * array.  Also kicks off dialogue_data_request() for any new item IDs
 * encountered that haven't been fetched yet.
 *
 * Call once per frame before dialogue_bubble_draw().
 */
void dialogue_bubble_update(void);

/**
 * @brief Draw the bubble icon column in screen space.
 *
 * Must be called OUTSIDE BeginMode2D (screen-space UI pass).
 */
void dialogue_bubble_draw(void);

/**
 * @brief Process a click and check if it hit a bubble icon.
 *
 * @param mx      Screen X.
 * @param my      Screen Y.
 * @param clicked True on the frame a tap/click was released.
 * @return        True if the click was consumed (hit a bubble).
 */
bool dialogue_bubble_handle_click(int mx, int my, bool clicked);

/**
 * @brief Get the current slot count (for UI layout awareness).
 */
int dialogue_bubble_slot_count(void);

#endif /* DIALOGUE_BUBBLE_H */

