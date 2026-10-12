#ifndef GAME_STATE_H
#define GAME_STATE_H

#include <assert.h>
#include <raylib.h>
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
 *   - domain/local_player.h          identity, map code, coins, inventory,
 *                                    stasis flag, FCT queue, self status
 *                                    icon, authoritative move speed
 *   - ui/ui_state.h                  skill_map
 */

#define MAX_ENTITIES 1000
#define MAX_OBJECTS 5000
/* Slots in the entity table. One slot per entity in the area of interest. */
#define ENTITY_CAP 8192
#define MAX_ENTITY_TYPES     16
#define MAX_DEFAULT_ITEM_IDS  8
#define MAX_ACTIVE_ITEM_TYPES 8
#define MAX_DEAD_ITEM_IDS    16

/* EntityState / PlayerState / BotState / WorldObject + their MAX_* sizing
 * macros live in world_types.h. */

/* Object-layer pool. Every appearance holds an offset and a count into it
 * instead of an embedded [MAX_OBJECT_LAYERS] array, of which the server fills
 * one to four slots. Sized for the worst realistic AOI, not for the array
 * caps: the server ships only active layers. */
#define LAYER_POOL_SIZE 4096

extern ObjectLayerState g_layer_pool[LAYER_POOL_SIZE];

/* The layers of an Appearance. */
#define OBJ_LAYERS(ap) (&g_layer_pool[(ap)->layer_offset])

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
} EntityTypeDefault;

struct GameState {
    /* Instance the simulation server loaded (forwarded in the metadata
     * message) — keys the Instance Map REST fetches against engine-cyberia. */
    char instance_code[MAX_ID_LENGTH];

    int grid_w;
    int grid_h;
    float aoi_radius;

    EntityTypeDefault entity_defaults[MAX_ENTITY_TYPES];
    int entity_defaults_count;

    PlayerState player;

    PlayerState other_players[MAX_ENTITIES];
    int other_player_count;

    BotState bots[MAX_ENTITIES];
    int bot_count;

    /* Scenery ids in wire order. The server sends floors sorted, and the floor
     * pass draws them in this order. */
    EntityID scenery[MAX_OBJECTS];
    int scenery_count;

    BotState resources[MAX_ENTITIES];
    int resource_count;

    WorldObject portals[MAX_OBJECTS];
    int portal_count;

    EquipmentRules equipment_rules;

    /* Dead-state (Fragmentation) item ids from init_data. Visible in the
     * inventory but never equippable; the default id is server-filtered. */
    char dead_item_ids[MAX_DEAD_ITEM_IDS][128];
    int  dead_item_id_count;

    bool init_received;
    double last_update_time;       /* wall-clock arrival of the latest snapshot */

    /* The entity table. ids[i] is the entity at slot i, 0 = free slot. mask[i]
     * is the only answer to "does slot i have component X". */
    EntityID        ids[ENTITY_CAP];
    uint64_t        mask[ENTITY_CAP];
    EntityData      data[ENTITY_CAP];
    EntityTransform transform[ENTITY_CAP];
    Appearance      appearance[ENTITY_CAP];
    Vitals          vitals[ENTITY_CAP];
    RenderState     render[ENTITY_CAP];   /* client only */
};

/* Component bits of mask[], in the §4 order. */
#define COMP_DATA       ((uint64_t)1 << 0)
#define COMP_TRANSFORM  ((uint64_t)1 << 1)
#define COMP_APPEARANCE ((uint64_t)1 << 2)
#define COMP_VITALS     ((uint64_t)1 << 3)

extern GameState g_game_state;

/* Clear the world mirror to its post-disconnect defaults: the init flag, the
 * player id, and every entity and object count. The one entry point to reset
 * world state — code outside game_state.c must not touch the count fields. */
void         game_state_reset(void);

/* Drop every pooled layer. The whole pool dies with the entity counts, so the
 * decoder calls this where it resets them. */
void         game_state_layer_pool_reset(void);

/* Claim `count` pool slots and return the offset, or -1 when the pool is full.
 * The decoder is a bump allocator — there is no free. */
int          game_state_layer_alloc(int count);

/* The slot of `id`, given one if it has none. -1 when the table is full. Only
 * the snapshot decoder calls it. No code may keep a slot across a tick. */
int          game_state_spawn(EntityID id);

/* Free the slot of `id`. Its component values stay, but the clear mask makes
 * them unreadable. */
void         game_state_despawn(EntityID id);

/* The slot of `id`, or -1. */
int          game_state_slot(EntityID id);

/* The transform and the render state of `id`. The id must have a transform. */
static inline EntityData* game_state_data(EntityID id) {
    int slot = game_state_slot(id);
    assert(0 <= slot && 0 != (g_game_state.mask[slot] & COMP_DATA));
    return &g_game_state.data[slot];
}

static inline EntityTransform* game_state_transform(EntityID id) {
    int slot = game_state_slot(id);
    assert(0 <= slot && 0 != (g_game_state.mask[slot] & COMP_TRANSFORM));
    return &g_game_state.transform[slot];
}

static inline Appearance* game_state_appearance(EntityID id) {
    int slot = game_state_slot(id);
    assert(0 <= slot && 0 != (g_game_state.mask[slot] & COMP_APPEARANCE));
    return &g_game_state.appearance[slot];
}

static inline Vitals* game_state_vitals(EntityID id) {
    int slot = game_state_slot(id);
    assert(0 <= slot && 0 != (g_game_state.mask[slot] & COMP_VITALS));
    return &g_game_state.vitals[slot];
}

static inline RenderState* game_state_render(EntityID id) {
    int slot = game_state_slot(id);
    assert(0 <= slot && 0 != (g_game_state.mask[slot] & COMP_TRANSFORM));
    return &g_game_state.render[slot];
}

/* The own player's transform and render state. Before the first snapshot the
 * own player has no slot, and both read as zero. */
const EntityTransform* game_state_own_transform(void);
const RenderState*     game_state_own_render(void);

PlayerState* game_state_find_player(EntityID id);
BotState*    game_state_find_bot(EntityID id);

/* Any entity a modal can target: the local player, other players, bots and
 * resources. 0 resolves to the local player. */
const EntityState* game_state_find_entity(EntityID id);

static inline const EntityTypeDefault* game_state_get_entity_default(const char* entity_type) {
    for (int i = 0; i < g_game_state.entity_defaults_count; i++) {
        if (strcmp(g_game_state.entity_defaults[i].entity_type, entity_type) == 0)
            return &g_game_state.entity_defaults[i];
    }
    return NULL;
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

/* True for a bot that is a creature in the world rather than a thing travelling through it:
 * skill projectiles, coins and drops carry no feet. These are the bots that walk, raise dust and
 * cast the ground shadow, so the rule is stated once and read by everything that needs it. */
static inline bool game_state_bot_has_feet(const BotState* bot) {
    if (!bot || 0 != bot->caster_id) return false;
    return 0 != strcmp(bot->behavior, "skill") &&
           0 != strcmp(bot->behavior, "coin") &&
           0 != strcmp(bot->behavior, "drop");
}

/* Ordinary obstacles have zero maximum life and cannot deplete. */
static inline bool game_state_obstacle_depleted(const EntityData* d) {
    return 0.0f < d->max_life && 0.0f >= d->life;
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
