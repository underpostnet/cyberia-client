#include "message.h"

#include "config.h"
#include "game_state.h"
#include "fx/fx_death.h"
#include "fx/fx_heal_aura.h"
#include "fx/fx_hit_burst.h"
#include "fx/fx_level_up.h"
#include "fx/fx_obstacle_burst.h"
#include "domain/stat_contract_generated.h"
#include "util/serial.h"
#include "world_types.h"
#include "object_layer.h"
#include "object_layers_management.h"
#include "domain/camera.h"
#include "domain/local_player.h"
#include "domain/presentation_runtime.h"
#include "domain/audio_context.h"
#include "domain/self_motion.h"
#include "audio/audio.h"
#include "audio/audio_events.h"
#include "replication.h"
#include "notify_store.h"
#include "ui/floating_combat_text.h"
#include "fx/loot_fx.h"
#include "ui/item_slot_grid.h"
#include "ui/modal_interact.h"
#include "ui/modal_notification.h"
#include "ui/quest_progress_store.h"
#include "ui/ui_state.h"
#include "util/log.h"
#include "util/utils.h"

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
    g_game_state.player[slot] = (Player){
        .on_portal   = serial_get_bool_default(e, "onPortal", false),
        .portal_hold = serial_get_float_default(e, "portalHoldProgress", 0.0f),
    };
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
                                                 MAX_OBJECT_LAYERS, FETCH_P1);

    /* Every vault of the player: the server copy the reconcile builds on. */
    g_local_player.vault_count = 0;
    const cJSON* vault_json = NULL;
    cJSON_ArrayForEach(vault_json, serial_get_array(e, "vaults")) {
        if (VAULTS_MAX <= g_local_player.vault_count) break;
        Vault* vault = &g_local_player.vaults[g_local_player.vault_count++];
        memset(vault, 0, sizeof *vault);
        serial_get_string(vault_json, "actionCode", vault->action_code, sizeof vault->action_code);
        const cJSON* item = NULL;
        cJSON_ArrayForEach(item, serial_get_array(vault_json, "items")) {
            if (VAULT_SLOTS_MAX <= vault->count) break;
            ObjectLayerState* stack = &vault->items[vault->count++];
            serial_get_string(item, "itemId", stack->item_id, sizeof stack->item_id);
            stack->quantity = serial_get_int_default(item, "qty", 0);
        }
    }

    /* The running assembly; the bar ends when the server's timer ends. */
    const cJSON* craft = serial_get_object(e, "craft");
    g_local_player.crafting = NULL != craft;

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
    loot_fx_push(drop_id, collector_id, item_id,
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
    loot_fx_note_spawn(drop_id,
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
 * server sends code, status and progress only. `granted` is NULL for the
 * init_data seed, which records no change; dialog_ack passes the granted code. */
static void store_quest_array(const cJSON* quests_json, const char* granted) {
    if (!quests_json || !cJSON_IsArray(quests_json)) return;
    const cJSON* q = NULL;
    cJSON_ArrayForEach(q, quests_json) {
        char code[64]        = {0};
        char status[32]      = {0};
        char active_step[160]= {0};
        char objectives[160] = {0};
        serial_get_string(q, "code", code, sizeof(code));
        serial_get_string(q, "status", status, sizeof(status));
        serial_get_string(q, "activeStep", active_step, sizeof(active_step));
        serial_get_string(q, "objectivesText", objectives, sizeof(objectives));

        if (granted)
            quest_progress_store_apply(code, status, active_step, objectives,
                                       0 == strcmp(code, granted));
        else
            quest_progress_store_upsert(code, status, active_step, objectives);
    }
}

static void json_unpack_init_data(const cJSON* payload) {
    LOG_INFO("[INIT_DATA] parsing payload\n");

    // Parse grid configuration — gameplay only (simulation contract).
    // cellSize / interpolationMs / cameraZoom are NOT here; the cyberia-server
    // never sends presentation. They are hydrated by presentation_runtime
    // once the /api/v1/cyberia-client-hints fetch settles.
    serial_get_string(payload, "engineId", g_local_player.engine_id, sizeof g_local_player.engine_id);
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

    if (skill_map_json && cJSON_IsObject(skill_map_json)) {
        cJSON* entry = NULL;
        cJSON_ArrayForEach(entry, skill_map_json) {
            if (!entry->string || !cJSON_IsArray(entry)) continue;

            cJSON* def_obj = NULL;
            cJSON_ArrayForEach(def_obj, entry) {
                if (!cJSON_IsObject(def_obj)) continue;

                UiSkillEntry se = {0};
                strncpy(se.trigger_item_id, entry->string, MAX_ITEM_ID_LENGTH - 1);

                cJSON* id_json = cJSON_GetObjectItem(def_obj, "logicEventId");
                if (id_json && cJSON_IsString(id_json))
                    strncpy(se.logic_event_id, cJSON_GetStringValue(id_json), MAX_ITEM_ID_LENGTH - 1);

                cJSON* name_json = cJSON_GetObjectItem(def_obj, "name");
                if (name_json && cJSON_IsString(name_json))
                    strncpy(se.name, cJSON_GetStringValue(name_json), MAX_ITEM_ID_LENGTH - 1);

                cJSON* desc_json = cJSON_GetObjectItem(def_obj, "description");
                if (desc_json && cJSON_IsString(desc_json))
                    strncpy(se.description, cJSON_GetStringValue(desc_json), sizeof(se.description) - 1);

                cJSON* summon_json = cJSON_GetObjectItem(def_obj, "summonedEntityItemId");
                if (summon_json && cJSON_IsString(summon_json))
                    strncpy(se.summoned_entity_item_id, cJSON_GetStringValue(summon_json), MAX_ITEM_ID_LENGTH - 1);

                se.held = serial_get_bool_default(def_obj, "held", false);

                ui_state_push_skill(&se);
            }
        }
    }

    /* Seed the Quest Journal store from the connect-time snapshot. Cleared
     * first so a reconnect repopulates cleanly. */
    quest_progress_store_reset();
    store_quest_array(cJSON_GetObjectItem(payload, "quests"), NULL);

    LOG_INFO("init_data parsed gridW=%d gridH=%d aoiRadius=%.1f entityDefaults=%d skills=%d",
             g_game_state.grid_w, g_game_state.grid_h, g_game_state.aoi_radius,
             g_game_state.entity_defaults_count, ui_state_skill_count());
    g_game_state.init_received = true;

    /* Camera follows the presentation runtime's zoom; the underlying value
     * lives in domain/camera.c. Re-initialise with the viewport size so the
     * offset is correct when the player position lands. */
    if (camera_zoom() <= 0.0f) {
        camera_set_zoom(presentation_runtime_camera_zoom());
    }
    camera_init(GetScreenWidth(), GetScreenHeight());

    /* Signal interested modules (network FSM) that the handshake completed. */
    if (s_init_handler) { s_init_handler(); }
}

static void json_unpack_metadata(const cJSON* payload) {
    if (NULL == obj_layers_mgr_get()) {
        LOG_ERROR("[METADATA] ObjectLayersManager not initialized yet\n");
        return;
    }

    // Parse objectLayers: map of itemId → OL definition. These are the item facts the client
    // reasons about — type, stats, render descriptor — for everything the player could ever
    // equip or pick up, and they arrive in this one message at no extra request cost.
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

    // Parse instanceCode if provided: keys the Instance Map REST fetches.
    serial_get_string(payload, "instanceCode", g_game_state.instance_code,
                      sizeof(g_game_state.instance_code));

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

    LOG_INFO("[METADATA] Cached %d ObjectLayers, scheduled %d atlas REST fetches\n", ol_count, ol_count);
}

static void json_unpack_chat(const cJSON* payload) {
    EntityID from_id = serial_get_u32_default(payload, "from", 0);
    char text[256] = {0};
    serial_get_string(payload, "text", text, sizeof(text));
    /* TODO: show the sender name. The client holds no player names yet. */
    char sender[16];
    snprintf(sender, sizeof sender, "%u", from_id);
    if (0 != from_id && text[0]) notify_store_push(from_id, sender, text, false);
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
                            0 == strcmp(reason, "insufficient_funds")
                                ? "Not enough currency."
                                : "The vendor refused the trade.",
                            (Color){ 210, 120, 110, 255 });
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
    else if (0 == strcmp(type, "dialog_ack"))      json_unpack_dialog_ack(payload);
    else if (0 == strcmp(type, "shop_ack"))        json_unpack_shop_ack(payload);
    else LOG_ERROR("[MESSAGE] unknown type '%s'\n", type);

    cJSON_Delete(root);
}
