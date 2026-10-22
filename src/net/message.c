#include "ui/player_panel.h"
#include "message.h"

#include "config.h"
#include "sim/game_state.h"
#include "fx/fx_death.h"
#include "fx/fx_heal_aura.h"
#include "fx/fx_hit_burst.h"
#include "fx/fx_level_up.h"
#include "fx/fx_obstacle_burst.h"
#include "content/stat_contract_generated.h"
#include "serial.h"
#include "world/world_types.h"
#include "world/object_layer.h"
#include "world/object_layers_management.h"
#include "world/economy.h"
#include "sim/local_player.h"
#include "audio/audio_context.h"
#include "sim/self_motion.h"
#include "audio/audio.h"
#include "audio/audio_events.h"
#include "sim/replication.h"
#include "ui/chat_log.h"
#include "ui/floating_combat_text.h"
#include "ui/identity_icon.h"
#include "fx/fx_loot.h"
#include "ui/item_slot_grid.h"
#include "ui/market_panel.h"
#include "ui/merchant_panel.h"
#include "ui/modal_interact.h"
#include "ui/modal_notification.h"
#include "ui/save_status.h"
#include "ui/session_gate.h"
#include "client_event.h"
#include "game_client.h"
#include "data/instance_map_data.h"
#include "data/quest_progress_store.h"
#include "ui/ui_state.h"
#include "log.h"
#include "utils.h"

#include <cJSON.h>
#include <raylib.h>
#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static MessageInitHandler s_init_handler = NULL;

void message_set_init_handler(MessageInitHandler handler) {
    s_init_handler = handler;
}

/* A living thing: it has life and is not waiting to respawn. */
static bool entity_alive(const Vitals* v) {
    return 0 < v->max_life && 0 < v->life && 0 >= v->respawn_in;
}

/* The snapshot stamp of each slot. A live slot the newest snapshot did not
 * stamp has left the area of interest, and loses its slot (Q3). */
static uint32_t s_slot_stamp[ENTITY_CAP];
static uint32_t s_stamp = 0;

/* Give `id` its slot and mark it present in this snapshot. -1 when the table
 * is full: the decoder then skips the entity. */
static int claim_slot(EntityID id) {
    int slot = game_state_spawn(id);
    if (0 > slot) { LOG_WARN("entity table full, id %u has no slot", id); return -1; }
    s_slot_stamp[slot] = s_stamp;
    return slot;
}

/* Write the transform of `slot` from `e`. The slot keeps the previous
 * position for the lerp; a new slot, or a `snap`, starts at the new one. */
static Vector2 read_vec2(const cJSON* owner, const char* key) {
    const cJSON* v = serial_get_object(owner, key);
    if (NULL == v) return (Vector2){ 0 };
    return (Vector2){ serial_get_float_default(v, "x", 0.0f), serial_get_float_default(v, "y", 0.0f) };
}

static void write_transform(int slot, const cJSON* e, bool snap) {
    GameState* gs = &g_game_state;
    Vector2 pos  = read_vec2(e, "pos");
    Vector2 size = read_vec2(e, "size");
    RenderState* r = &gs->render[slot];
    if (snap || 0 == (gs->mask[slot] & COMP_TRANSFORM)) {
        r->pos_prev   = pos;
        r->interp_pos = pos;
    } else {
        r->pos_prev = gs->transform[slot].pos;
    }
    r->snapshot_time = gs->last_update_time;
    gs->transform[slot] = (EntityTransform){ .pos = pos, .size = size };
    gs->mask[slot] |= COMP_TRANSFORM;
}

static void free_absent_slots(void) {
    for (int i = 0; ENTITY_CAP > i; i++) {
        if (0 != g_game_state.ids[i] && s_stamp != s_slot_stamp[i])
            game_state_despawn(g_game_state.ids[i]);
    }
}

/* ============================================================================
 * Snapshot readers
 * ============================================================================ */

/* A full array drops the rest of its kind for that snapshot. Log it once per
 * array per snapshot, not once per dropped entity. */
enum { OVF_PLAYERS, OVF_BOTS, OVF_RESOURCES, OVF_SCENERY, OVF_PORTALS, OVF_LAYERS, OVF_COUNT };

static const char* const OVF_NAME[OVF_COUNT] = {
    "players", "bots", "resources", "scenery", "portals", "layer pool"
};

static uint16_t s_overflow_warned = 0;

static void warn_overflow(int which) {
    if (s_overflow_warned & (1u << which)) return;
    s_overflow_warned |= (uint16_t)(1u << which);
    LOG_WARN("snapshot truncated: %s array full", OVF_NAME[which]);
}


static int read_layers(const cJSON* owner, const char* key,
                       ObjectLayerState* layers, int max_layers, FetchPriority priority) {
    const cJSON* arr = serial_get_array(owner, key);
    int n = 0;
    const cJSON* item = NULL;
    cJSON_ArrayForEach(item, arr) {
        if (n >= max_layers) break;
        serial_get_string(item, "itemId", layers[n].item_id, MAX_ITEM_ID_LENGTH);
        layers[n].active   = serial_get_bool_default(item, "active", true);
        layers[n].quantity = serial_get_int_default(item, "quantity", 0);
        layers[n].slot     = serial_get_int_default(item, "slot", -1);
        /* Only what the player is wearing is critical; a carried spare is not yet drawn. */
        const FetchPriority wanted =
            (FETCH_P0 == priority && !layers[n].active) ? FETCH_P2 : priority;
        if (obj_layers_mgr_get()) obj_layers_mgr_schedule_atlas_fetch(layers[n].item_id, wanted);
        n++;
    }
    return n;
}

/* Write the appearance of `slot`: its layers, into the layer pool. A full pool
 * costs the entity its layers, so warn and leave it empty. */
static void write_appearance(int slot, const cJSON* e, FetchPriority priority) {
    Appearance* ap = &g_game_state.appearance[slot];
    g_game_state.mask[slot] |= COMP_APPEARANCE;
    int n = cJSON_GetArraySize(serial_get_array(e, "layers"));
    if (MAX_OBJECT_LAYERS < n) n = MAX_OBJECT_LAYERS;
    int pool_offset = game_state_layer_alloc(n);
    if (0 > pool_offset) {
        warn_overflow(OVF_LAYERS);
        *ap = (Appearance){ 0 };
        return;
    }
    ap->layer_offset = (uint16_t)pool_offset;
    ap->layer_count  = (uint8_t)read_layers(e, "layers", &g_layer_pool[pool_offset], n, priority);
}

static void read_stats(const cJSON* owner, const char* key, int values[CYBERIA_STAT_COUNT]) {
    const cJSON* array = serial_get_array(owner, key);
    for (int i = 0; CYBERIA_STAT_COUNT > i; i++) {
        const cJSON* item = cJSON_GetArrayItem(array, i);
        values[i] = cJSON_IsNumber(item) ? item->valueint : 0;
    }
}

/* `death_cue` is false for what is extracted rather than slain: a resource
 * goes straight to its extracted state with no death cue. */
static void read_entity_state(int slot, const cJSON* e, EntityID id, FetchPriority priority, bool death_cue) {
    g_game_state.motion[slot] = (Motion){
        .direction = (Direction)serial_get_int_default(e, "direction", 0),
        .mode      = (ObjectLayerMode)serial_get_int_default(e, "mode", 0),
    };
    g_game_state.mask[slot] |= COMP_MOTION;
    /* The slot keeps the prior vitals and level; a new slot has none. */
    GameState* gs = &g_game_state;
    Vitals* v = &gs->vitals[slot];
    Progress* pr = &gs->progress[slot];
    const bool prior_alive = 0 != (gs->mask[slot] & COMP_VITALS) && entity_alive(v);
    const int prior_level = 0 != (gs->mask[slot] & COMP_PROGRESS) ? pr->level : 0;
    *v = (Vitals){
        .life       = serial_get_int_default(e, "life", 0),
        .max_life   = serial_get_int_default(e, "maxLife", 0),
        .respawn_in = serial_get_int_default(e, "respawnIn", 0),
    };
    pr->level     = serial_get_int_default(e, "level", 0);
    pr->stats_sum = serial_get_int_default(e, "statsSum", 0);
    read_stats(e, "stats", pr->stats);
    gs->mask[slot] |= COMP_VITALS | COMP_PROGRESS;
    /* Snapshots own typed death cues. The server owns level-up audio. */
    fx_level_up_observe(prior_level, pr->level, id);
    if (death_cue &&
        fx_death_observe(prior_alive, 0 < v->max_life && !entity_alive(v), id)) {
        audio_event(AUDIO_EVENT_DEATH);
    }
}

