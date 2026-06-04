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

/**
 * @file game_state.h
 * @brief Authoritative world-state view.
 *
 * GameState is the client's mirror of the simulation. It carries only
 * gameplay-relevant fields: entities, their positions and life, the
 * world configuration that the server pushed at boot, and the
 * economy/inventory state that the simulation owns.
 *
 * Render-only data, per-frame UI bookkeeping, status indicators, and the
 * camera live in dedicated modules:
 *
 *   - domain/camera.h           Camera2D + follow-smoothing
 *   - domain/presentation_runtime.h  palette, status icon visuals, dev_ui
 *   - domain/local_player.h     frozen flag, FCT queue, self status icon,
 *                               authoritative move speed
 *   - ui/ui_state.h             skill_map, associated_item_ids
 */

#define MAX_ENTITIES 1000
#define MAX_OBJECTS 5000
#define MAX_MESSAGE_SIZE USHRT_MAX
#define MAX_ENTITY_TYPES     16
#define MAX_DEFAULT_ITEM_IDS  8
#define MAX_ACTIVE_ITEM_TYPES 8

/* EntityState / PlayerState / BotState / WorldObject + their MAX_* sizing
 * macros live in world_types.h so the serializer can use them without
 * depending on this header's global GameState. */

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

    bool init_received;
    double last_update_time;       /* wall-clock arrival of the latest snapshot */
    uint32_t last_snapshot_tick;   /* mirror of session_server_tick_estimate() */

    char pending_error[256];
};

extern GameState g_game_state;

/** Clear the world mirror to its post-disconnect defaults: drops init flag,
 *  player id, and all entity/object counts. The single entry point for
 *  resetting world state; callers outside game_state.c must not poke the
 *  count fields directly. */
void         game_state_reset(void);

PlayerState* game_state_find_player(const char* id);
BotState*    game_state_find_bot(const char* id);
int          game_state_update_player(const PlayerState* player);
int          game_state_update_bot(const BotState* bot);
void         game_state_remove_player(const char* id);
void         game_state_remove_bot(const char* id);

/** Observer fired when an entity is removed from the world mirror (left AOI).
 *  Lets the presentation layer release per-entity resources (e.g. animation
 *  states) without game_state depending on render modules. */
typedef void (*GameStateEntityRemovedFn)(const char* id);
void         game_state_set_entity_removed_cb(GameStateEntityRemovedFn cb);

/** Toggle the client-owned dev-overlay flag. The toggle delegates to
 *  presentation_runtime so the value stays a single source of truth. */
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

static inline bool game_state_is_active_item_type(const char* item_type) {
    if (!item_type || item_type[0] == '\0') return false;
    for (int i = 0; i < g_game_state.equipment_rules.active_item_type_count; i++) {
        if (strcmp(g_game_state.equipment_rules.active_item_types[i], item_type) == 0)
            return true;
    }
    return false;
}

#endif /* GAME_STATE_H */
