#ifndef WORLD_TYPES_H
#define WORLD_TYPES_H

#include <raylib.h>
#include <stdbool.h>
#include <stdint.h>

#include "object_layer.h"
#include "domain/stat_contract_generated.h"

/* World-entity structures for the game-state mirror. Definitions only — no
 * behaviour and no global state — so a module can use them without pulling
 * in g_game_state or its accessors. */

#define MAX_OBJECT_LAYERS   20
#define MAX_PATH_POINTS     100
#define MAX_ID_LENGTH       64
#define MAX_BEHAVIOR_LENGTH 32

/* The id of an entity on the server, on the wire and here. 0 is no entity. */
typedef uint32_t EntityID;

/* The transform component: the top-left and the size of an entity, in grid
 * units, as the newest snapshot gives them. raylib owns the name Transform. */
typedef struct {
    Vector2 pos;
    Vector2 size;
} EntityTransform;

/* Client-only render interpolation, beside the components. snapshot_time is
 * the arrival of the snapshot that wrote transform.pos; the lerp runs from
 * pos_prev to transform.pos per entity. */
typedef struct {
    Vector2 pos_prev;
    Vector2 interp_pos;
    double  snapshot_time;
} RenderState;

typedef struct EntityState EntityState;
typedef struct PlayerState PlayerState;
typedef struct BotState BotState;

struct EntityState {
    EntityID id;
    Direction direction;
    ObjectLayerMode mode;
    uint16_t layer_offset;   /* index into g_layer_pool */
    uint8_t  layer_count;
    float life;
    float max_life;
    float respawn_in;
    int stats_sum;
    int level;
    float effective_stats[CYBERIA_STAT_COUNT];
    uint8_t status_icon;
    bool on_portal;     /* a player standing on a portal */
    float portal_hold;  /* its teleport charge, 0..1 */
};

struct PlayerState {
    EntityState base;
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
    EntityID caster_id;
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
    EntityID         id;
    char             type[MAX_TYPE_LENGTH];
    /* Portal-only: presence status icon (ESI 10) + teleport destination, used to
     * build the "<targetMapCode> <x>,<y>" overhead nameplate. Zero for non-portals. */
    uint8_t          status_icon;
    char             target_map_code[MAX_ID_LENGTH];
    int              target_cell_x;
    int              target_cell_y;
    uint16_t         layer_offset;   /* index into g_layer_pool */
    uint8_t          layer_count;
    /* Foreground-only: the bound behavior is "overhead-occlusion". */
    bool             overhead_occlusion;
    /* Obstacle-only: authoritative life of a destructible obstacle. An
     * ordinary obstacle has max_life 0; a depleted one has life 0. */
    float            life;
    float            max_life;
} WorldObject;

#endif /* WORLD_TYPES_H */