/* Write the data of `slot`. The decoder knows the kind from its dispatch. */
static void write_data(int slot, const cJSON* e, EntityKind kind) {
    g_game_state.data[slot] = (EntityData){
        .kind        = (uint8_t)kind,
        .draw_order  = (uint8_t)serial_get_int_default(e, "drawOrder", DRAW_SORTED),
        .flags       = (uint8_t)serial_get_int_default(e, "flags", 0),
        .status_icon = (uint8_t)serial_get_int_default(e, "statusIcon", 0),
    };
    g_game_state.mask[slot] |= COMP_DATA;
    if (ENTITY_KIND_PLAYER != kind) return;
    Player* player = &g_game_state.player[slot];
    *player = (Player){
        .on_portal   = serial_get_bool_default(e, "onPortal", false),
        .portal_hold = serial_get_float_default(e, "portalHoldProgress", 0.0f),
    };
    serial_get_string(e, "name", player->name, sizeof player->name);
    g_game_state.mask[slot] |= COMP_PLAYER;
}

/* TELEPORTING is a one-snapshot signal that the entity jumped (portal), so a
 * lerp from the old position would sweep across the map. */
static bool teleported(const cJSON* e) {
    return MODE_TELEPORTING == serial_get_int_default(e, "mode", 0);
}

static void unpack_player(const cJSON* e) {
    GameState* gs = &g_game_state;
    EntityID id = serial_get_u32_default(e, "id", 0);
    int slot = claim_slot(id);
    if (0 > slot) return;

    int idx = -1;
    for (int i = 0; i < gs->other_player_count; i++) {
        if (id == gs->other_players[i]) { idx = i; break; }
    }
    if (idx < 0) {
        if (gs->other_player_count >= MAX_ENTITIES) { warn_overflow(OVF_PLAYERS); return; }
        idx = gs->other_player_count++;
        gs->other_players[idx] = id;
    }

    write_data(slot, e, ENTITY_KIND_PLAYER);
    write_transform(slot, e, teleported(e));
    write_appearance(slot, e, FETCH_P2);
    read_entity_state(slot, e, id, FETCH_P2, true);
}

static void unpack_bot(const cJSON* e) {
    GameState* gs = &g_game_state;
    EntityID id = serial_get_u32_default(e, "id", 0);
    int slot = claim_slot(id);
    if (0 > slot) return;

    int idx = -1;
    for (int i = 0; i < gs->bot_count; i++) {
        if (id == gs->bots[i]) { idx = i; break; }
    }
    if (idx < 0) {
        if (gs->bot_count >= MAX_ENTITIES) { warn_overflow(OVF_BOTS); return; }
        idx = gs->bot_count++;
        gs->bots[idx] = id;
    }

    write_data(slot, e, ENTITY_KIND_BOT);
    write_transform(slot, e, teleported(e));
    write_appearance(slot, e, FETCH_P2);
    read_entity_state(slot, e, id, FETCH_P2, true);

    Agent* agent = &gs->agent[slot];
    serial_get_string(e, "behavior", agent->behavior, MAX_BEHAVIOR_LENGTH);
    agent->caster_id = serial_get_u32_default(e, "casterId", 0);
    serial_get_string(e, "actionCode", agent->action_code, MAX_ID_LENGTH);
    gs->mask[slot] |= COMP_AGENT;
}

/* One row of the self `interaction` list. */
static void unpack_interaction(const cJSON* row, Interaction* in) {
    memset(in, 0, sizeof *in);
    in->id    = serial_get_u32_default(row, "id", 0);
    in->flags = (uint8_t)serial_get_int_default(row, "flags", 0);

    const cJSON* code = NULL;
    cJSON_ArrayForEach(code, serial_get_array(row, "questCodes")) {
        if (in->quest_code_count >= BOT_QUEST_CODES_MAX) break;
        if (!cJSON_IsString(code)) continue;
        copy_str(in->quest_codes[in->quest_code_count++], MAX_ID_LENGTH, cJSON_GetStringValue(code));
    }

    /* Parallel to quest_codes: entry i is non-empty when quest_codes[i] has an
     * open talk objective this NPC answers. */
    int t = 0;
    const cJSON* talk = NULL;
    cJSON_ArrayForEach(talk, serial_get_array(row, "questTalkDialogCodes")) {
        if (t >= BOT_QUEST_CODES_MAX) break;
        if (cJSON_IsString(talk))
            copy_str(in->quest_talk_dialog_codes[t], MAX_ID_LENGTH, cJSON_GetStringValue(talk));
        t++;
    }
}

static void unpack_resource(const cJSON* e) {
    GameState* gs = &g_game_state;
    if (gs->resource_count >= MAX_ENTITIES) { warn_overflow(OVF_RESOURCES); return; }
    EntityID id = serial_get_u32_default(e, "id", 0);
    int slot = claim_slot(id);
    if (0 > slot) return;
    gs->resources[gs->resource_count++] = id;

    write_data(slot, e, ENTITY_KIND_RESOURCE);
    write_transform(slot, e, true); /* static — no interpolation */
    write_appearance(slot, e, FETCH_P2);
    read_entity_state(slot, e, id, FETCH_P2, false);
}

/* Scenery never acts: data, transform, appearance and vitals are the whole
 * entity. Only a destructible obstacle has a max life. */
static void unpack_scenery(const cJSON* e) {
    GameState* gs = &g_game_state;
    if (MAX_OBJECTS <= gs->scenery_count) { warn_overflow(OVF_SCENERY); return; }
    EntityID id = serial_get_u32_default(e, "id", 0);
    int slot = claim_slot(id);
    if (0 > slot) return;
    gs->scenery[gs->scenery_count++] = id;
    write_data(slot, e, ENTITY_KIND_SCENERY);
    gs->vitals[slot] = (Vitals){
        .life     = serial_get_int_default(e, "life", 0),
        .max_life = serial_get_int_default(e, "maxLife", 0),
    };
    gs->mask[slot] |= COMP_VITALS;
    write_transform(slot, e, true);
    write_appearance(slot, e, FETCH_P2);
}

static void unpack_portal(const cJSON* e) {
    GameState* gs = &g_game_state;
    if (MAX_OBJECTS <= gs->portal_count) { warn_overflow(OVF_PORTALS); return; }
    EntityID id = serial_get_u32_default(e, "id", 0);
    int slot = claim_slot(id);
    if (0 > slot) return;
    gs->portals[gs->portal_count++] = id;
    write_data(slot, e, ENTITY_KIND_PORTAL);
    write_transform(slot, e, true);
    write_appearance(slot, e, FETCH_P2);
    Portal* p = &gs->portal[slot];
    serial_get_string(e, "targetMap", p->target_map, MAX_ID_LENGTH);
    const cJSON* cell = serial_get_object(e, "targetCell");
    p->target_cell = (Cell){ NULL != cell ? serial_get_int_default(cell, "x", 0) : 0,
                             NULL != cell ? serial_get_int_default(cell, "y", 0) : 0 };
    gs->mask[slot] |= COMP_PORTAL;
}

/* A container's stacks, {itemId, qty, slot} each. */
static void read_stacks(const cJSON* array, Stacks* out) {
    out->count = 0;
    const cJSON* item = NULL;
    cJSON_ArrayForEach(item, array) {
        if (VAULT_SLOTS_MAX <= out->count) break;
        ObjectLayerState* stack = &out->items[out->count++];
        memset(stack, 0, sizeof *stack);
        serial_get_string(item, "itemId", stack->item_id, sizeof stack->item_id);
        stack->quantity = serial_get_int_default(item, "qty", 0);
        stack->slot     = serial_get_int_default(item, "slot", -1);
    }
}

static HeldReason held_reason_of(const char* word) {
    if (0 == strcmp(word, "rebound")) return HELD_REBOUND;
    if (0 == strcmp(word, "no_room")) return HELD_NO_ROOM;
    if (0 == strcmp(word, "unsupported")) return HELD_UNSUPPORTED;
    if (0 == strcmp(word, "incompatible")) return HELD_INCOMPATIBLE;
    return HELD_UNKNOWN;
}

