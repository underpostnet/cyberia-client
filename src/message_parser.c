#include "message_parser.h"
#include "binary_aoi_decoder.h"
#include "config.h"
#include "game_state.h"
#include "serial.h"
#include <cJSON.h>
#include "object_layers_management.h"
#include "js/interact_bridge.h"
#include "notify_store.h"
#include "domain/camera.h"
#include "domain/presentation_runtime.h"
#include "ui/ui_state.h"
#include "ui/quest_progress_store.h"
#include "ui/modal_notification.h"
#include "ui/quest_cache.h"
#include "notification.h"
#include "util/log.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <assert.h>

static MessageType get_message_type(const cJSON* root);
static int message_parser_parse_metadata(const cJSON* json_root);
static int message_parser_parse_init_data(const cJSON* json_root);
static int message_parser_parse_aoi_update(const cJSON* json_root);
static int message_parser_parse_skill_item_ids(const cJSON* json_root);
static int message_parser_parse_error(const cJSON* json_root);
static int message_parser_parse_visible_players(const cJSON* players_json);
static int message_parser_parse_dlg_ack(const cJSON* json_root);
static void message_parser_upsert_quest_array(const cJSON* quests_json);

static MessageParserInitHandler s_init_handler = NULL;

void message_parser_set_init_handler(MessageParserInitHandler handler) {
    s_init_handler = handler;
}

/* ============================================================================
 * Main Message Processing Entry Point
 * ============================================================================ */

