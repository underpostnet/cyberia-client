#ifndef CYBERIA_DOMAIN_LOCAL_PLAYER_H
#define CYBERIA_DOMAIN_LOCAL_PLAYER_H

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include <raylib.h>

#include "net/client_event.h"           /* Box */
#include "ui/floating_combat_text.h"   /* FCTType */
#include "world/object_layer.h"
#include "content/stat_contract_generated.h"
#include "world/world_types.h"

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
 * the own slot of g_game_state, where every entity keeps it. The decoder is
 * the sole writer. */

/* Matches the server storageMaxSlots and ITEM_SLOT_GRID_MAX_SLOTS. */
#define VAULT_SLOTS_MAX 64
#define VAULTS_MAX       8
#define INTERACTIONS_MAX 64
/* Every stack the inventory capacity opens, plus one appearance for the coin
 * slot and a dead loadout. */
#define INVENTORY_LAYERS_MAX (CYBERIA_INVENTORY_SLOTS_MAX + MAX_OBJECT_LAYERS)

/* The stacks of one container, in server order. */
typedef struct {
    ObjectLayerState items[VAULT_SLOTS_MAX];
    int              count;
} Stacks;

/* Why the world holds an owned stack instead of loading it. */
typedef enum { HELD_UNKNOWN, HELD_REBOUND, HELD_NO_ROOM, HELD_UNSUPPORTED, HELD_INCOMPATIBLE } HeldReason;

/* An owned stack the world cannot load now. It keeps the definition it was
 * granted with, so it is drawn by its own cid and is never usable here. */
typedef struct {
    char       item_id[MAX_ITEM_ID_LENGTH];
    char       cid[MAX_ITEM_ID_LENGTH];
    int        quantity;
    HeldReason reason;
} HeldStack;

/* The held stacks the self snapshot carries in full; the count stays exact. */
#define HELD_STACKS_SHOWN 8

/* One NPC vault of the player, keyed by the action code of its terminal. */
typedef struct {
    char   action_code[MAX_ID_LENGTH];
    Stacks stacks;
} Vault;

/* One member of the player's party, wherever it is on this server. */
typedef struct {
    EntityID id;
    char     name[PLAYER_NAME_BYTES];
    int      level;
    int      life;
    int      max_life;
    char     map_code[MAX_ID_LENGTH];
} PartyMember;

/* The party of the player in join order; member_count 0 is no party. */
typedef struct {
    EntityID    leader_id;
    PartyMember members[CYBERIA_PARTY_MAX_MEMBERS];
    int         member_count;
} Party;

/* An invite that waits for the player's answer. */
typedef struct {
    EntityID from_id;
    char     from_name[PLAYER_NAME_BYTES];
    double   expires_at;   /* GetTime() clock */
} PartyInvite;

/* An invite of the player that waits for an answer. */
typedef struct {
    EntityID to_id;
    double   expires_at;   /* GetTime() clock */
} SentInvite;

typedef enum { FRIEND_ACCEPTED, FRIEND_INCOMING, FRIEND_OUTGOING } FriendStatus;

/* One friendship or friend request of the account. A friend online on this
 * server carries its entity id; one in another world names that world. */
typedef struct {
    char         id[RECORD_ID_BYTES];       /* the friendship */
    char         name[PLAYER_NAME_BYTES];
    char         note[CYBERIA_STATUS_NOTE_MAX_RUNES * 4 + 1];
    FriendStatus status;
    bool         online;
    char         world[MAX_ID_LENGTH];
    char         map_code[MAX_ID_LENGTH];
    EntityID     entity_id;
    double       expires_at;                /* a request, GetTime() clock */
} Friend;

#define FRIENDS_LIST_MAX (CYBERIA_FRIENDS_MAX + 2 * CYBERIA_FRIEND_REQUESTS_MAX)

/* How far the engine holds the state of an account player, as the server
 * reports it: every change saved, one on its way, commits failing and
 * retrying, or the state refused until an operator repairs the records. */
typedef enum { DURABILITY_SAVED, DURABILITY_SAVING, DURABILITY_DEGRADED, DURABILITY_RECOVERY } Durability;

/* The stage of the trade of the player: it asked, it was asked, both edit
 * offers, or the server commits them. */
typedef enum { TRADE_NONE, TRADE_REQUESTED, TRADE_INCOMING, TRADE_OPEN, TRADE_COMMITTING } TradeState;