/* The owned stacks the world holds: the first HELD_STACKS_SHOWN rows, and the exact count. */
static void read_held(const cJSON* array) {
    g_local_player.held_count = cJSON_GetArraySize(array);
    int n = 0;
    const cJSON* item = NULL;
    cJSON_ArrayForEach(item, array) {
        if (HELD_STACKS_SHOWN <= n) break;
        HeldStack* stack = &g_local_player.held[n++];
        memset(stack, 0, sizeof *stack);
        char reason[16] = {0};
        serial_get_string(item, "itemId", stack->item_id, sizeof stack->item_id);
        serial_get_string(item, "cid", stack->cid, sizeof stack->cid);
        serial_get_string(item, "reason", reason, sizeof reason);
        stack->quantity = serial_get_int_default(item, "quantity", 0);
        stack->reason   = held_reason_of(reason);
    }
}

/* The party, the invites that wait for the player's answer and the invites it sent. */
static void read_party(const cJSON* e) {
    Party* party = &g_local_player.party;
    memset(party, 0, sizeof *party);
    const cJSON* party_json = serial_get_object(e, "party");
    if (NULL != party_json) {
        party->leader_id = serial_get_u32_default(party_json, "leaderId", 0);
        const cJSON* row = NULL;
        cJSON_ArrayForEach(row, serial_get_array(party_json, "members")) {
            if (CYBERIA_PARTY_MAX_MEMBERS <= party->member_count) break;
            PartyMember* m = &party->members[party->member_count++];
            m->id       = serial_get_u32_default(row, "id", 0);
            m->level    = serial_get_int_default(row, "level", 0);
            m->life     = serial_get_int_default(row, "life", 0);
            m->max_life = serial_get_int_default(row, "maxLife", 0);
            serial_get_string(row, "name", m->name, sizeof m->name);
            serial_get_string(row, "mapCode", m->map_code, sizeof m->map_code);
        }
    }
    g_local_player.invite_count = 0;
    const cJSON* row = NULL;
    cJSON_ArrayForEach(row, serial_get_array(e, "partyInvites")) {
        if (CYBERIA_PARTY_INVITES_MAX <= g_local_player.invite_count) break;
        PartyInvite* in = &g_local_player.invites[g_local_player.invite_count++];
        memset(in, 0, sizeof *in);
        in->from_id    = serial_get_u32_default(row, "fromId", 0);
        in->expires_at = GetTime() + serial_get_int_default(row, "expiresInMs", 0) / 1000.0;
        serial_get_string(row, "fromName", in->from_name, sizeof in->from_name);
    }
    g_local_player.sent_invite_count = 0;
    cJSON_ArrayForEach(row, serial_get_array(e, "partyInvitesSent")) {
        if (CYBERIA_PARTY_INVITES_MAX <= g_local_player.sent_invite_count) break;
        SentInvite* out = &g_local_player.sent_invites[g_local_player.sent_invite_count++];
        out->to_id      = serial_get_u32_default(row, "toId", 0);
        out->expires_at = GetTime() + serial_get_int_default(row, "expiresInMs", 0) / 1000.0;
    }
}

static void read_trade_offer(const cJSON* json, TradeOffer* out) {
    out->coins = serial_get_int_default(json, "coins", 0);
    out->confirmed = serial_get_bool_default(json, "confirmed", false);
    const cJSON* row = NULL;
    cJSON_ArrayForEach(row, serial_get_array(json, "items")) {
        if (CYBERIA_TRADE_OFFER_MAX_STACKS <= out->count) break;
        TradeLine* line = &out->items[out->count++];
        serial_get_string(row, "itemId", line->item_id, sizeof line->item_id);
        serial_get_string(row, "cid", line->cid, sizeof line->cid);
        line->quantity = serial_get_int_default(row, "quantity", 0);
        line->slot = serial_get_int_default(row, "slot", -1);
    }
}

/* The trade of the player, or none. */
static void read_trade(const cJSON* json) {
    static const char* const STATE_WORDS[] = { [TRADE_REQUESTED] = "requested", [TRADE_INCOMING] = "incoming",
                                               [TRADE_OPEN] = "open", [TRADE_COMMITTING] = "committing" };
    Trade* trade = &g_local_player.trade;
    memset(trade, 0, sizeof *trade);
    if (NULL == json) return;
    char word[16] = {0};
    serial_get_string(json, "state", word, sizeof word);
    for (int state = TRADE_REQUESTED; TRADE_COMMITTING >= state; state++) {
        if (0 == strcmp(word, STATE_WORDS[state])) trade->state = (TradeState)state;
    }
    trade->partner_id = serial_get_u32_default(json, "partnerId", 0);
    serial_get_string(json, "partnerName", trade->partner_name, sizeof trade->partner_name);
    trade->revision = serial_get_u32_default(json, "revision", 0);
    trade->expires_at = GetTime() + serial_get_int_default(json, "expiresInMs", 0) / 1000.0;
    read_trade_offer(serial_get_object(json, "mine"), &trade->mine);
    read_trade_offer(serial_get_object(json, "theirs"), &trade->theirs);
}

static void unpack_self(const cJSON* e, cyberia_frame_t frame, cyberia_input_seq_t consumed) {
    GameState* gs = &g_game_state;

    EntityID id = serial_get_u32_default(e, "id", 0);
    g_local_player.id = id;
    int slot = claim_slot(id);
    assert(0 <= slot);

    write_data(slot, e, ENTITY_KIND_PLAYER);
    write_transform(slot, e, false);
    write_appearance(slot, e, FETCH_P0);
    read_entity_state(slot, e, id, FETCH_P0, true);

    g_local_player.xp = serial_get_double_default(e, "xp", 0.0);
    g_local_player.level_xp = serial_get_double_default(e, "levelXp", 0.0);
    g_local_player.next_level_xp = serial_get_double_default(e, "nextLevelXp", 0.0);
    read_stats(e, "baseStats", g_local_player.base_stats);
    read_stats(e, "layerStats", g_local_player.layer_stats);
    read_stats(e, "temporaryStats", g_local_player.temporary_stats);
    g_local_player.coins            = serial_get_int_default(e, "coinBalance", 0);

    serial_get_string(e, "mapCode", g_local_player.map_code, MAX_ID_LENGTH);

    /* The server keeps no path once the walk ends. */
    self_motion_t state = { .pos = gs->transform[slot].pos, .direction = gs->motion[slot].direction };
    const cJSON* point = NULL;
    cJSON_ArrayForEach(point, serial_get_array(e, "path")) {
        if (MODE_WALKING != gs->motion[slot].mode || MAX_PATH_POINTS <= state.path_count) break;
        state.path[state.path_count++] = (Vector2){ serial_get_float_default(point, "x", 0.0f),
                                                    serial_get_float_default(point, "y", 0.0f) };
    }
    prediction_on_snapshot(&state, frame, consumed);

    /* Full inventory — every visible layer, active and inactive. Powers the
     * inventory bottom bar. */
    g_local_player.inventory_count = read_layers(e, "inventory", g_local_player.inventory,
                                                 INVENTORY_LAYERS_MAX, FETCH_P1);

    /* Every container of the player: the server copies the reconcile builds on. */
    g_local_player.vault_count = 0;
    const cJSON* vault_json = NULL;
    cJSON_ArrayForEach(vault_json, serial_get_array(e, "vaults")) {
        if (VAULTS_MAX <= g_local_player.vault_count) break;
        Vault* vault = &g_local_player.vaults[g_local_player.vault_count++];
        serial_get_string(vault_json, "actionCode", vault->action_code, sizeof vault->action_code);
        read_stacks(serial_get_array(vault_json, "items"), &vault->stacks);
    }
    read_stacks(serial_get_array(e, "storage"), &g_local_player.storage);
    read_party(e);

    /* The running assembly; the bar ends when the server's timer ends. */
    const cJSON* craft = serial_get_object(e, "craft");
    g_local_player.crafting = NULL != craft;
    read_trade(serial_get_object(e, "trade"));
    read_held(serial_get_array(e, "held"));
    char durability[16] = { 0 };
    serial_get_string(e, "durability", durability, sizeof durability);
    g_local_player.durability = 0 == strcmp(durability, "recovery") ? DURABILITY_RECOVERY
                              : 0 == strcmp(durability, "degraded") ? DURABILITY_DEGRADED
                              : 0 == strcmp(durability, "saving")   ? DURABILITY_SAVING
                                                                    : DURABILITY_SAVED;
    g_local_player.unsaved_ms = serial_get_int_default(e, "unsavedMs", 0);
    save_status_observe(g_local_player.durability, g_local_player.unsaved_ms);
    g_local_player.portal_fee = serial_get_int_default(e, "portalFee", 0);

    g_local_player.interaction_count = 0;
    const cJSON* row = NULL;
    cJSON_ArrayForEach(row, serial_get_array(e, "interaction")) {
        if (INTERACTIONS_MAX <= g_local_player.interaction_count) break;
        unpack_interaction(row, &g_local_player.interactions[g_local_player.interaction_count++]);
    }
    if (craft)
        modal_notification_set_assemble_remaining(
            (float)serial_get_int_default(craft, "remainingMs", 0) / 1000.0f);

    local_player_set_stasis(serial_get_bool_default(e, "frozen", false));
    local_player_set_status_icon(gs->data[slot].status_icon);

    /* Authoritative move speed — grid units per second. The server pushes it
     * every snapshot so prediction stays in lock-step with phaseMovement. */
    float move_speed = serial_get_float_default(e, "moveSpeed", 0.0f);
    if (move_speed > 0.0f) local_player_set_move_speed(move_speed);

    /* Authoritative action cooldown — the skill-trigger period. Movement is not
     * gated by it; keyboard steering paces its refresh by it. */
    int cooldown_ms = serial_get_int_default(e, "actionCooldownMs", 0);
    if (cooldown_ms > 0) local_player_set_action_cooldown((float)cooldown_ms / 1000.0f);
    local_player_set_action_ready_at(GetTime() + serial_get_int_default(e, "actionReadyInMs", 0) / 1000.0);
    local_player_set_consume_ready_at(GetTime() + serial_get_int_default(e, "consumeReadyInMs", 0) / 1000.0);
}

