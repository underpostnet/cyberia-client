#ifndef WORLD_TYPES_H
#define WORLD_TYPES_H

#include <raylib.h>
#include <stdint.h>

#include "object_layer.h"

/**
 * @file world_types.h
 * @brief Plain world-entity data structures shared by the serializer and the
 *        game-state mirror.
 *
 * These are pure structure definitions with no behaviour and no global state.
 * The serializer depends only on this header so it can stay structure-aware
 * without pulling in the GameState god object (g_game_state) or its accessors.
 */

#define MAX_OBJECT_LAYERS   20
#define MAX_PATH_POINTS     100
#define MAX_ID_LENGTH       64
#define MAX_BEHAVIOR_LENGTH 32

typedef struct EntityState EntityState;
typedef struct PlayerState PlayerState;
typedef struct BotState BotState;

struct EntityState {
    char id[MAX_ID_LENGTH];
    Vector2 pos_server;
    Vector2 pos_prev;
    Vector2 interp_pos;
    Vector2 dims;
    Direction direction;
    ObjectLayerMode mode;
    ObjectLayerState object_layers[MAX_OBJECT_LAYERS];
    int object_layer_count;
    float life;
    float max_life;
    float respawn_in;
    double last_update;     /* wall-clock time the entity was last touched */
    double snapshot_time;   /* wall-clock time of the snapshot that produced
                             * pos_server. Used by interpolation to compute
                             * a per-entity alpha instead of a global one. */
    int stats_sum;          /* sum of active stats, capped at sum_stats_limit */
    uint8_t status_icon;
};

struct PlayerState {
    EntityState base;
    char map_code[MAX_ID_LENGTH];
    Vector2 path[MAX_PATH_POINTS]; /* debug only */
    int path_count;                /* debug only */
    Vector2 target_pos;            /* debug only */

    Vector2 tap_target;
    bool    has_tap_target;
};

/* Interaction capability bits (mirror cyberia-server entity_status.go). The bit
 * index pairs with the capability status-icon ID (bit i ↔ status icon 8+i).
 * Both bits mean "actionable for this player right now" and drive attention
 * icons only: the quest bit lights for an acceptable/advanceable quest (never
 * completed feedback — the Quest tab follows quest_codes instead); the action
 * bit marks a pending action-talk-quest (quest-framed dialogue, no tab). */
#define INTERACTION_FLAG_ACTION       (1u << 0)
#define INTERACTION_FLAG_QUEST        (1u << 1)
/* Drop tokens only, per viewing player: set when the local player is a damage
 * contributor eligible to collect. Drives particle tint (gold vs gray); no
 * overhead icon. */
#define INTERACTION_FLAG_LOOT_ELIGIBLE (1u << 2)
#define STATUS_ICON_NONE              0
#define STATUS_ICON_PASSIVE           1
#define STATUS_ICON_HOSTILE           2
#define STATUS_ICON_FROZEN            3
#define STATUS_ICON_PLAYER            4
#define STATUS_ICON_DEAD              5
#define STATUS_ICON_RESOURCE          6
#define STATUS_ICON_RESOURCE_EXTRACTED 7
#define STATUS_ICON_ACTION_PROVIDER   8
#define STATUS_ICON_QUEST_PROVIDER    9
#define STATUS_ICON_PORTAL            10
#define STATUS_ICON_PORTAL_RANDOM     11

#define BOT_QUEST_CODES_MAX           8

struct BotState {
    EntityState base;
    char behavior[MAX_BEHAVIOR_LENGTH];
    char caster_id[MAX_ID_LENGTH];
    /* Bound cyberia-action code; "" for ordinary bots. The client fetches the
     * action metadata (label, dialogue map) by this code via REST.
     * Position-independent, so a wandering NPC keeps it. */
    char action_code[MAX_ID_LENGTH];
    /* Per-player interaction capability bitmask (INTERACTION_FLAG_*). */
    uint8_t interaction_flags;
    /* Authoritative quest codes this NPC provides to the local player; metadata
     * is fetched by code only when not already cached. */
    char quest_codes[BOT_QUEST_CODES_MAX][MAX_ID_LENGTH];
    int  quest_code_count;
    /* Per-player pending quest-talk dialogue code for quest_codes[i], "" when
     * that quest has no talk objective to advance here. The interact modal
     * offers one quest-talk button per non-empty entry. */
    char quest_talk_dialog_codes[BOT_QUEST_CODES_MAX][MAX_ID_LENGTH];
};

typedef struct WorldObject {
    char             id[MAX_ID_LENGTH];
    Vector2          pos;
    Vector2          dims;
    ObjectLayerType  type_kind;
    char             type[MAX_TYPE_LENGTH];
    /* Portal-only: presence status icon (ESI 10) + teleport destination, used to
     * build the "<targetMapCode> <x>,<y>" overhead nameplate. Zero for non-portals. */
    uint8_t          status_icon;
    char             target_map_code[MAX_ID_LENGTH];
    int              target_cell_x;
    int              target_cell_y;
    ObjectLayerState object_layers[MAX_OBJECT_LAYERS];
    int              object_layer_count;
} WorldObject;

#endif /* WORLD_TYPES_H */
