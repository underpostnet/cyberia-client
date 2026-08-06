#ifndef INTERACTION_BUBBLE_H
#define INTERACTION_BUBBLE_H

#include "game_state.h"
#include "object_layer.h"

#include <stdbool.h>

/* Screen-space bubble column, one bubble per interactable entity in the AOI
 * (NPC bot or other player), keyed by entity ID. Each bubble icon is the
 * entity's full active ObjectLayer stack, so it matches how the entity looks
 * in the world. A tap opens the JS interact overlay through interact_bridge,
 * which carries the Dialog, Chat, and Actions tabs. */

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

/* One bubble slot. `alive_layers` keeps the last known alive stack, so the
 * icon always shows the living appearance even for a dead or ghost entity;
 * `layers` tracks whatever the server currently sends. */
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
    uint8_t status_icon;        /* presence lifecycle icon */
    uint8_t interaction_flags;  /* INTERACTION_FLAG_* capability bits */
    bool is_player;
    bool active;
    double appeared_at;
    Color fallback_color; /* solid colour when no OLs — entity DB colour or palette default */
} InteractionBubbleSlot;

/* ── Public API ────────────────────────────────────────────────────────── */

void interaction_bubble_init(void);
void interaction_bubble_update(void);
void interaction_bubble_draw(void);
bool interaction_bubble_handle_click(int mx, int my);

/* Collapsed-column state + programmatic expand (e.g. a world tap landing on
 * a quest/action provider auto-opens the column). */
bool interaction_bubble_is_collapsed(void);
void interaction_bubble_expand(void);
bool interaction_bubble_handle_wheel(float wheel_delta);
int  interaction_bubble_slot_count(void);

/* Apply an equip or unequip that happened while the self-player is dead. The
 * server queues those in PreRespawnObjectLayers and never sends them back, so
 * the bubble updates its own alive cache to match what revive will render. */
void interaction_bubble_dead_equip(const char* item_id, bool active);

/* Cached alive stack for an entity. Returns NULL and sets *out_count to 0
 * when the entity has no bubble slot or no alive cache. */
const ObjectLayerState* interaction_bubble_get_alive_layers(
    const char *entity_id, int *out_count);

/* Initial JS overlay tab, selected by modal_interact's action buttons. */
#define INTERACT_OVERLAY_TAB_CHAT         0
#define INTERACT_OVERLAY_TAB_INTEGRATION  1

/* Open the JS overlay on `initial_tab` (INTERACT_OVERLAY_TAB_*) and push the
 * entity's layer stack for the preview. No-op when the entity has no slot. */
void interaction_bubble_open_js_overlay(const char* entity_id, int initial_tab);

/* True when the bubble column UI covers the point. The toggle tab always
 * counts; the bubble band counts only while the column is expanded, so a
 * collapsed column gives the space back to world taps. */
bool interaction_bubble_point_covered(int x, int y);

#endif /* INTERACTION_BUBBLE_H */