static void json_unpack_snapshot(const cJSON* payload) {
    char destination[MAX_ID_LENGTH] = {0};
    const cJSON* destination_self = serial_get_object(payload, "self");
    if (NULL != destination_self) serial_get_string(destination_self, "mapCode", destination, sizeof(destination));
    const bool map_changed = '\0' != destination[0] && 0 != strcmp(destination, g_local_player.map_code);
    if (map_changed) {
        fx_obstacle_burst_reset();
        if ('\0' != g_local_player.map_code[0]) fetch_deprioritize();
        fetch_event("destination_snapshot", destination, 0, 0);
    }
    GameState* gs = &g_game_state;

    /* Stamp the arrival time. The interpolator computes
     * t = (now - snapshot_time) * 1000 / interpolation_ms; without this write
     * t stays at 1.0 and entities teleport between snapshots. */
    gs->last_update_time = GetTime();

    /* Each snapshot re-lists everything in the area of interest. */

    s_overflow_warned = 0;
    s_stamp++;
    game_state_layer_pool_reset();
    gs->other_player_count = 0;
    gs->bot_count = 0;
    gs->resource_count = 0;
    gs->scenery_count = 0;
    gs->portal_count = 0;

    const cJSON* self = serial_get_object(payload, "self");
    if (NULL != self) unpack_self(self, serial_get_u32_default(payload, "frame", 0),
                                  serial_get_u32_default(payload, "inputConsumedThrough", 0));
    const cJSON* entity = NULL;
    cJSON_ArrayForEach(entity, serial_get_array(payload, "entities")) {
        char kind[16] = {0};
        if (0 != serial_get_string(entity, "kind", kind, sizeof(kind))) continue;
        if      (0 == strcmp(kind, "player"))   unpack_player(entity);
        else if (0 == strcmp(kind, "bot"))      unpack_bot(entity);
        else if (0 == strcmp(kind, "resource")) unpack_resource(entity);
        else if (0 == strcmp(kind, "scenery"))  unpack_scenery(entity);
        else if (0 == strcmp(kind, "portal"))   unpack_portal(entity);
    }
    free_absent_slots();

    fx_obstacle_burst_snapshot();
    audio_context_snapshot();
}

/* A world sound the server raised for everyone in reach. The id is an audio
 * LogicId; the map bindings decide what it plays. */
static void json_unpack_audio_event(const cJSON* payload) {
    char logic_event_id[64] = {0};
    serial_get_string(payload, "logicEventId", logic_event_id, sizeof(logic_event_id));
    /* The entity snapshot handles death because this event has no entity type. */
    if ('\0' != logic_event_id[0] && 0 != strcmp(logic_event_id, AUDIO_EVENT_DEATH))
        audio_event(logic_event_id);
}

static void json_unpack_combat_text(const cJSON* payload) {
    char kind[16] = {0};
    serial_get_string(payload, "kind", kind, sizeof(kind));
    /* `kind` mirrors game/snapshot.go: FCTDamage "damage", FCTRegen "regen", FCTXp "xp". */
    LocalFctEvent ev = {
        .world_x = serial_get_float_default(payload, "worldX", 0.0f),
        .world_y = serial_get_float_default(payload, "worldY", 0.0f),
        .value   = (uint32_t)serial_get_int_default(payload, "value", 0),
        .type    = 0 == strcmp(kind, "regen") ? FCT_TYPE_REGEN
                 : 0 == strcmp(kind, "xp")    ? FCT_TYPE_XP
                 : FCT_TYPE_DAMAGE,
    };
    local_player_fct_push(&ev);
    /* The authoritative collision signal: the server emits one of these wherever damage lands,
     * on any entity in view. Sounding it and bursting it here is what makes a hit audible and
     * visible when it is someone else being hit, not only when it is the player.
     *
     * Only damage sounds here. Regeneration is emitted constantly and in small amounts, so it is
     * sounded from the snapshot instead, once an entity's life actually crosses a milestone —
     * see audio_context.c. */
    if (FCT_TYPE_DAMAGE == ev.type) {
        audio_event(AUDIO_EVENT_HIT);
        fx_hit_burst_spawn((Vector2){ ev.world_x, ev.world_y });
    }
}

static void json_unpack_drop_collect(const cJSON* payload) {
    EntityID drop_id      = serial_get_u32_default(payload, "dropId", 0);
    EntityID collector_id = serial_get_u32_default(payload, "collectorId", 0);
    char item_id[MAX_ITEM_ID_LENGTH] = {0};
    serial_get_string(payload, "itemId", item_id, sizeof(item_id));
    fx_loot_push(drop_id, collector_id, item_id,
                 serial_get_float_default(payload, "worldX", 0.0f),
                 serial_get_float_default(payload, "worldY", 0.0f),
                 serial_get_bool_default(payload, "granted", true));
}

static void json_unpack_item_consume(const cJSON* payload) {
    EntityID entity_id = serial_get_u32_default(payload, "entityId", 0);
    char behavior[MAX_BEHAVIOR_LENGTH] = {0};
    serial_get_string(payload, "behavior", behavior, sizeof(behavior));
    fx_heal_aura_observe(behavior, entity_id);
}

static void json_unpack_drop_spawn(const cJSON* payload) {
    audio_event(AUDIO_EVENT_DROP);
    EntityID drop_id = serial_get_u32_default(payload, "dropId", 0);
    char item_id[MAX_ITEM_ID_LENGTH] = {0};
    serial_get_string(payload, "itemId", item_id, sizeof(item_id));
    fx_loot_note_spawn(drop_id,
                       serial_get_float_default(payload, "originX", 0.0f),
                       serial_get_float_default(payload, "originY", 0.0f),
                       serial_get_float_default(payload, "landingX", 0.0f),
                       serial_get_float_default(payload, "landingY", 0.0f),
                       item_id,
                       (uint16_t)serial_get_int_default(payload, "launchMs", 0));
}

/* ============================================================================
 * Cold-path handlers
 * ============================================================================ */

/* Write each entry of a server quest array into quest_progress_store. The
 * server sends code, status, the step and its objective counts only.
 * `granted` is NULL for the init_data seed, which records no change;
 * dialog_ack passes the granted code. */
