#include "message.h"

#include "config.h"
#include "game_state.h"
#include "util/serial.h"
#include "world_types.h"
#include "object_layer.h"
#include "object_layers_management.h"
#include "domain/camera.h"
#include "domain/local_player.h"
#include "domain/presentation_runtime.h"
#include "js/interact_bridge.h"
#include "replication.h"
#include "notification.h"
#include "notify_store.h"
#include "ui/floating_combat_text.h"
#include "ui/loot_fx.h"
#include "ui/item_slot_grid.h"
#include "ui/modal_interact.h"
#include "ui/modal_notification.h"
#include "ui/quest_cache.h"
#include "ui/quest_progress_store.h"
#include "ui/ui_state.h"
#include "util/log.h"

#include <cJSON.h>
#include <raylib.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static MessageInitHandler s_init_handler = NULL;

void message_set_init_handler(MessageInitHandler handler) {
    s_init_handler = handler;
}

/* ============================================================================
 * Previous-position snapshot — keeps interpolation across snapshot resets
 * ============================================================================
 *
 * json_unpack_snapshot copies gs->bots / gs->other_players into these buffers
 * BEFORE it resets the counters, so each entity can recover its prior server
 * position for a smooth lerp.
 */

typedef struct { char id[MAX_ID_LENGTH]; Vector2 pos_server; } PrevPos;

static PrevPos s_prev_bots[MAX_ENTITIES];
static int     s_prev_bot_count = 0;
static PrevPos s_prev_players[MAX_ENTITIES];
static int     s_prev_player_count = 0;

/* lookup_prev_server_pos returns the previous server position for `id` if it
 * appeared in the prior snapshot, else `fallback` — so a first appearance or a
 * post-reconnect entity keeps pos_prev == pos_server and does not jump. */
static Vector2 lookup_prev_server_pos(const PrevPos* arr, int n,
                                      const char* id, Vector2 fallback) {
    for (int k = 0; k < n; k++) {
        if (0 == strcmp(arr[k].id, id)) return arr[k].pos_server;
    }
    return fallback;
}

void message_reset_prev_snapshots(void) {
    s_prev_bot_count = 0;
    s_prev_player_count = 0;
}

/* ============================================================================
 * Snapshot readers
 * ============================================================================ */

static int read_layers(const cJSON* owner, const char* key,
                       ObjectLayerState* layers, int max_layers) {
    const cJSON* arr = serial_get_array(owner, key);
    int n = 0;
    const cJSON* item = NULL;
    cJSON_ArrayForEach(item, arr) {
        if (n >= max_layers) break;
        serial_get_string(item, "itemId", layers[n].item_id, MAX_ITEM_ID_LENGTH);
        layers[n].active   = serial_get_bool_default(item, "active", true);
        layers[n].quantity = serial_get_int_default(item, "quantity", 0);
        n++;
    }
    return n;
}

static Vector2 read_pos(const cJSON* e) {
    return (Vector2){ serial_get_float_default(e, "posX", 0.0f),
                      serial_get_float_default(e, "posY", 0.0f) };
}

/* read_entity_state fills every field an active entity shares. Position is the
 * caller's job — interpolation differs per entity kind. */
static void read_entity_state(const cJSON* e, EntityState* base) {
    GameState* gs = &g_game_state;
    base->dims = (Vector2){ serial_get_float_default(e, "dimW", 0.0f),
                            serial_get_float_default(e, "dimH", 0.0f) };
    base->direction   = (Direction)serial_get_int_default(e, "direction", 0);
    base->mode        = (ObjectLayerMode)serial_get_int_default(e, "mode", 0);
    base->life        = serial_get_float_default(e, "life", 0.0f);
    base->max_life    = serial_get_float_default(e, "maxLife", 0.0f);
    base->respawn_in  = serial_get_float_default(e, "respawnIn", 0.0f);
    base->stats_sum   = serial_get_int_default(e, "statsSum", 0);
    base->status_icon = (uint8_t)serial_get_int_default(e, "statusIcon", 0);
    base->object_layer_count = read_layers(e, "objectLayers", base->object_layers, MAX_OBJECT_LAYERS);
    base->last_update   = gs->last_update_time;
    base->snapshot_time = gs->last_update_time;
}

