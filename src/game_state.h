#ifndef GAME_STATE_H
#define GAME_STATE_H

#include <raylib.h>
#include <limits.h>
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>

#include "object_layer.h"
#include "world_types.h"

/* The client mirror of the simulation. Carries gameplay fields only:
 * entities with their positions and life, the world configuration the server
 * pushed at boot, and the economy and inventory state the simulation owns.
 *
 * Render data, per-frame UI bookkeeping, status indicators, and the camera
 * live in their own modules:
 *
 *   - domain/camera.h                Camera2D and follow smoothing
 *   - domain/presentation_runtime.h  palette, status icon visuals, dev_ui
 *   - domain/local_player.h          frozen flag, FCT queue, self status
 *                                    icon, authoritative move speed
 *   - ui/ui_state.h                  skill_map
 */

#define MAX_ENTITIES 1000
#define MAX_OBJECTS 5000
#define MAX_MESSAGE_SIZE USHRT_MAX
#define MAX_ENTITY_TYPES     16
#define MAX_DEFAULT_ITEM_IDS  8
#define MAX_ACTIVE_ITEM_TYPES 8
#define MAX_DEAD_ITEM_IDS    16

/* EntityState / PlayerState / BotState / WorldObject + their MAX_* sizing
 * macros live in world_types.h. */

typedef struct GameState GameState;

typedef struct {
    char active_item_types[MAX_ACTIVE_ITEM_TYPES][32];
    int  active_item_type_count;
    bool one_per_type;
    bool require_skin;
} EquipmentRules;

typedef struct {
    char entity_type[32];
    char live_item_ids[MAX_DEFAULT_ITEM_IDS][128];
    int  live_item_id_count;
    char dead_item_ids[MAX_DEFAULT_ITEM_IDS][128];
    int  dead_item_id_count;
    char drop_item_ids[MAX_DEFAULT_ITEM_IDS][128];
    int  drop_item_id_count;
} EntityTypeDefault;

struct GameState {
    char player_id[MAX_ID_LENGTH];

    /* Instance the simulation server loaded (forwarded in the metadata
     * message) — keys the Instance Map REST fetches against engine-cyberia. */
    char instance_code[MAX_ID_LENGTH];

    int grid_w;
    int grid_h;
    float cell_size;
    int interpolation_ms;
    float aoi_radius;

    EntityTypeDefault entity_defaults[MAX_ENTITY_TYPES];
    int entity_defaults_count;

    PlayerState player;

    PlayerState other_players[MAX_ENTITIES];
    int other_player_count;

    BotState bots[MAX_ENTITIES];
    int bot_count;

    WorldObject obstacles[MAX_OBJECTS];
    int obstacle_count;

    WorldObject foregrounds[MAX_OBJECTS];
    int foreground_count;

    /* Static decorators — non-moving, passable; depth-sorted with entities.
     * AOI-bounded like bots/resources, so sized MAX_ENTITIES (not MAX_OBJECTS,
     * which is reserved for full-map-tiling objects like floors). */
    WorldObject statics[MAX_ENTITIES];
    int static_count;

    BotState resources[MAX_ENTITIES];
    int resource_count;

    WorldObject portals[MAX_OBJECTS];
    int portal_count;

    WorldObject floors[MAX_OBJECTS];
    int floor_count;

    int sum_stats_limit;
    int active_stats_sum;

    int player_coins;

    EquipmentRules equipment_rules;

    ObjectLayerState full_inventory[MAX_OBJECT_LAYERS];
    int full_inventory_count;

    /* Dead-state (Fragmentation) item ids from init_data. Visible in the
     * inventory but never equippable; the default id is server-filtered. */
    char dead_item_ids[MAX_DEAD_ITEM_IDS][128];
    int  dead_item_id_count;

    bool init_received;
    double last_update_time;       /* wall-clock arrival of the latest snapshot */
    uint32_t last_snapshot_tick;   /* mirror of session_server_tick_estimate() */
};

extern GameState g_game_state;

/* Clear the world mirror to its post-disconnect defaults: the init flag, the
 * player id, and every entity and object count. The one entry point to reset
 * world state — code outside game_state.c must not touch the count fields. */
void         game_state_reset(void);

PlayerState* game_state_find_player(const char* id);
BotState*    game_state_find_bot(const char* id);
int          game_state_update_player(const PlayerState* player);
int          game_state_update_bot(const BotState* bot);
void         game_state_remove_player(const char* id);
void         game_state_remove_bot(const char* id);

/* Fires when an entity leaves the world mirror (left the AOI). Lets the
 * presentation layer release its per-entity resources, such as animation
 * states, without game_state depending on the render modules. */
typedef void (*GameStateEntityRemovedFn)(const char* id);
void         game_state_set_entity_removed_cb(GameStateEntityRemovedFn cb);

/* Toggle the dev overlay flag. Delegates to presentation_runtime, which
 * keeps the one copy of the value. */
void game_state_toggle_dev_ui(void);

static inline const EntityTypeDefault* game_state_get_entity_default(const char* entity_type) {
    for (int i = 0; i < g_game_state.entity_defaults_count; i++) {
        if (strcmp(g_game_state.entity_defaults[i].entity_type, entity_type) == 0)
            return &g_game_state.entity_defaults[i];
    }
    return NULL;
}

static inline int game_state_get_player_coins(void) {
    return g_game_state.player_coins;
}

/* Quantity of an item the player holds; 0 when the inventory has no stack for
 * it. Coins included — the server keeps their display slot in sync with the
 * flat balance. */
static inline int game_state_item_quantity(const char* item_id) {
    if (NULL == item_id || '\0' == item_id[0]) return 0;
    for (int i = 0; i < g_game_state.full_inventory_count; i++) {
        if (0 == strcmp(g_game_state.full_inventory[i].item_id, item_id))
            return g_game_state.full_inventory[i].quantity;
    }
    return 0;
}

/* True when item_id is a dead-state (Fragmentation) visual — an incomplete
 * manifestation the player can hold but never equip. */
static inline bool game_state_is_dead_item(const char* item_id) {
    if (NULL == item_id || '\0' == item_id[0]) return false;
    for (int i = 0; i < g_game_state.dead_item_id_count; i++) {
        if (0 == strcmp(g_game_state.dead_item_ids[i], item_id)) return true;
    }
    return false;
}

static inline bool game_state_is_active_item_type(const char* item_type) {
    if (!item_type || item_type[0] == '\0') return false;
    for (int i = 0; i < g_game_state.equipment_rules.active_item_type_count; i++) {
        if (strcmp(g_game_state.equipment_rules.active_item_types[i], item_type) == 0)
            return true;
    }
    return false;
}

#endif /* GAME_STATE_H */