static void store_quest_array(const cJSON* quests_json, const char* granted) {
    if (!quests_json || !cJSON_IsArray(quests_json)) return;
    const cJSON* q = NULL;
    cJSON_ArrayForEach(q, quests_json) {
        char code[64]        = {0};
        char status[32]      = {0};
        char active_step[160]= {0};
        char step_id[64]     = {0};
        serial_get_string(q, "code", code, sizeof(code));
        serial_get_string(q, "status", status, sizeof(status));
        serial_get_string(q, "activeStep", active_step, sizeof(active_step));
        serial_get_string(q, "stepId", step_id, sizeof(step_id));
        QuestSnapshot snapshot = { .code = code, .status = status, .active_step = active_step, .step_id = step_id,
                                   .rewards_held = serial_get_bool_default(q, "rewardsHeld", false) };
        const cJSON* o = NULL;
        cJSON_ArrayForEach(o, cJSON_GetObjectItem(q, "objectives")) {
            QuestObjectiveType type = quest_objective_type(json_str(o, "type"));
            if (QUEST_STEP_OBJECTIVES_MAX <= snapshot.objective_count || QUEST_OBJECTIVE_TYPE_COUNT == type) continue;
            QuestObjectiveProgress* objective = &snapshot.objectives[snapshot.objective_count++];
            *objective = (QuestObjectiveProgress){ .type = type, .current = json_int(o, "current", 0),
                                                   .required = json_int(o, "required", 1) };
            copy_str(objective->item_id, sizeof objective->item_id, json_str(o, "itemId"));
        }

        if (granted) quest_progress_store_apply(&snapshot, 0 == strcmp(code, granted));
        else quest_progress_store_upsert(&snapshot);
    }
}

/* The skills of each trigger item a skill map names replace the ones the
 * client holds for it; an empty list leaves the item with none. */
static void read_skill_map(const cJSON* skill_map) {
    const cJSON* entry = NULL;
    cJSON_ArrayForEach(entry, skill_map) {
        if (!entry->string || !cJSON_IsArray(entry)) continue;
        ui_state_drop_skills(entry->string);
        const cJSON* def_obj = NULL;
        cJSON_ArrayForEach(def_obj, entry) {
            if (!cJSON_IsObject(def_obj)) continue;
            UiSkillEntry se = {0};
            copy_str(se.trigger_item_id, sizeof se.trigger_item_id, entry->string);
            serial_get_string(def_obj, "logicEventId", se.logic_event_id, sizeof se.logic_event_id);
            serial_get_string(def_obj, "name", se.name, sizeof se.name);
            serial_get_string(def_obj, "description", se.description, sizeof se.description);
            serial_get_string(def_obj, "summonedEntityItemId", se.summoned_entity_item_id,
                              sizeof se.summoned_entity_item_id);
            se.held = serial_get_bool_default(def_obj, "held", false);
            ui_state_push_skill(&se);
        }
    }
}

static void json_unpack_init_data(const cJSON* payload) {
    LOG_INFO("[INIT_DATA] parsing payload\n");

    // Parse grid configuration — gameplay only (simulation contract).
    // cellSize / interpolationMs / cameraZoom are NOT here; the cyberia-server
    // never sends presentation. They are hydrated by presentation_runtime
    // once the /api/v1/cyberia-client-hints fetch settles.
    char session[16] = {0};
    serial_get_string(payload, "session", session, sizeof(session));
    g_local_player.account = 0 == strcmp(session, "account");
    session_gate_set_account(g_local_player.account);
    g_game_state.max_level = serial_get_int_default(payload, "maxLevel", 0);
    g_game_state.server_build[0] = '\0';
    serial_get_string(payload, "build", g_game_state.server_build, sizeof(g_game_state.server_build));
    g_game_state.grid_w = serial_get_int_default(payload, "gridW", 100);
    g_game_state.grid_h = serial_get_int_default(payload, "gridH", 100);
    g_game_state.aoi_radius = serial_get_float_default(payload, "aoiRadius", 15.0f);


    /* Skill map lives in ui_state — pure presentation lookup. */
    ui_state_reset();
    cJSON* skill_map_json = cJSON_GetObjectItem(payload, "skillMap");

    // Parse entity type defaults
    g_game_state.entity_defaults_count = 0;
    cJSON* entity_defaults_json = cJSON_GetObjectItem(payload, "entityDefaults");
    if (entity_defaults_json && cJSON_IsArray(entity_defaults_json)) {
        cJSON* etd = NULL;
        cJSON_ArrayForEach(etd, entity_defaults_json) {
            if (g_game_state.entity_defaults_count >= MAX_ENTITY_TYPES) break;
            EntityTypeDefault* d = &g_game_state.entity_defaults[g_game_state.entity_defaults_count];
            memset(d, 0, sizeof(EntityTypeDefault));
            serial_get_string(etd, "entityType", d->entity_type, sizeof(d->entity_type));

            // Parse liveItemIds array
            cJSON* live_arr = cJSON_GetObjectItem(etd, "liveItemIds");
            if (live_arr && cJSON_IsArray(live_arr)) {
                cJSON* item = NULL;
                cJSON_ArrayForEach(item, live_arr) {
                    if (d->live_item_id_count >= MAX_DEFAULT_ITEM_IDS) break;
                    if (cJSON_IsString(item)) {
                        strncpy(d->live_item_ids[d->live_item_id_count],
                                cJSON_GetStringValue(item), 127);
                        d->live_item_ids[d->live_item_id_count][127] = '\0';
                        d->live_item_id_count++;
                    }
                }
            }

            // Parse deadItemIds array
            cJSON* dead_arr = cJSON_GetObjectItem(etd, "deadItemIds");
            if (dead_arr && cJSON_IsArray(dead_arr)) {
                cJSON* item = NULL;
                cJSON_ArrayForEach(item, dead_arr) {
                    if (d->dead_item_id_count >= MAX_DEFAULT_ITEM_IDS) break;
                    if (cJSON_IsString(item)) {
                        strncpy(d->dead_item_ids[d->dead_item_id_count],
                                cJSON_GetStringValue(item), 127);
                        d->dead_item_ids[d->dead_item_id_count][127] = '\0';
                        d->dead_item_id_count++;
                    }
                }
            }

            if (d->entity_type[0] != '\0') g_game_state.entity_defaults_count++;
        }
    }

    /* Resolved dead-state (Fragmentation) ids — inventory labelling and
     * equip gating; the server rejects their activation regardless. */
    g_game_state.dead_item_id_count = 0;
    cJSON* dead_ids_json = cJSON_GetObjectItem(payload, "deadItemIds");
    if (dead_ids_json && cJSON_IsArray(dead_ids_json)) {
        cJSON* item = NULL;
        cJSON_ArrayForEach(item, dead_ids_json) {
            if (g_game_state.dead_item_id_count >= MAX_DEAD_ITEM_IDS) break;
            if (cJSON_IsString(item)) {
                char* dst = g_game_state.dead_item_ids[g_game_state.dead_item_id_count];
                strncpy(dst, cJSON_GetStringValue(item), 127);
                dst[127] = '\0';
                g_game_state.dead_item_id_count++;
            }
        }
    }

    read_skill_map(skill_map_json);

    /* Seed the Quest Journal store from the connect-time snapshot. Cleared
     * first so a reconnect repopulates cleanly. */
    quest_progress_store_reset();
    store_quest_array(cJSON_GetObjectItem(payload, "quests"), NULL);

    LOG_INFO("init_data parsed gridW=%d gridH=%d aoiRadius=%.1f entityDefaults=%d skills=%d",
             g_game_state.grid_w, g_game_state.grid_h, g_game_state.aoi_radius,
             g_game_state.entity_defaults_count, ui_state_skill_count());
    g_game_state.init_received = true;

    /* Signal interested modules (network FSM) that the handshake completed. */
    if (s_init_handler) { s_init_handler(serial_get_bool_default(payload, "adopted", false)); }
}