static void unpack_player(const cJSON* e) {
    GameState* gs = &g_game_state;
    char id[MAX_ID_LENGTH] = {0};
    serial_get_string(e, "id", id, sizeof(id));

    int idx = -1;
    for (int i = 0; i < gs->other_player_count; i++) {
        if (0 == strcmp(gs->other_players[i].base.id, id)) { idx = i; break; }
    }
    if (idx < 0) {
        if (gs->other_player_count >= MAX_ENTITIES) return;
        idx = gs->other_player_count++;
        memset(&gs->other_players[idx], 0, sizeof(PlayerState));
        strncpy(gs->other_players[idx].base.id, id, MAX_ID_LENGTH - 1);
    }

    PlayerState* p = &gs->other_players[idx];
    read_entity_state(e, &p->base);
    Vector2 incoming = read_pos(e);
    /* TELEPORTING is a one-snapshot signal that the entity jumped (portal), so
     * a lerp from the old position would sweep across the map. */
    p->base.pos_prev = (MODE_TELEPORTING == p->base.mode)
        ? incoming
        : lookup_prev_server_pos(s_prev_players, s_prev_player_count, id, incoming);
    p->base.pos_server = incoming;
}

static void unpack_bot(const cJSON* e) {
    GameState* gs = &g_game_state;
    char id[MAX_ID_LENGTH] = {0};
    serial_get_string(e, "id", id, sizeof(id));

    int idx = -1;
    for (int i = 0; i < gs->bot_count; i++) {
        if (0 == strcmp(gs->bots[i].base.id, id)) { idx = i; break; }
    }
    if (idx < 0) {
        if (gs->bot_count >= MAX_ENTITIES) return;
        idx = gs->bot_count++;
        memset(&gs->bots[idx], 0, sizeof(BotState));
        strncpy(gs->bots[idx].base.id, id, MAX_ID_LENGTH - 1);
    }

    BotState* b = &gs->bots[idx];
    read_entity_state(e, &b->base);
    Vector2 incoming = read_pos(e);
    b->base.pos_prev = (MODE_TELEPORTING == b->base.mode)
        ? incoming
        : lookup_prev_server_pos(s_prev_bots, s_prev_bot_count, id, incoming);
    b->base.pos_server = incoming;

    serial_get_string(e, "behavior", b->behavior, MAX_BEHAVIOR_LENGTH);
    serial_get_string(e, "casterId", b->caster_id, MAX_ID_LENGTH);
    serial_get_string(e, "actionCode", b->action_code, MAX_ID_LENGTH);
    b->interaction_flags = (uint8_t)serial_get_int_default(e, "interactionFlags", 0);

    b->quest_code_count = 0;
    const cJSON* code = NULL;
    cJSON_ArrayForEach(code, serial_get_array(e, "questCodes")) {
        if (b->quest_code_count >= BOT_QUEST_CODES_MAX) break;
        if (!cJSON_IsString(code)) continue;
        strncpy(b->quest_codes[b->quest_code_count], cJSON_GetStringValue(code), MAX_ID_LENGTH - 1);
        b->quest_codes[b->quest_code_count][MAX_ID_LENGTH - 1] = '\0';
        b->quest_code_count++;
    }

    /* Parallel to quest_codes: entry i is non-empty when quest_codes[i] has an
     * open talk objective this NPC answers. */
    for (int i = 0; i < BOT_QUEST_CODES_MAX; i++) b->quest_talk_dialog_codes[i][0] = '\0';
    int t = 0;
    const cJSON* talk = NULL;
    cJSON_ArrayForEach(talk, serial_get_array(e, "questTalkDialogCodes")) {
        if (t >= BOT_QUEST_CODES_MAX) break;
        if (cJSON_IsString(talk)) {
            strncpy(b->quest_talk_dialog_codes[t], cJSON_GetStringValue(talk), MAX_ID_LENGTH - 1);
            b->quest_talk_dialog_codes[t][MAX_ID_LENGTH - 1] = '\0';
        }
        t++;
    }
}

