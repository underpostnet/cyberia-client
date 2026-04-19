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
 *
 * `alive_layers` holds the last-known alive OL stack so the bubble icon
 * always shows the living appearance (even when the entity is dead/ghost).
 * `layers` tracks whatever the server currently sends (dead items if ghost).
 */
typedef struct {
    char entity_id[MAX_ID_LENGTH];
    char display_name[MAX_ID_LENGTH];
    ObjectLayerState layers[IBUBBLE_MAX_LAYERS];
    int layer_count;
    ObjectLayerState alive_layers[IBUBBLE_MAX_LAYERS];
    int alive_layer_count;
    int direction;
    uint32_t interact_flags;
    char dialogue_item_id[128];
    uint8_t status_icon;
    bool is_player;
    bool active;
    double appeared_at;
    Color fallback_color; /* solid colour when no OLs — entity DB colour or palette default */
} InteractionBubbleSlot;

/* ── Public API ────────────────────────────────────────────────────────── */

void interaction_bubble_init(void);
void interaction_bubble_update(void);
void interaction_bubble_draw(void);
bool interaction_bubble_handle_click(int mx, int my, bool clicked);
int  interaction_bubble_slot_count(void);

/**
 * @brief Optimistically update the self-player bubble's alive-OL cache
 *        after an equip/unequip while dead.
 *
 * The server queues dead-equip in PreRespawnObjectLayers (never sent to
 * the client), so the bubble must apply the change locally to keep the
 * icon in sync with what will render on revive.
 */
void interaction_bubble_dead_equip(const char* item_id, bool active);

/**
 * @brief Look up the cached alive-OL stack for an entity.
 *
 * Returns the alive layer snapshot and count via out-params.
 * If the entity has no bubble slot or no alive cache, returns NULL
 * and sets *out_count to 0.
 */
const ObjectLayerState* interaction_bubble_get_alive_layers(
    const char *entity_id, int *out_count);

#endif /* INTERACTION_BUBBLE_H */