static void json_unpack_metadata(const cJSON* payload) {
    if (NULL == obj_layers_mgr_get()) {
        LOG_ERROR("[METADATA] ObjectLayersManager not initialized yet\n");
        return;
    }

    // Parse objectLayers: map of itemId → OL definition. These are the item facts the client
    // reasons about — type, stats, render descriptor — for everything the player could ever
    // equip or pick up. The join message carries every definition the world holds; a later
    // one carries only the definitions that changed or arrived since, with no other key.
    //
    // Their atlases are NOT fetched here. The definition set spans the whole instance (and, in
    // the fallback world, every canonical item), while a session draws a small subset of it:
    // fetching all of them costs two REST requests per item before the first frame, most of
    // them for sprites nothing on screen references. Atlases are scheduled from presence
    // instead — entity layers in read_layers, and anything else the moment it is first drawn
    // (obj_layers_mgr_get_atlas_texture).
    cJSON* ol_map = cJSON_GetObjectItem(payload, "objectLayers");
    int ol_count = 0;
    if (ol_map && cJSON_IsObject(ol_map)) {
        cJSON* entry = NULL;
        cJSON_ArrayForEach(entry, ol_map) {
            const char* item_id = entry->string;
            if (item_id && cJSON_IsObject(entry)) {
                populate_object_layer_from_json(item_id, entry);
                ol_count++;
            }
        }
    }
    /* A delta carries the skills of each definition it carries. */
    read_skill_map(cJSON_GetObjectItem(payload, "skillMap"));

    /* The economy the world runs: the join message and each change carry it whole. */
    economy_read(cJSON_GetObjectItem(payload, "economy"));

    // Parse instanceCode if provided: keys the Instance Map REST fetches.
    serial_get_string(payload, "instanceCode", g_game_state.instance_code,
                      sizeof(g_game_state.instance_code));

    /* The join and each world rebuild name the world revision: the map views read the content again. */
    if (cJSON_HasObjectItem(payload, "worldRevision")) instance_map_data_refresh();

    // Parse equipmentRules if provided
    cJSON* eq_rules = cJSON_GetObjectItem(payload, "equipmentRules");
    if (eq_rules && cJSON_IsObject(eq_rules)) {
        g_game_state.equipment_rules.active_item_type_count = 0;
        cJSON* ait = cJSON_GetObjectItem(eq_rules, "activeItemTypes");
        if (ait && cJSON_IsObject(ait)) {
            cJSON* entry = NULL;
            cJSON_ArrayForEach(entry, ait) {
                if (entry->string && cJSON_IsTrue(entry) &&
                    g_game_state.equipment_rules.active_item_type_count < MAX_ACTIVE_ITEM_TYPES) {
                    strncpy(g_game_state.equipment_rules.active_item_types[
                        g_game_state.equipment_rules.active_item_type_count],
                        entry->string, 31);
                    g_game_state.equipment_rules.active_item_type_count++;
                }
            }
        }
        cJSON* opt = cJSON_GetObjectItem(eq_rules, "onePerType");
        g_game_state.equipment_rules.one_per_type = (opt && cJSON_IsTrue(opt));
        cJSON* rs = cJSON_GetObjectItem(eq_rules, "requireSkin");
        g_game_state.equipment_rules.require_skin = (rs && cJSON_IsTrue(rs));
        LOG_INFO("[METADATA] Equipment rules: %d activeItemTypes, onePerType=%d, requireSkin=%d\n",
               g_game_state.equipment_rules.active_item_type_count,
               g_game_state.equipment_rules.one_per_type,
               g_game_state.equipment_rules.require_skin);
    }

    LOG_INFO("[METADATA] Cached %d ObjectLayers\n", ol_count);
}

/* The conversation a chat message files under: its channel, and for a direct
 * line the other player. */
static ChatConversation chat_conversation_of(const cJSON* payload, EntityID from_id) {
    static const char* const words[CHAT_CHANNEL_COUNT] = { "world", "party", "direct" };
    char word[16] = {0};
    serial_get_string(payload, "channel", word, sizeof(word));
    for (int c = 0; CHAT_CHANNEL_COUNT > c; c++) {
        if (0 != strcmp(word, words[c])) continue;
        if (CHAT_CHANNEL_DIRECT != c) return chat_channel((chat_channel_t)c);
        EntityID to_id = serial_get_u32_default(payload, "toId", 0);
        return chat_direct(from_id == g_local_player.id || 0 == from_id ? to_id : from_id);
    }
    return chat_channel(CHAT_CHANNEL_WORLD);
}

/* What a refusal reason from the social server means to the player. */
static const char* social_reason_text(const char* reason) {
    static const struct { const char* reason; const char* text; } TEXTS[] = {
        { "rate_limited", "Slow down: wait a moment, then send again." },
        { "unavailable",  "That player is out of reach." },
        { "no_party",     "You are not in a party." },
        { "in_party",     "That player is already in a party." },
        { "not_leader",   "Only the party leader can do that." },
        { "party_full",   "The party is full." },
        { "busy",         "That player has too many open invites." },
        { "invites_full", "Too many of your invites wait for an answer." },
        { "expired",      "That invite is no longer open." },
        { "not_member",   "That player is not in your party." },
        { "guest",        "Friends need a signed-in account." },
        { "self",         "You cannot befriend yourself." },
        { "exists",       "You are already friends, or a request waits." },
        { "too_long",     "Use 80 Unicode code points or fewer." },
        { "too_soon",     "Wait 10 seconds between status note changes." },
        { "invalid",      "Enter a valid status note." },
        { "limit",        "The friend list is full." },
        { "not_found",    "That request is no longer open." },
    };
    for (size_t i = 0; i < sizeof TEXTS / sizeof TEXTS[0]; i++) {
        if (0 == strcmp(reason, TEXTS[i].reason)) return TEXTS[i].text;
    }
    return "That cannot be done now.";
}

static void json_unpack_chat(const cJSON* payload) {
    EntityID from_id = serial_get_u32_default(payload, "fromId", 0);
    char name[PLAYER_NAME_BYTES] = {0};
    char avatar[CYBERIA_IDENTITY_PREVIEW_URL_MAX] = {0};
    char text[CHAT_LINE_BYTES] = {0};
    serial_get_string(payload, "fromName", name, sizeof(name));
    serial_get_string(payload, "fromAvatar", avatar, sizeof(avatar));
    serial_get_string(payload, "text", text, sizeof(text));
    if (0 == from_id || '\0' == text[0]) return;
    identity_icon_note(name, avatar);
    chat_log_push(chat_conversation_of(payload, from_id), from_id, name, text,
                  serial_get_double_default(payload, "atMs", 0.0), from_id == g_local_player.id);
}

/* A refused chat line: the sender reads why in the conversation it typed in. */
static void json_unpack_chat_ack(const cJSON* payload) {
    if (serial_get_bool_default(payload, "ok", false)) return;
    char reason[32] = {0};
    serial_get_string(payload, "reason", reason, sizeof(reason));
    chat_log_push_system(chat_conversation_of(payload, g_local_player.id), social_reason_text(reason));
}

/* A refused party request. The snapshot already shows that nothing changed. */
static void json_unpack_party_ack(const cJSON* payload) {
    if (serial_get_bool_default(payload, "ok", false)) return;
    char reason[32] = {0};
    serial_get_string(payload, "reason", reason, sizeof(reason));
    modal_notification_show("Party", social_reason_text(reason), (Color){ 230, 170, 110, 255 });
}

/* A note save or a refused friend action. */
static void json_unpack_friend_ack(const cJSON* payload) {
    char action[16] = {0};
    serial_get_string(payload, "action", action, sizeof action);
    if (0 == strcmp(action, "note")) player_panel_note_result(serial_get_bool_default(payload, "ok", false));
    if (serial_get_bool_default(payload, "ok", false)) return;
    if (0 == strcmp(action, "list")) g_local_player.friends_unavailable = true;
    char reason[32] = {0};
    serial_get_string(payload, "reason", reason, sizeof(reason));
    const char* message = 0 == strcmp(action, "note") && 0 == strcmp(reason, "unavailable")
                              ? "Could not save your note. Try again." : social_reason_text(reason);
    modal_notification_show("Social", message, (Color){ 230, 170, 110, 255 });
}