static void unpack_resource(const cJSON* e) {
    GameState* gs = &g_game_state;
    if (gs->resource_count >= MAX_ENTITIES) return;
    BotState* res = &gs->resources[gs->resource_count++];
    memset(res, 0, sizeof(BotState));
    serial_get_string(e, "id", res->base.id, MAX_ID_LENGTH);

    read_entity_state(e, &res->base);
    res->base.pos_server = read_pos(e);
    res->base.pos_prev   = res->base.pos_server;
    res->base.interp_pos = res->base.pos_server; /* static — no interpolation */
    strncpy(res->behavior, "resource", MAX_BEHAVIOR_LENGTH - 1);
}

/* passive_slot claims the next free slot for a passive world object and tags
 * its type. Returns NULL when that array is full. */
static WorldObject* passive_slot(const char* type) {
    GameState* gs = &g_game_state;
    WorldObject* o = NULL;
    ObjectLayerType kind = OBJECT_LAYER_TYPE_UNKNOWN;

    if (0 == strcmp(type, "floor")) {
        if (gs->floor_count >= MAX_OBJECTS) return NULL;
        o = &gs->floors[gs->floor_count++];
        kind = OBJECT_LAYER_TYPE_FLOOR;
    } else if (0 == strcmp(type, "obstacle")) {
        if (gs->obstacle_count >= MAX_OBJECTS) return NULL;
        o = &gs->obstacles[gs->obstacle_count++];
        kind = OBJECT_LAYER_TYPE_OBSTACLE;
    } else if (0 == strcmp(type, "portal")) {
        if (gs->portal_count >= MAX_OBJECTS) return NULL;
        o = &gs->portals[gs->portal_count++];
        kind = OBJECT_LAYER_TYPE_PORTAL;
    } else if (0 == strcmp(type, "foreground")) {
        if (gs->foreground_count >= MAX_OBJECTS) return NULL;
        o = &gs->foregrounds[gs->foreground_count++];
        kind = OBJECT_LAYER_TYPE_FOREGROUND;
    } else if (0 == strcmp(type, "static")) {
        if (gs->static_count >= MAX_ENTITIES) return NULL;
        o = &gs->statics[gs->static_count++];
        kind = OBJECT_LAYER_TYPE_STATIC;
    } else {
        return NULL;
    }

    memset(o, 0, sizeof(WorldObject));
    o->type_kind = kind;
    strncpy(o->type, type, MAX_TYPE_LENGTH - 1);
    return o;
}

/* unpack_passive reads any non-acting entity: floors, obstacles, portals,
 * foregrounds and statics. Only portals use the target fields; the rest read
 * them as zero. */
static void unpack_passive(const cJSON* e, const char* type) {
    WorldObject* o = passive_slot(type);
    if (!o) return;
    serial_get_string(e, "id", o->id, MAX_ID_LENGTH);
    o->pos  = read_pos(e);
    o->dims = (Vector2){ serial_get_float_default(e, "dimW", 0.0f),
                         serial_get_float_default(e, "dimH", 0.0f) };
    o->status_icon = (uint8_t)serial_get_int_default(e, "statusIcon", 0);
    serial_get_string(e, "targetMapCode", o->target_map_code, MAX_ID_LENGTH);
    o->target_cell_x = serial_get_int_default(e, "targetCellX", 0);
    o->target_cell_y = serial_get_int_default(e, "targetCellY", 0);
    o->object_layer_count = read_layers(e, "objectLayers", o->object_layers, MAX_OBJECT_LAYERS);
}

