#ifndef CYBERIA_DOMAIN_LOCAL_PLAYER_H
#define CYBERIA_DOMAIN_LOCAL_PLAYER_H

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "ui/floating_combat_text.h"   /* FCTType */
#include "object_layer.h"
#include "domain/stat_contract_generated.h"
#include "world_types.h"

/* Local-player state — the one owner of everything the AOI self-player block
 * says about this client and nobody else:
 *
 *   - identity, map code, coin balance, progression and the full inventory
 *     (g_local_player)
 *   - stasis flag (modal protection)
 *   - status icon ID (overhead UI hint)
 *   - authoritative move speed (cells/second) for the prediction integrator
 *   - per-frame FCT event queue drained by the floating combat text module
 *
 * None of it is world state, so it lives outside the simulation-shaped
 * GameState. The derived state stays out too: local_player_view owns the
 * smoothed presentation, replication owns the predicted position and route.
 */

/* What the server says about this client's own player and nobody else's:
 * identity, the map it stands on, the economy counters and the full
 * inventory. World-mirror data — position, layers, life — stays on
 * g_game_state.player, where every entity keeps it. The snapshot decoder is
 * the sole writer. */

/* Matches the server storageMaxSlots and ITEM_SLOT_GRID_MAX_SLOTS. */
#define VAULT_SLOTS_MAX 64
#define VAULTS_MAX       8
#define INTERACTIONS_MAX 64

/* One vault of the player, keyed by the action code of its terminal. */
typedef struct {
    char             action_code[MAX_ID_LENGTH];
    ObjectLayerState items[VAULT_SLOTS_MAX];
    int              count;
} Vault;

typedef struct {
    EntityID         id;
    char             engine_id[MAX_ID_LENGTH]; /* the own id for engine REST calls */
    char             map_code[MAX_ID_LENGTH];
    int              coins;
    double           xp;             /* total XP */
    double           level_xp;       /* threshold of the current level */
    double           next_level_xp;  /* threshold of the next level */
    int              base_stats[CYBERIA_STAT_COUNT];
    int              layer_stats[CYBERIA_STAT_COUNT];
    int              temporary_stats[CYBERIA_STAT_COUNT];
    ObjectLayerState inventory[MAX_OBJECT_LAYERS];
    int              inventory_count;
    Vault            vaults[VAULTS_MAX];   /* the server copy, with the unconsumed ops on top */
    int              vault_count;
    bool             crafting;             /* the server runs an assembly */
    Interaction      interactions[INTERACTIONS_MAX];
    int              interaction_count;
} LocalPlayer;

/* The vault the player has at `action_code`, or NULL when it is still empty. */
Vault* local_player_vault(const char* action_code);

/* What the bot `id` offers the player, or NULL when it offers nothing. */
const Interaction* local_player_interaction(EntityID id);

static inline uint8_t local_player_interaction_flags(EntityID id) {
    const Interaction* in = local_player_interaction(id);
    return in ? in->flags : 0;
}

extern LocalPlayer g_local_player;

static inline int local_player_coins(void) { return g_local_player.coins; }

/* Progress through the current level, 0..1. A full bar at the maximum level,
 * where both thresholds meet. */
static inline float local_player_xp_ratio(void) {
    double span = g_local_player.next_level_xp - g_local_player.level_xp;
    if (0.0 >= span) return 1.0f;
    double ratio = (g_local_player.xp - g_local_player.level_xp) / span;
    return 0.0 > ratio ? 0.0f : 1.0 < ratio ? 1.0f : (float)ratio;
}

/* Quantity of an item the player holds; 0 when the inventory has no stack for
 * it. Coins included — the server keeps their display slot in sync with the
 * flat balance. */
static inline int local_player_item_quantity(const char* item_id) {
    if (NULL == item_id || '\0' == item_id[0]) return 0;
    for (int i = 0; i < g_local_player.inventory_count; i++) {
        if (0 == strcmp(g_local_player.inventory[i].item_id, item_id))
            return g_local_player.inventory[i].quantity;
    }
    return 0;
}

#define LOCAL_FCT_PENDING_MAX 64

typedef struct {
    float    world_x;
    float    world_y;
    uint32_t value;
    FCTType  type;
} LocalFctEvent;

/* Reset all local-player flags to their post-disconnect defaults. */
void  local_player_reset(void);

/* Stasis flag. The modal counter predicts it; the snapshot replaces it. */
void  local_player_set_stasis(bool stasis);
bool  local_player_in_stasis(void);

/* Abandon an active quest by code — server moves it to the failed section. */
void  local_player_request_quest_abandon(const char* quest_code);

/* Accept the quest the entity offers — the only path to start a mission. */
void  local_player_request_quest_accept(EntityID entity_id, const char* quest_code);

/* Buy `quantity` units of a vendor entity's catalog item. The server validates
 * the total price against the player's balance and replies with a shop_ack. */
void  local_player_request_shop_buy(EntityID entity_id, const char* item_id,
                                    int quantity);

/* Assemble an assembler entity's recipe. The caller checks the ingredients
 * first; the snapshot self block then carries the running assembly. */
void  local_player_request_craft(EntityID entity_id, int recipe_index);

/* Abort the running assembly — the server refunds its ingredients. */
void  local_player_request_craft_cancel(void);

/* Status icon ID (mirrors entity status indicator for self). */
void     local_player_set_status_icon(uint8_t id);
uint8_t  local_player_status_icon(void);

/* Authoritative move speed in grid units per second, pushed by the server
 * in every AOI self-player block. Returns a non-zero positive value at
 * steady state; before the first snapshot it returns the bootstrap default. */
void   local_player_set_move_speed(float speed);
float  local_player_move_speed(void);

/* Authoritative action cooldown in seconds, pushed by the server in every AOI
 * self-player block. It does not gate movement — that re-plans every tick —
 * but every tap fires skills, so it is the period after which repeating an
 * unchanged input is worth anything. Keyboard steering paces its refresh by
 * it. Returns the bootstrap default before the first snapshot. */
void   local_player_set_action_cooldown(float seconds);
float  local_player_action_cooldown(void);

/* Authoritative end of the running action cooldown, on the GetTime() clock.
 * Every skill preview reads this one value. */
void   local_player_set_action_ready_at(double time_s);
/* Elapsed part of the action cooldown at `now`, 0..1. 1 means the next tap
 * fires skills; a skill can still fail its summon chance. */
float  local_player_action_charge(double now);

/* Authoritative end of the consume cooldown a held skill starts, on the
 * GetTime() clock. The action cooldown rule times it. */
void   local_player_set_consume_ready_at(double time_s);
/* Elapsed part of the consume cooldown at `now`, 0..1. */
float  local_player_consume_charge(double now);

/* FCT event queue — single-producer (message.c) /
 * single-consumer (floating_combat_text). */
bool                   local_player_fct_push(const LocalFctEvent* ev);
int                    local_player_fct_count(void);
const LocalFctEvent*   local_player_fct_at(int idx);
void                   local_player_fct_clear(void);

#endif /* CYBERIA_DOMAIN_LOCAL_PLAYER_H */