/* The whole friend list of the account, after each change. */
static void json_unpack_friends(const cJSON* payload) {
    g_local_player.friends_loaded = true;
    g_local_player.friends_unavailable = false;
    static const char* const STATUS_WORDS[] = { [FRIEND_ACCEPTED] = "friend", [FRIEND_INCOMING] = "incoming",
                                                [FRIEND_OUTGOING] = "outgoing" };
    g_local_player.friend_count = 0;
    serial_get_string(payload, "note", g_local_player.status_note, sizeof g_local_player.status_note);
    g_local_player.note_ready_at = GetTime() + serial_get_int_default(payload, "noteRetryInMs", 0) / 1000.0;
    const cJSON* row = NULL;
    cJSON_ArrayForEach(row, serial_get_array(payload, "friends")) {
        if (FRIENDS_LIST_MAX <= g_local_player.friend_count) break;
        char word[16] = {0};
        serial_get_string(row, "status", word, sizeof(word));
        int status = -1;
        for (int s = FRIEND_ACCEPTED; FRIEND_OUTGOING >= s; s++) {
            if (0 == strcmp(word, STATUS_WORDS[s])) status = s;
        }
        if (0 > status) continue;
        Friend* f = &g_local_player.friends[g_local_player.friend_count];
        memset(f, 0, sizeof *f);
        f->status = (FriendStatus)status;
        serial_get_string(row, "id", f->id, sizeof f->id);
        serial_get_string(row, "name", f->name, sizeof f->name);
        char avatar[CYBERIA_IDENTITY_PREVIEW_URL_MAX] = {0};
        serial_get_string(row, "avatar", avatar, sizeof avatar);
        identity_icon_note(f->name, avatar);
        serial_get_string(row, "world", f->world, sizeof f->world);
        serial_get_string(row, "mapCode", f->map_code, sizeof f->map_code);
        if (FRIEND_ACCEPTED == f->status) serial_get_string(row, "note", f->note, sizeof f->note);
        f->online     = serial_get_bool_default(row, "online", false);
        f->entity_id  = serial_get_u32_default(row, "entityId", 0);
        f->expires_at = GetTime() + serial_get_double_default(row, "expiresInMs", 0.0) / 1000.0;
        g_local_player.friend_count++;
    }
}

/* The server ended the session, or refused to start it. */
static void json_unpack_session_end(const cJSON* payload) {
    char reason[32] = {0};
    char travel_ticket[64] = {0};
    serial_get_string(payload, "reason", reason, sizeof(reason));
    const cJSON* travel = serial_get_object(payload, "travel");
    if (travel) serial_get_string(travel, "ticket", travel_ticket, sizeof(travel_ticket));
    LOG_INFO("[SESSION] ended: %s", reason);
    game_client_on_session_end(reason, travel_ticket);
}

/* The server queued no event after a batch it never got: the events from
 * `from` go again with the next flush. */
static void json_unpack_input_resend(const cJSON* payload) {
    client_event_resend((cyberia_input_seq_t)serial_get_u32_default(payload, "from", 0));
}

/* The next resume ticket of the account session. */
static void json_unpack_resume(const cJSON* payload) {
    char ticket[64] = {0};
    serial_get_string(payload, "ticket", ticket, sizeof(ticket));
    if ('\0' != ticket[0]) game_client_hold_resume(ticket);
}

/* shop_ack is the server verdict on a shop purchase. An accepted purchase needs
 * nothing here: the items arrive in the snapshot self block and the quantity
 * picker already launched their slot delivery. Only a rejection has to surface,
 * so the player learns why nothing came. */
static void json_unpack_shop_ack(const cJSON* payload) {
    if (serial_get_bool_default(payload, "ok", false)) return;

    char reason[64] = {0};
    serial_get_string(payload, "reason", reason, sizeof(reason));
    modal_notification_show("Purchase failed",
                            0 == strcmp(reason, "insufficient_funds") ? "Not enough currency."
                            : 0 == strcmp(reason, "inventory_full")   ? "Your bag has no free slot for it."
                                                                      : "The vendor refused the trade.",
                            (Color){ 210, 120, 110, 255 });
}

/* What a refusal of an economic operation means to the player. */
static const char* economy_reason_text(const char* reason) {
    static const struct { const char* reason; const char* text; } TEXTS[] = {
        { "not_allowed",        "That is not possible here." },
        { "not_offered",        "This terminal does not offer that." },
        { "policy_refused",     "The economy of this world does not allow that for this item." },
        { "market_closed",      "This market does not open here, or takes no new listing." },
        { "terms_refused",      "That price or duration is outside the terms of this market." },
        { "market_changed",     "The terms of this market changed. Check them and try again." },
        { "incompatible",       "This item runs another version here. It is safe in your inventory." },
        { "no_stack",           "That stack changed. Try again." },
        { "equipped",           "Equip another skin first: this one cannot come off." },
        { "insufficient_funds", "Not enough coins." },
        { "wallet_full",        "Your wallet cannot hold more coins." },
        { "inventory_full",     "Your bag has no free slot for it." },
        { "quote_changed",      "The price changed. Check it and try again." },
        { "quote_gone",         "That price is no longer offered. Ask again." },
        { "out_of_range",       "Too far away." },
        { "trading",            "Finish or cancel your trade first." },
        { "saving",             "Your last change is still saving. Try again." },
        { "listing_gone",       "That listing is gone: sold, taken back or ended." },
        { "listing_changed",    "That listing changed. Check it and try again." },
        { "bid_low",            "Another bid came first. Check the new lowest bid." },
        { "market_full",        "You have too many listings open." },
        { "claim_gone",         "Those claims were already collected." },
        { "unavailable",        "The market is not available now. Try again soon." },
        { "dead",               "Not while you wait to respawn." },
        { "busy",               "That player is busy." },
        { "guest_account",      "Guests and account players cannot trade, and guests use no market." },
        { "offer_changed",      "The offers changed. Check them and confirm again." },
        { "declined",           "The trade was declined." },
        { "expired",            "The trade request expired." },
        { "cancelled",          "The trade was cancelled." },
        { "left",               "The other player left." },
        { "unsaved",            "The trade could not be saved. Every item is back with its owner." },
        { "done",               "Trade complete. Each side received the other offer." },
    };
    for (size_t i = 0; sizeof(TEXTS) / sizeof(TEXTS[0]) > i; i++) {
        if (0 == strcmp(TEXTS[i].reason, reason)) return TEXTS[i].text;
    }
    return "The server refused it.";
}

static const Color C_REFUSED = { 210, 120, 110, 255 };

/* A drop waits for its save, turns public, or is refused. */
static void json_unpack_item_drop_ack(const cJSON* payload) {
    char reason[32] = {0}, status[16] = {0}, item_id[MAX_ITEM_ID_LENGTH] = {0};
    serial_get_string(payload, "reason", reason, sizeof reason);
    serial_get_string(payload, "status", status, sizeof status);
    serial_get_string(payload, "itemId", item_id, sizeof item_id);
    int quantity = serial_get_int_default(payload, "quantity", 0);
    if (!serial_get_bool_default(payload, "ok", false)) {
        modal_notification_show("Drop failed", economy_reason_text(reason), C_REFUSED);
        return;
    }
    char line[96 + MAX_ITEM_ID_LENGTH];
    snprintf(line, sizeof line, 0 == strcmp(status, "pending") ? "Dropping %d x %s: saving first." :
                                                                  "Dropped %d x %s. Anyone can take it.",
             quantity, item_id);
    chat_log_push_system(chat_channel(CHAT_CHANNEL_WORLD), line);
}

/* A market operation done, or why the market refused it. */
static void json_unpack_market_ack(const cJSON* payload) {
    char action[24] = {0}, reason[32] = {0};
    serial_get_string(payload, "action", action, sizeof action);
    serial_get_string(payload, "reason", reason, sizeof reason);
    if (!serial_get_bool_default(payload, "ok", false)) {
        if (0 == strcmp(action, "market_open")) market_panel_refused(economy_reason_text(reason));
        else modal_notification_show("Market", economy_reason_text(reason), C_REFUSED);
        return;
    }
    static const struct { const char* action; const char* line; } DONE[] = {
        { "market_list",    "Listed on the market." },
        { "market_cancel",  "Listing taken back." },
        { "market_buy",     "Bought on the market." },
        { "market_bid",     "Bid placed." },
        { "market_collect", "Market claims collected." },
    };
    for (size_t i = 0; sizeof DONE / sizeof DONE[0] > i; i++) {
        if (0 == strcmp(DONE[i].action, action)) chat_log_push_system(chat_channel(CHAT_CHANNEL_WORLD), DONE[i].line);
    }
}