/* One offered stack. `slot` is the bag slot of an own line; -1 for the partner's. */
typedef struct {
    char item_id[MAX_ITEM_ID_LENGTH];
    char cid[MAX_CID_LENGTH];
    int  quantity;
    int  slot;
} TradeLine;

typedef struct {
    TradeLine items[CYBERIA_TRADE_OFFER_MAX_STACKS];
    int       count;
    int       coins;
    bool      confirmed;
} TradeOffer;

/* The trade of the player, as the server holds it. */
typedef struct {
    TradeState state;
    EntityID   partner_id;
    char       partner_name[PLAYER_NAME_BYTES];
    uint32_t   revision;      /* a confirmation names it */
    TradeOffer mine, theirs;
    double     expires_at;    /* GetTime() clock */
} Trade;

typedef struct {
    EntityID         id;
    bool             account;        /* the state persists with the account */
    HeldStack        held[HELD_STACKS_SHOWN];
    int              held_count;     /* owned stacks the world cannot load now */
    Durability       durability;
    int              unsaved_ms;     /* age of the oldest change the engine has not applied */
    int              portal_fee;     /* the fee the current portal hold charges, 0 off a portal */
    char             map_code[MAX_ID_LENGTH];
    int              coins;
    double           xp;             /* total XP */
    double           level_xp;       /* threshold of the current level */
    double           next_level_xp;  /* threshold of the next level */
    int              base_stats[CYBERIA_STAT_COUNT];
    int              layer_stats[CYBERIA_STAT_COUNT];
    int              temporary_stats[CYBERIA_STAT_COUNT];
    ObjectLayerState inventory[INVENTORY_LAYERS_MAX];
    int              inventory_count;
    /* The server copies of the containers, with the unconsumed ops on top. */
    Vault            vaults[VAULTS_MAX];
    int              vault_count;
    Stacks           storage;              /* the personal storage */
    Party            party;
    PartyInvite      invites[CYBERIA_PARTY_INVITES_MAX];
    int              invite_count;
    SentInvite       sent_invites[CYBERIA_PARTY_INVITES_MAX];
    int              sent_invite_count;
    bool             crafting;             /* the server runs an assembly */
    Trade            trade;
    Interaction      interactions[INTERACTIONS_MAX];
    int              interaction_count;
    Friend           friends[FRIENDS_LIST_MAX];
    int              friend_count;
    bool             friends_loaded;
    bool             friends_unavailable;
    char             status_note[CYBERIA_STATUS_NOTE_MAX_RUNES * 4 + 1];
    double           note_ready_at;
} LocalPlayer;

/* Friend requests that wait for the player's answer. */
int local_player_incoming_requests(void);

/* Social news: the friend requests and the party invites that wait for the player. */
int local_player_social_news(void);

/* The vault the player has at `action_code`, or NULL when it is still empty. */
Vault* local_player_vault(const char* action_code);

/* True when item_id is the currency: its stack lives in the wallet slot and
 * takes no container slot. */
bool local_player_is_coin(const char* item_id);

/* Open slots of a container rule at the player's level. */
int  local_player_open_slots(CyberiaCapacityRule rule);

/* Level that opens slot `slot` of a container rule; 0 when this instance
 * never reaches it. */
int  local_player_unlock_level(CyberiaCapacityRule rule, int slot);

/* Inventory stacks that take a slot: every stack but the coin. */
int  local_player_inventory_used(void);

/* True when the inventory takes more of item_id: the stack is held, the item
 * is the coin, or a slot is open. Mirrors the server rule. */
bool local_player_inventory_has_room(const char* item_id);

/* The party member `id`, or NULL. */
const PartyMember* local_player_party_member(EntityID id);

/* True when the local player leads its party. */
bool local_player_leads_party(void);

/* True while the goods of the player cannot be saved: the server refuses its
 * trades until a commit lands again. */
bool local_player_unsaved(void);

/* The pending invite from `id`, or NULL. */
const PartyInvite* local_player_invite_from(EntityID id);

/* The pending invite of the local player to `id`, or NULL. */
const SentInvite* local_player_invite_to(EntityID id);

/* The friendship or request of the player on this server `id`, by entity id
 * or by its unique account name, or NULL. */
const Friend* local_player_friend_of(EntityID id);

/* What the bot `id` offers the player, or NULL when it offers nothing. */
const Interaction* local_player_interaction(EntityID id);

static inline uint8_t local_player_interaction_flags(EntityID id) {
    const Interaction* in = local_player_interaction(id);
    return in ? in->flags : 0;
}

extern LocalPlayer g_local_player;

static inline int local_player_coins(void) { return g_local_player.coins; }