bool message_parser_parse(const char* json_str) {
    assert(json_str);
    cJSON* root = cJSON_Parse(json_str);
    if (!root) {
        LOG_ERROR("[MESSAGE_PARSER] Failed to parse JSON (length: %zu bytes)\n", strlen(json_str));
        return false;
    }

    MessageType msg_type = get_message_type(root);

    bool result = false;
    switch (msg_type) {
        case MSG_TYPE_INIT_DATA:
            result = 0 == message_parser_parse_init_data(root);
            break;
        case MSG_TYPE_METADATA:
            result = 0 == message_parser_parse_metadata(root);
            break;
        case MSG_TYPE_AOI_UPDATE:
            result = 0 == message_parser_parse_aoi_update(root);
            break;
        case MSG_TYPE_SKILL_ITEM_IDS:
            result = 0 == message_parser_parse_skill_item_ids(root);
            break;
        case MSG_TYPE_ERROR:
            result = 0 == message_parser_parse_error(root);
            break;
        case MSG_TYPE_PING:
        case MSG_TYPE_PONG:
            result = true;
            break;
        case MSG_TYPE_DLG_ACK:
            result = 0 == message_parser_parse_dlg_ack(root);
            break;
        case MSG_TYPE_CHAT: {
            cJSON* payload = serial_get_object(root, "payload");
            if (payload) {
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
            result = true;
            break;
        }
        case MSG_TYPE_UNKNOWN:
        default: {
            cJSON* payload = serial_get_object(root, "payload");
            if (payload) {
                // Check for init_data indicators
                if (cJSON_HasObjectItem(payload, "gridW") && cJSON_HasObjectItem(payload, "gridH")) {
                    result = 0 == message_parser_parse_init_data(root);
                }
                // Check for aoi_update indicators
                else if (cJSON_HasObjectItem(payload, "player") && cJSON_HasObjectItem(payload, "playerID")) {
                    result = 0 == message_parser_parse_aoi_update(root);
                }
                // Check for skill_item_ids indicators
                else if (cJSON_HasObjectItem(payload, "associatedItemIds")) {
                    result = 0 == message_parser_parse_skill_item_ids(root);
                }
            }
            break;
        }
    }

    cJSON_Delete(root);
    return result;
}

/* ============================================================================
 * Init Data Message Parser
 * ============================================================================ */

static int message_parser_parse_init_data(const cJSON* json_root) {
    LOG_INFO("[INIT_DATA] message_parser_parse_init_data entered\n");
    assert(json_root);

    // Get payload object
    cJSON* payload = serial_get_object(json_root, "payload");
    if (!payload) {
        LOG_ERROR("[INIT_DATA] ERROR: 'payload' object missing from init_data\n");
        return -1;
    }
    LOG_INFO("[INIT_DATA] payload found, parsing grid/world config\n");

    /* New session boundary — drop any stale prev-position snapshot from a
     * prior server lifetime so post-restart UUIDs don't interpolate from
     * origin. Cheap; safe to call on every init_data. */
    binary_aoi_reset_prev_snapshots();

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
    message_parser_upsert_quest_array(cJSON_GetObjectItem(payload, "quests"));

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
    return 0;
}

/* ============================================================================
 * Metadata Message Parser (ObjectLayer + Atlas data from Go server)
 * ============================================================================ */

static int message_parser_parse_metadata(const cJSON* json_root) {
    assert(json_root);

    cJSON* payload = serial_get_object(json_root, "payload");
    if (!payload) return -1;

    if (NULL == obj_layers_mgr_get()) {
        LOG_ERROR("[METADATA] ObjectLayersManager not initialized yet\n");
        return -1;
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
    return 0;
}

/* ============================================================================
 * AOI Update Message Parser
 * ============================================================================ */

static int message_parser_parse_visible_players(const cJSON* players_json) {
    assert(players_json);

    // Mark all existing players as not seen in this update
    bool player_seen[MAX_ENTITIES] = {false};

    // Parse all visible players from server
    cJSON* player_obj = NULL;
    cJSON_ArrayForEach(player_obj, players_json) {
        PlayerState player = {0};

        // Deserialize player as entity (VisiblePlayer is subset of PlayerState)
        if (serial_deserialize_entity_state(player_obj, &player.base) == 0) {
            // Update or add player - this preserves smooth interpolation for existing players
            game_state_update_player(&player);

            // Mark this player as seen
            for (int i = 0; i < g_game_state.other_player_count; i++) {
                if (strcmp(g_game_state.other_players[i].base.id, player.base.id) == 0) {
                    player_seen[i] = true;
                    break;
                }
            }
        }
    }

    // Remove players that are no longer visible
    for (int i = g_game_state.other_player_count - 1; i >= 0; i--) {
        if (!player_seen[i]) {
            game_state_remove_player(g_game_state.other_players[i].base.id);
        }
    }

    return 0;
}

static int message_parser_parse_aoi_update(const cJSON* json_root) {
    assert(json_root);

    // Get payload object
    cJSON* payload = serial_get_object(json_root, "payload");
    if (!payload) {
        return -1;
    }

    // Parse main player object
    cJSON* player_obj = serial_get_object(payload, "player");
    if (player_obj) {
        PlayerState player = {0};

        if (serial_deserialize_player_state(player_obj, &player) == 0) {
            bool first_update = (g_game_state.player_id[0] == '\0');

            Vector2 prev_interp_pos = g_game_state.player.base.interp_pos;
            Vector2 prev_server_pos = g_game_state.player.base.pos_server;
            Vector2 tap_target      = g_game_state.player.tap_target;
            bool    has_tap_target  = g_game_state.player.has_tap_target;

            /* Clear the local tap target as soon as the server reports the
             * player stopped — drives the on-tap arrow off when motion ends. */
            if (!first_update) {
                float sdx = player.base.pos_server.x - prev_server_pos.x;
                float sdy = player.base.pos_server.y - prev_server_pos.y;
                if (sdx * sdx + sdy * sdy < 0.0001f) {
                    has_tap_target = false;
                }
            }

            memcpy(&g_game_state.player, &player, sizeof(PlayerState));

            g_game_state.player.tap_target     = tap_target;
            g_game_state.player.has_tap_target = has_tap_target;

            if (!first_update) {
                g_game_state.player.base.interp_pos = prev_interp_pos;
            }

            // Store player ID if not set
            if (first_update) {
                strncpy(g_game_state.player_id, player.base.id, sizeof(g_game_state.player_id) - 1);
            }
        }
    }

    // Parse visible players
    cJSON* visible_players = serial_get_object(payload, "visiblePlayers");
    if (visible_players) {
        message_parser_parse_visible_players(visible_players);
    }

    // Parse visible grid objects - this is a FLAT dictionary where each object has a "Type" field
    cJSON* visible_grid_objects = serial_get_object(payload, "visibleGridObjects");
    if (visible_grid_objects) {
        // Clear all grid object collections first (non-entity objects)
        g_game_state.obstacle_count = 0;
        g_game_state.portal_count = 0;
        g_game_state.floor_count = 0;
        g_game_state.foreground_count = 0;

        // Note: bot_count is managed by game_state_update_bot() to preserve smooth interpolation
        // Mark all existing bots as not seen in this update
        bool bot_seen[MAX_ENTITIES] = {false};

        // Iterate through all objects in the flat dictionary (it's an object, not an array)
        // In cJSON, for {"id1": {...}, "id2": {...}}, child points to first key-value pair
        // obj->string is the key (ID), obj itself is the value object
        cJSON* obj = visible_grid_objects->child;
        while (obj != NULL) {
            // Get the Type field to determine what kind of object this is
            cJSON* type_field = cJSON_GetObjectItemCaseSensitive(obj, "Type");
            if (!type_field || !cJSON_IsString(type_field)) {
                obj = obj->next;
                continue;
            }

            const char* obj_type = type_field->valuestring;

            // Dispatch based on type
            if (strcmp(obj_type, "obstacle") == 0) {
                if (g_game_state.obstacle_count < MAX_OBJECTS) {
                    WorldObject* obstacle = &g_game_state.obstacles[g_game_state.obstacle_count];
                    if (serial_deserialize_world_object(obj, obstacle) != 0) {
                        obj = obj->next;
                        continue;
                    }
                        g_game_state.obstacle_count++;
                }
            }
            else if (strcmp(obj_type, "foreground") == 0) {
                if (g_game_state.foreground_count < MAX_OBJECTS) {
                    WorldObject* foreground = &g_game_state.foregrounds[g_game_state.foreground_count];
                    if (serial_deserialize_world_object(obj, foreground) != 0) {
                        obj = obj->next;
                        continue;
                    }
                        g_game_state.foreground_count++;
                }
            }
            else if (strcmp(obj_type, "portal") == 0) {
                if (g_game_state.portal_count < MAX_OBJECTS) {
                    WorldObject* portal = &g_game_state.portals[g_game_state.portal_count];
                    if (serial_deserialize_world_object(obj, portal) != 0) {
                        obj = obj->next;
                        continue;
                    }
                        g_game_state.portal_count++;
                }
            }
            else if (strcmp(obj_type, "floor") == 0) {
                if (g_game_state.floor_count < MAX_OBJECTS) {
                    WorldObject* floor = &g_game_state.floors[g_game_state.floor_count];
                    if (serial_deserialize_world_object(obj, floor) != 0) {
                        obj = obj->next;
                        continue;
                    }
                        g_game_state.floor_count++;
                }
            }
            else if (strcmp(obj_type, "bot") == 0) {
                // Parse bot
                BotState bot = {0};

                if (serial_deserialize_bot_state(obj, &bot) == 0) {
                    // Update or add bot - preserves smooth interpolation for existing bots
                    game_state_update_bot(&bot);

                    // Mark this bot as seen
                    for (int i = 0; i < g_game_state.bot_count; i++) {
                        if (strcmp(g_game_state.bots[i].base.id, bot.base.id) == 0) {
                            bot_seen[i] = true;
                            break;
                        }
                    }
                }
            }

            // Move to next object in the dictionary
            obj = obj->next;
        }

        // Remove bots that are no longer visible
        for (int i = g_game_state.bot_count - 1; i >= 0; i--) {
            if (!bot_seen[i]) {
                game_state_remove_bot(g_game_state.bots[i].base.id);
            }
        }
    }

    // Update timestamp
    g_game_state.last_update_time = GetTime();

    return 0;
}

/* ============================================================================
 * Skill/Item IDs Message Parser
 * ============================================================================ */

static int message_parser_parse_skill_item_ids(const cJSON* json_root) {
    assert(json_root);

    printf("[MESSAGE_PARSER] Parsing skill_item_ids message\n");

    // Get payload object
    cJSON* payload = serial_get_object(json_root, "payload");
    if (!payload) {
        printf("[MESSAGE_PARSER] skill_item_ids missing payload\n");
        return -1;
    }

    ui_state_clear_associated_items();

    cJSON* associated_ids = serial_get_array(payload, "associatedItemIds");
    if (associated_ids) {
        cJSON* item = NULL;
        cJSON_ArrayForEach(item, associated_ids) {
            if (cJSON_IsString(item)) {
                const char* item_id = cJSON_GetStringValue(item);
                if (item_id) ui_state_push_associated_item(item_id);
            }
        }
    }

    LOG_INFO("skill_item_ids parsed: %d associations", ui_state_associated_item_count());
    return 0;
}

static int message_parser_parse_error(const cJSON* json_root) {
    LOG_INFO("[MESSAGE_PARSER] Parsing error message\n");
    assert(json_root);

    // Get payload object
    cJSON* payload = serial_get_object(json_root, "payload");
    if (!payload) {
        LOG_ERROR("[MESSAGE_PARSER] error missing payload\n");
        return -1;
    }

    // Get error message
    char error_msg[MAX_MESSAGE_SIZE] = {0};
    if (serial_get_string(payload, "message", error_msg, sizeof(error_msg)) == 0)
    {
        strncpy(g_game_state.pending_error, error_msg, sizeof(g_game_state.pending_error) - 1);
        g_game_state.pending_error[sizeof(g_game_state.pending_error) - 1] = '\0';
        LOG_INFO("[MESSAGE_PARSER] Server error: %s\n", error_msg);
    }
    else
    {
        strncpy(g_game_state.pending_error, "UNKNOWN SERVER ERROR", sizeof(g_game_state.pending_error) - 1);
    }

    return 0;
}

static MessageType get_message_type(const cJSON* root) {
    char type_str[64] = {0};
    if (0 != serial_get_string(root, "type", type_str, sizeof(type_str))) {
        LOG_ERROR("[MESSAGE_PARSER] error missing type\n");
        return MSG_TYPE_UNKNOWN;
    }

    if (0 == strcmp(type_str, "init_data"))      return MSG_TYPE_INIT_DATA;
    if (0 == strcmp(type_str, "metadata"))       return MSG_TYPE_METADATA;
    if (0 == strcmp(type_str, "aoi_update"))     return MSG_TYPE_AOI_UPDATE;
    if (0 == strcmp(type_str, "skill_item_ids")) return MSG_TYPE_SKILL_ITEM_IDS;
    if (0 == strcmp(type_str, "error"))          return MSG_TYPE_ERROR;
    if (0 == strcmp(type_str, "ping"))           return MSG_TYPE_PING;
    if (0 == strcmp(type_str, "pong"))           return MSG_TYPE_PONG;
    if (0 == strcmp(type_str, "chat"))           return MSG_TYPE_CHAT;
    if (0 == strcmp(type_str, "dlg_ack"))        return MSG_TYPE_DLG_ACK;

    LOG_ERROR("[MESSAGE_PARSER] warning type unknown\n");
    return MSG_TYPE_UNKNOWN;
}

/* Upsert each entry of a server quest snapshot array into the local store.
 * Shared by init_data (initial snapshot) and dlg_ack (live updates).
 * The server only sends authoritative data (code, status, progress);
 * metadata (title, description, rewards) is fetched asynchronously from
 * the engine REST endpoint /api/cyberia-quest/:code via quest_cache. */
static void message_parser_upsert_quest_array(const cJSON* quests_json) {
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

/* dlg_ack is notify-only: it updates the local quest_progress_store from the affected
 * quest entries the server attached. questGranted / objectivesDone gate an
 * optional notification; no simulation state is touched here. */
static int message_parser_parse_dlg_ack(const cJSON* json_root) {
    cJSON* payload = serial_get_object(json_root, "payload");
    if (!payload) return -1;

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

    message_parser_upsert_quest_array(quests);

    if (quest_granted[0] != '\0')
        LOG_INFO("[DLG_ACK] quest granted: %s\n", quest_granted);
    if (objectives_done)
        LOG_INFO("[DLG_ACK] objective progressed\n");
    return 0;
}
