/**
 * @file interaction_bubble.h
 * @brief Entity-specific interaction bubbles rendered in screen space.
 *
 * Each interactable entity (NPC bot, other player) in the AOI gets its
 * own bubble whose icon is the full active ObjectLayer stack — exactly
 * how the entity appears in the world.
 *
 * Tapping a bubble opens the JS interact overlay (interact_overlay.js)
 * via the interact_bridge module.  The overlay provides tabs for Dialog,
 * Chat, and Actions interactions.
 *
 * Keyed by **entity ID** (not item ID) — one bubble per entity.
 */

#ifndef INTERACTION_BUBBLE_H
#define INTERACTION_BUBBLE_H

#include <stdbool.h>
#include "object_layer.h"
#include "game_state.h"

/* ── Layout constants ──────────────────────────────────────────────────── */

#define IBUBBLE_ICON_SIZE   56
#define IBUBBLE_GAP         6
#define IBUBBLE_MARGIN_X    8
#define IBUBBLE_MARGIN_Y    8
#define IBUBBLE_MAX_SLOTS   12
#define IBUBBLE_MIN_DISPLAY_SEC  3.0
#define IBUBBLE_MAX_LAYERS  MAX_OBJECT_LAYERS

/* ── Interaction type flags (bitmask) ──────────────────────────────────── */

#define INTERACT_DIALOGUE   (1 << 0)
#define INTERACT_SOCIAL     (1 << 1)
#define INTERACT_QUEST      (1 << 2)

/**
 * @brief One visible bubble slot — entity-specific, stores full OL snapshot.
 */
typedef struct {
    char entity_id[MAX_ID_LENGTH];
    char display_name[MAX_ID_LENGTH];
    ObjectLayerState layers[IBUBBLE_MAX_LAYERS];
    int layer_count;
    int direction;
    uint32_t interact_flags;
    char dialogue_item_id[128];
    uint8_t status_icon;
    bool active;
    double appeared_at;
} InteractionBubbleSlot;

/* ── Public API ────────────────────────────────────────────────────────── */

void interaction_bubble_init(void);
void interaction_bubble_update(void);
void interaction_bubble_draw(void);
bool interaction_bubble_handle_click(int mx, int my, bool clicked);
int  interaction_bubble_slot_count(void);

#endif /* INTERACTION_BUBBLE_H */