/* True while a trade holds the goods of the player: open or committing. */
static inline bool local_player_trading(void) {
    return TRADE_OPEN == g_local_player.trade.state || TRADE_COMMITTING == g_local_player.trade.state;
}

/* Progress through the current level, 0..1. A full bar at the maximum level,
 * where both thresholds meet. */
static inline float local_player_xp_ratio(void) {
    double span = g_local_player.next_level_xp - g_local_player.level_xp;
    if (0.0 >= span) return 1.0f;
    double ratio = (g_local_player.xp - g_local_player.level_xp) / span;
    return 0.0 > ratio ? 0.0f : 1.0 < ratio ? 1.0f : (float)ratio;
}

/* Quantity of an item the player holds over all its stacks; 0 when the
 * inventory has none. Coins included — the server keeps their display slot in
 * sync with the flat balance. */
static inline int local_player_item_quantity(const char* item_id) {
    if (NULL == item_id || '\0' == item_id[0]) return 0;
    int quantity = 0;
    for (int i = 0; i < g_local_player.inventory_count; i++) {
        if (0 == strcmp(g_local_player.inventory[i].item_id, item_id))
            quantity += g_local_player.inventory[i].quantity;
    }
    return quantity;
}

/* The inventory index of the stack in bag slot `slot`, or -1. */
static inline int local_player_bag_stack(int slot) {
    for (int i = 0; 0 <= slot && i < g_local_player.inventory_count; i++) {
        if (slot == g_local_player.inventory[i].slot) return i;
    }
    return -1;
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

/* Assemble an assembler entity's recipe for the fee the player saw. The caller
 * checks the ingredients first; the snapshot self block then carries the
 * running assembly, and craft_ack a refusal. */
void  local_player_request_craft(EntityID entity_id, int recipe_index, int fee);

/* Drop `qty` units of the stack in `slot` of `box` (bag or storage) on the
 * world cell `at`; a negative cell drops at the feet. item_drop_ack answers. */
void  local_player_request_item_drop(Box box, int slot, int qty, Vector2 at);

/* Ask the terminal `entity_id` the price of a sale (`burn` false) or a burn
 * of the bag stack in `slot`. item_quote_ack answers with a quote. */
void  local_player_request_quote(EntityID entity_id, bool burn, int slot);

/* Confirm a quote for `quantity` units; item_sell_ack or item_burn_ack answers. */
void  local_player_confirm_quote(bool burn, const char* quote_id, int quantity);

/* Ask the market of terminal `terminal` for the page at cursor `after` ("" the
 * first) of the listings of `mode` ("" every mode); a market message answers. */
void  local_player_market_open(EntityID terminal, const char* mode, const char* after);

/* List `quantity` units of the bag stack in `slot` at terminal `terminal` for
 * `hours`: an auction opening at `price`, else a fixed price. */
void  local_player_market_list(EntityID terminal, int slot, int quantity, bool auction, int price, int hours);

/* Cancel, buy or bid (`price`) on a listing of the last market view. A
 * market_ack answers each, and a fresh view follows it. */
void  local_player_market_cancel(EntityID terminal, const char* listing_id);
void  local_player_market_buy(EntityID terminal, const char* listing_id);
void  local_player_market_bid(EntityID terminal, const char* listing_id, int price);

/* Take the pending claims of the last market view. */
void  local_player_market_collect(EntityID terminal);

/* Trade requests. trade_ack answers a refusal; the snapshot carries the trade. */
void  local_player_request_trade(EntityID player_id);
void  local_player_request_trade_reply(EntityID from_id, bool accept);
void  local_player_request_trade_offer(const TradeOffer* offer);
void  local_player_request_trade_confirm(void);
void  local_player_request_trade_cancel(void);


/* Abort the running assembly — the server refunds its ingredients. */
void  local_player_request_craft_cancel(void);

/* Party requests. The server answers a refusal with party_ack; the snapshot
 * carries the result. */
void  local_player_request_party_invite(EntityID player_id);
void  local_player_request_party_reply(EntityID from_id, bool accept);
void  local_player_request_party_leave(void);
void  local_player_request_party_kick(EntityID member_id);
void  local_player_request_party_disband(void);

/* Friend requests. The server answers each with a fresh friend list, or a
 * refusal with friend_ack. */
void  local_player_request_friends(void);
void  local_player_request_friend(EntityID player_id);
void  local_player_request_friend_reply(const char* friendship_id, bool accept);
void  local_player_request_friend_remove(const char* friendship_id);

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