/* A quote of a sale or a burn opens its quantity prompt, or tells why not. */
static void json_unpack_item_quote_ack(const cJSON* payload) {
    char reason[32] = {0}, kind[8] = {0};
    serial_get_string(payload, "reason", reason, sizeof reason);
    serial_get_string(payload, "kind", kind, sizeof kind);
    MerchantQuote quote = { .burn = 0 == strcmp(kind, "burn") };
    if (!serial_get_bool_default(payload, "ok", false)) {
        modal_notification_show(quote.burn ? "Burn failed" : "Sale failed", economy_reason_text(reason), C_REFUSED);
        return;
    }
    serial_get_string(payload, "quoteId", quote.quote_id, sizeof quote.quote_id);
    serial_get_string(payload, "itemId", quote.item_id, sizeof quote.item_id);
    quote.unit = serial_get_int_default(payload, "unit", 0);
    quote.max_quantity = serial_get_int_default(payload, "maxQuantity", 0);
    merchant_panel_quoted(&quote);
}

/* A sale or a burn at a terminal: the coins it paid or charged, or why not. */
static void json_unpack_stack_operation_ack(const cJSON* payload, bool burn) {
    char reason[32] = {0}, item_id[MAX_ITEM_ID_LENGTH] = {0};
    serial_get_string(payload, "reason", reason, sizeof reason);
    serial_get_string(payload, "itemId", item_id, sizeof item_id);
    if (!serial_get_bool_default(payload, "ok", false)) {
        char changed[96];
        const char* text = economy_reason_text(reason);
        if (0 == strcmp(reason, "quote_changed")) {
            snprintf(changed, sizeof changed, "The price is now %d coins each. Check it and try again.",
                     serial_get_int_default(payload, "unit", 0));
            text = changed;
        }
        modal_notification_show(burn ? "Burn failed" : "Sale failed", text, C_REFUSED);
        return;
    }
    char line[96 + MAX_ITEM_ID_LENGTH];
    int quantity = serial_get_int_default(payload, "quantity", 0);
    int coins = serial_get_int_default(payload, "coins", 0);
    if (burn) snprintf(line, sizeof line, "Burned %d x %s for %d coins.", quantity, item_id, coins);
    else      snprintf(line, sizeof line, "Sold %d x %s for %d coins.", quantity, item_id, coins);
    chat_log_push_system(chat_channel(CHAT_CHANNEL_WORLD), line);
}

static void json_unpack_craft_ack(const cJSON* payload) {
    if (serial_get_bool_default(payload, "ok", false)) return;
    char reason[32] = {0};
    serial_get_string(payload, "reason", reason, sizeof reason);
    modal_notification_close_assemble();
    modal_notification_show("Assembly failed", economy_reason_text(reason), C_REFUSED);
}

/* A trade request, offer or confirmation the server refused, or the end of a trade. */
static void json_unpack_trade_ack(const cJSON* payload) {
    char action[16] = {0}, reason[32] = {0};
    serial_get_string(payload, "action", action, sizeof action);
    serial_get_string(payload, "reason", reason, sizeof reason);
    bool ok = serial_get_bool_default(payload, "ok", false);
    if (0 == strcmp(action, "close")) {
        if (0 == strcmp(reason, "unsaved")) modal_notification_show("Trade failed", economy_reason_text(reason), C_REFUSED);
        else chat_log_push_system(chat_channel(CHAT_CHANNEL_WORLD), economy_reason_text(reason));
        return;
    }
    if (ok) return;
    modal_notification_show("Trade", economy_reason_text(reason), C_REFUSED);
}

/* A portal the player cannot pay keeps it; tell it once in a while. */
static void json_unpack_portal_ack(const cJSON* payload) {
    static double s_told_at = -100.0;
    if (serial_get_bool_default(payload, "ok", false) || GetTime() - s_told_at < 15.0) return;
    s_told_at = GetTime();
    char line[96];
    snprintf(line, sizeof line, "This portal costs %d coins. You hold %d.", serial_get_int_default(payload, "fee", 0),
             local_player_coins());
    modal_notification_show("Portal", line, C_REFUSED);
}

/* dialog_ack carries the quests a talk, kill, collect or accept changed. The
 * quest store records each change; the quest card reads it from there. */
static void json_unpack_dialog_ack(const cJSON* payload) {
    char quest_granted[64] = {0};
    serial_get_string(payload, "questGranted", quest_granted, sizeof(quest_granted));
    bool objectives_done = serial_get_bool_default(payload, "objectivesDone", false);

    store_quest_array(serial_get_array(payload, "quests"), quest_granted);

    if (quest_granted[0] != '\0')
        LOG_INFO("[DIALOG_ACK] quest granted: %s\n", quest_granted);
    if (objectives_done)
        LOG_INFO("[DIALOG_ACK] objective progressed\n");
}

/* ============================================================================
 * Dispatch
 * ============================================================================ */

void message_receive(const uint8_t* data, size_t len) {
    assert(data);
    cJSON* root = serial_unpack(data, len);
    if (!root) {
        LOG_ERROR("[MESSAGE] unpack failed (%zu bytes)\n", len);
        return;
    }

    char type[32] = {0};
    const cJSON* payload = serial_get_object(root, "payload");
    if (0 != serial_get_string(root, "type", type, sizeof(type)) || !payload) {
        LOG_ERROR("[MESSAGE] bad envelope\n");
        cJSON_Delete(root);
        return;
    }

    if      (0 == strcmp(type, "snapshot"))        json_unpack_snapshot(payload);
    else if (0 == strcmp(type, "combat_text"))     json_unpack_combat_text(payload);
    else if (0 == strcmp(type, "audio_event"))     json_unpack_audio_event(payload);
    else if (0 == strcmp(type, "drop_collect"))    json_unpack_drop_collect(payload);
    else if (0 == strcmp(type, "item_consume"))    json_unpack_item_consume(payload);
    else if (0 == strcmp(type, "drop_spawn"))      json_unpack_drop_spawn(payload);
    else if (0 == strcmp(type, "init_data"))       json_unpack_init_data(payload);
    else if (0 == strcmp(type, "metadata"))        json_unpack_metadata(payload);
    else if (0 == strcmp(type, "chat"))            json_unpack_chat(payload);
    else if (0 == strcmp(type, "chat_ack"))        json_unpack_chat_ack(payload);
    else if (0 == strcmp(type, "party_ack"))       json_unpack_party_ack(payload);
    else if (0 == strcmp(type, "friend_ack"))      json_unpack_friend_ack(payload);
    else if (0 == strcmp(type, "friends"))         json_unpack_friends(payload);
    else if (0 == strcmp(type, "session_end"))     json_unpack_session_end(payload);
    else if (0 == strcmp(type, "resume"))          json_unpack_resume(payload);
    else if (0 == strcmp(type, "input_resend"))    json_unpack_input_resend(payload);
    else if (0 == strcmp(type, "dialog_ack"))      json_unpack_dialog_ack(payload);
    else if (0 == strcmp(type, "shop_ack"))        json_unpack_shop_ack(payload);
    else if (0 == strcmp(type, "item_drop_ack"))   json_unpack_item_drop_ack(payload);
    else if (0 == strcmp(type, "item_quote_ack"))  json_unpack_item_quote_ack(payload);
    else if (0 == strcmp(type, "market"))          market_panel_view(payload);
    else if (0 == strcmp(type, "market_ack"))      json_unpack_market_ack(payload);
    else if (0 == strcmp(type, "item_sell_ack"))   json_unpack_stack_operation_ack(payload, false);
    else if (0 == strcmp(type, "item_burn_ack"))   json_unpack_stack_operation_ack(payload, true);
    else if (0 == strcmp(type, "craft_ack"))       json_unpack_craft_ack(payload);
    else if (0 == strcmp(type, "trade_ack"))       json_unpack_trade_ack(payload);
    else if (0 == strcmp(type, "portal_ack"))      json_unpack_portal_ack(payload);
    else LOG_ERROR("[MESSAGE] unknown type '%s'\n", type);

    cJSON_Delete(root);
}