static void unpack_self(const cJSON* e) {
    GameState* gs = &g_game_state;
    PlayerState* p = &gs->player;

    serial_get_string(e, "id", p->base.id, MAX_ID_LENGTH);
    strncpy(gs->player_id, p->base.id, MAX_ID_LENGTH - 1);

    read_entity_state(e, &p->base);
    p->base.pos_prev   = p->base.pos_server;
    p->base.pos_server = read_pos(e);

    gs->sum_stats_limit  = serial_get_int_default(e, "sumStatsLimit", 0);
    gs->active_stats_sum = serial_get_int_default(e, "activeStatsSum", 0);
    gs->player_coins     = serial_get_int_default(e, "coinBalance", 0);

    serial_get_string(e, "mapCode", p->map_code, MAX_ID_LENGTH);

    p->path_count = 0;
    const cJSON* point = NULL;
    cJSON_ArrayForEach(point, serial_get_array(e, "path")) {
        if (p->path_count >= MAX_PATH_POINTS) break;
        p->path[p->path_count].x = serial_get_float_default(point, "x", 0.0f);
        p->path[p->path_count].y = serial_get_float_default(point, "y", 0.0f);
        p->path_count++;
    }
    p->target_pos = (Vector2){ (float)serial_get_int_default(e, "targetPosX", 0),
                               (float)serial_get_int_default(e, "targetPosY", 0) };

    /* Full inventory — every visible layer, active and inactive. Powers the
     * inventory bottom bar. */
    gs->full_inventory_count = read_layers(e, "inventory", gs->full_inventory, MAX_OBJECT_LAYERS);

    local_player_set_frozen(serial_get_bool_default(e, "frozen", false));
    local_player_set_status_icon(p->base.status_icon);

    /* Authoritative move speed — grid units per second. The server pushes it
     * every snapshot so prediction stays in lock-step with phaseMovement. */
    float move_speed = serial_get_float_default(e, "moveSpeed", 0.0f);
    if (move_speed > 0.0f) local_player_set_move_speed(move_speed);

    /* The HUD renders the hold bar only while on_portal is set. */
    local_player_set_portal_hold(serial_get_bool_default(e, "onPortal", false),
                                 serial_get_float_default(e, "portalHoldProgress", 0.0f));
}

static void json_unpack_snapshot(const cJSON* payload) {
    GameState* gs = &g_game_state;

    /* Feed the session bookkeeping so prediction and interpolation align to
     * the authoritative tick stream. */
    session_on_snapshot(serial_get_u32_default(payload, "tick", 0),
                        serial_get_u32_default(payload, "ack", 0));

    /* Stamp the arrival time. The interpolator computes
     * t = (now - snapshot_time) * 1000 / interpolation_ms; without this write
     * t stays at 1.0 and entities teleport between snapshots. */
    gs->last_update_time = GetTime();

    /* Keep the current positions so the readers can recover pos_prev. */
    s_prev_bot_count = gs->bot_count;
    for (int i = 0; i < s_prev_bot_count; i++) {
        memcpy(s_prev_bots[i].id, gs->bots[i].base.id, MAX_ID_LENGTH);
        s_prev_bots[i].pos_server = gs->bots[i].base.pos_server;
    }
    s_prev_player_count = gs->other_player_count;
    for (int i = 0; i < s_prev_player_count; i++) {
        memcpy(s_prev_players[i].id, gs->other_players[i].base.id, MAX_ID_LENGTH);
        s_prev_players[i].pos_server = gs->other_players[i].base.pos_server;
    }

    /* Each snapshot re-lists everything in the area of interest. */
    gs->other_player_count = 0;
    gs->bot_count = 0;
    gs->resource_count = 0;
    gs->obstacle_count = 0;
    gs->foreground_count = 0;
    gs->static_count = 0;
    gs->portal_count = 0;
    gs->floor_count = 0;

    const cJSON* entity = NULL;
    cJSON_ArrayForEach(entity, serial_get_array(payload, "entities")) {
        char type[16] = {0};
        if (0 != serial_get_string(entity, "type", type, sizeof(type))) continue;
        if      (0 == strcmp(type, "player"))   unpack_player(entity);
        else if (0 == strcmp(type, "bot"))      unpack_bot(entity);
        else if (0 == strcmp(type, "resource")) unpack_resource(entity);
        else                                    unpack_passive(entity, type);
    }

    const cJSON* self = serial_get_object(payload, "self");
    if (self) unpack_self(self);

    /* The authoritative self position is fresh — reconcile prediction. */
    prediction_reconcile();
}

static void json_unpack_combat_text(const cJSON* payload) {
    char kind[16] = {0};
    serial_get_string(payload, "kind", kind, sizeof(kind));
    /* `kind` mirrors game/snapshot.go: FCTDamage "damage", FCTRegen "regen". */
    LocalFctEvent ev = {
        .world_x = serial_get_float_default(payload, "worldX", 0.0f),
        .world_y = serial_get_float_default(payload, "worldY", 0.0f),
        .value   = (uint32_t)serial_get_int_default(payload, "value", 0),
        .type    = (0 == strcmp(kind, "regen")) ? FCT_TYPE_REGEN : FCT_TYPE_DAMAGE,
    };
    local_player_fct_push(&ev);
}

static void json_unpack_drop_collect(const cJSON* payload) {
    char drop_id[MAX_ID_LENGTH] = {0};
    char collector_id[MAX_ID_LENGTH] = {0};
    char item_id[MAX_ITEM_ID_LENGTH] = {0};
    serial_get_string(payload, "dropId", drop_id, sizeof(drop_id));
    serial_get_string(payload, "collectorId", collector_id, sizeof(collector_id));
    serial_get_string(payload, "itemId", item_id, sizeof(item_id));
    loot_fx_push(drop_id, collector_id, item_id,
                 serial_get_float_default(payload, "worldX", 0.0f),
                 serial_get_float_default(payload, "worldY", 0.0f));
}

static void json_unpack_drop_spawn(const cJSON* payload) {
    char drop_id[MAX_ID_LENGTH] = {0};
    char item_id[MAX_ITEM_ID_LENGTH] = {0};
    serial_get_string(payload, "dropId", drop_id, sizeof(drop_id));
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

/* Upsert each entry of a server quest snapshot array into the local store.
 * Shared by init_data (initial snapshot) and dialog_ack (live updates).
 * The server only sends authoritative data (code, status, progress);
 * metadata (title, description, rewards) is fetched asynchronously from
 * the engine REST endpoint /api/cyberia-quest/:code via quest_cache. */
static void upsert_quest_array(const cJSON* quests_json) {
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

        /* Store authoritative data — title/description will be populated
         * lazily by quest_cache when its REST fetch completes.
         * Use an empty title as placeholder for the quest_progress_store upsert. */
        quest_progress_store_upsert(code, "", "", status, active_step, objectives);

        /* Kick off async metadata fetch from engine REST. */
        quest_cache_fetch(code);
    }
}

static void json_unpack_init_data(const cJSON* payload) {
    LOG_INFO("[INIT_DATA] parsing payload\n");

    /* New session boundary — drop any stale prev-position snapshot from a
     * prior server lifetime so post-restart UUIDs don't interpolate from
     * origin. Cheap; safe to call on every init_data. */
    message_reset_prev_snapshots();

    // Parse grid configuration — gameplay only (simulation contract).
    // cellSize / interpolationMs / cameraZoom are NOT here; the cyberia-server
    // never sends presentation. They are hydrated by presentation_runtime
    // once the /api/cyberia-client-hints fetch settles.
    g_game_state.grid_w = serial_get_int_default(payload, "gridW", 100);
    g_game_state.grid_h = serial_get_int_default(payload, "gridH", 100);
    g_game_state.aoi_radius = serial_get_float_default(payload, "aoiRadius", 15.0f);

    g_game_state.sum_stats_limit = serial_get_int_default(payload, "sumStatsLimit", 9999);

    /* Skill map lives in ui_state — pure presentation lookup. */
    ui_state_clear_skills();
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

            // Parse dropItemIds array
            cJSON* drop_arr = cJSON_GetObjectItem(etd, "dropItemIds");
            if (drop_arr && cJSON_IsArray(drop_arr)) {
                cJSON* item = NULL;
                cJSON_ArrayForEach(item, drop_arr) {
                    if (d->drop_item_id_count >= MAX_DEFAULT_ITEM_IDS) break;
                    if (cJSON_IsString(item)) {
                        strncpy(d->drop_item_ids[d->drop_item_id_count],
                                cJSON_GetStringValue(item), 127);
                        d->drop_item_ids[d->drop_item_id_count][127] = '\0';
                        d->drop_item_id_count++;
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

                ui_state_push_skill(&se);
            }
        }
    }

    /* Seed the Quest Journal store from the connect-time snapshot. Cleared
     * first so a reconnect repopulates cleanly. */
    quest_progress_store_reset();
    upsert_quest_array(cJSON_GetObjectItem(payload, "quests"));

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

    // Parse objectLayers: map of itemId → OL metadata, then schedule
    // atlas sprite sheet REST fetch for each item (two requests per itemKey:
    // 1. GET /api/atlas-sprite-sheet/metadata/:itemKey  → cache frames + dims
    // 2. GET /api/atlas-sprite-sheet/blob/:itemKey      → cache PNG texture)
    cJSON* ol_map = cJSON_GetObjectItem(payload, "objectLayers");
    int ol_count = 0;
    if (ol_map && cJSON_IsObject(ol_map)) {
        cJSON* entry = NULL;
        cJSON_ArrayForEach(entry, ol_map) {
            const char* item_id = entry->string;
            if (item_id && cJSON_IsObject(entry)) {
                populate_object_layer_from_json(item_id, entry);
                obj_layers_mgr_schedule_atlas_fetch(item_id);
                ol_count++;
            }
        }
    }

    // Parse apiBaseUrl if provided: the server-forwarded public Content
    // Authority origin (never the internal cluster address).
    char api_url[256] = {0};
    if (serial_get_string(payload, "apiBaseUrl", api_url, sizeof(api_url)) == 0 && api_url[0] != '\0') {
        js_init_engine_api(api_url);
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
    char from_id[64] = {0};
    char text[256] = {0};
    serial_get_string(payload, "from", from_id, sizeof(from_id));
    serial_get_string(payload, "text", text, sizeof(text));
    if (from_id[0] && text[0]) {
        notify_store_push(from_id, from_id, text);
        notification_push(NOTIF_CHAT, from_id);
        js_interact_overlay_receive_chat(from_id, from_id, text);
    }
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

/* storage_state is the authoritative vault, pushed after open and after every
 * mutation. The client adopts it wholesale over its optimistic view, so a
 * rejected drag self-heals without a bespoke error path. */
static void json_unpack_storage_state(const cJSON* payload) {
    char entity_id[64] = {0};
    serial_get_string(payload, "entityId", entity_id, sizeof(entity_id));
    int capacity = serial_get_int_default(payload, "capacity", 0);

    ObjectLayerState slots[ITEM_SLOT_GRID_MAX_SLOTS];
    int indices[ITEM_SLOT_GRID_MAX_SLOTS];
    int count = 0;

    cJSON* arr = serial_get_array(payload, "slots");
    if (arr) {
        const cJSON* row = NULL;
        cJSON_ArrayForEach(row, arr) {
            if (count >= ITEM_SLOT_GRID_MAX_SLOTS) break;
            memset(&slots[count], 0, sizeof(slots[count]));
            if (0 != serial_get_string(row, "itemId", slots[count].item_id,
                                       sizeof(slots[count].item_id))) continue;
            slots[count].quantity = serial_get_int_default(row, "qty", 0);
            indices[count] = serial_get_int_default(row, "index", 0);
            count++;
        }
    }
    modal_interact_storage_state(entity_id, capacity, slots, indices, count);
}

/* craft_ack answers the start of an assembly. Accepted: the assembly modal
 * adopts the server's authoritative duration so its progress bar tracks the
 * real timer rather than the cached recipe. Rejected: the modal is torn down
 * and the reason surfaced, since nothing was consumed. */
static void json_unpack_craft_ack(const cJSON* payload) {
    if (serial_get_bool_default(payload, "ok", false)) {
        modal_notification_set_assemble_duration(
            (float)serial_get_int_default(payload, "craftTimeMs", 0) / 1000.0f);
        return;
    }

    char reason[64] = {0};
    serial_get_string(payload, "reason", reason, sizeof(reason));
    modal_notification_abort_assemble();
    modal_notification_show("Synthesis failed",
                            0 == strcmp(reason, "missing_ingredients") ? "Missing components."
                            : 0 == strcmp(reason, "already_assembling") ? "Already assembling."
                            : "The assembler rejected the schematic.",
                            (Color){ 210, 120, 110, 255 });
}

/* dialog_ack is notify-only: it updates the local quest_progress_store from the
 * affected quest entries the server attached. questGranted / objectivesDone gate
 * an optional notification; no simulation state is touched here. */
static void json_unpack_dialog_ack(const cJSON* payload) {
    char quest_granted[64] = {0};
    serial_get_string(payload, "questGranted", quest_granted, sizeof(quest_granted));
    bool objectives_done = serial_get_bool_default(payload, "objectivesDone", false);

    cJSON* quests = serial_get_array(payload, "quests");

    /* Notifications must read the prior state, so compute them BEFORE the store
     * upsert flips statuses. Titles/rewards come from the REST metadata cache
     * (the authoritative snapshot carries only codes + progress). */
    if (quests) {
        const cJSON* q = NULL;
        cJSON_ArrayForEach(q, quests) {
            char code[64] = {0}, status[32] = {0}, active_step[160] = {0};
            serial_get_string(q, "code", code, sizeof(code));
            serial_get_string(q, "status", status, sizeof(status));
            serial_get_string(q, "activeStep", active_step, sizeof(active_step));
            if (code[0] == '\0') continue;

            /* Ensure metadata is cached for the journal + these notifications. */
            quest_cache_fetch(code);
            const QuestMetadataEntry* qm = quest_cache_get(code);
            const char* disp = (qm && qm->title[0]) ? qm->title : code;

            if (0 == strcmp(status, "completed") && !quest_progress_store_is_completed(code)) {
                if (qm && qm->reward_count > 0) {
                    char body[160];
                    snprintf(body, sizeof(body), "Reward: %dx %s",
                             qm->rewards[0].quantity, qm->rewards[0].item_id);
                    modal_notification_show_reward(disp, body, (Color){ 90, 200, 110, 255 },
                                                   qm->rewards[0].item_id, qm->rewards[0].quantity);
                } else {
                    modal_notification_show("Quest Complete", disp, (Color){ 90, 200, 110, 255 });
                }
            } else if (0 == strcmp(code, quest_granted)) {
                modal_notification_show("Quest Accepted",
                                        active_step[0] ? active_step : disp,
                                        (Color){ 220, 190, 60, 255 });
            } else if (0 == strcmp(status, "active")) {
                /* Notify only when a whole STEP completes — i.e. the active step
                 * advanced — not on every per-objective +1. The active step
                 * description changes exactly when the previous step finished. */
                const QuestProgressEntry* prev = quest_progress_store_find(code);
                if (prev && QUEST_ACTIVE == prev->status && prev->active_step[0] != '\0' &&
                    0 != strcmp(prev->active_step, active_step)) {
                    char body[200];
                    snprintf(body, sizeof(body), "Next: %s", active_step[0] ? active_step : disp);
                    modal_notification_show("Step Complete", body, (Color){ 90, 170, 220, 255 });
                }
            }
        }
    }

    upsert_quest_array(quests);

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
    else if (0 == strcmp(type, "drop_collect"))    json_unpack_drop_collect(payload);
    else if (0 == strcmp(type, "drop_spawn"))      json_unpack_drop_spawn(payload);
    else if (0 == strcmp(type, "init_data"))       json_unpack_init_data(payload);
    else if (0 == strcmp(type, "metadata"))        json_unpack_metadata(payload);
    else if (0 == strcmp(type, "chat"))            json_unpack_chat(payload);
    else if (0 == strcmp(type, "dialog_ack"))      json_unpack_dialog_ack(payload);
    else if (0 == strcmp(type, "shop_ack"))        json_unpack_shop_ack(payload);
    else if (0 == strcmp(type, "craft_ack"))       json_unpack_craft_ack(payload);
    else if (0 == strcmp(type, "storage_state"))   json_unpack_storage_state(payload);
    else LOG_ERROR("[MESSAGE] unknown type '%s'\n", type);

    cJSON_Delete(root);
}
