#include "message_parser.h"
#include "serial.h"
#include "config.h"
#include "cJSON.h"
#include "game_render.h"
#include "object_layers_management.h"
#include "js/interact_bridge.h"
#include "js/notify_badge.h"
#include "js/services.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <assert.h>

static int message_parser_parse_metadata(const cJSON* json_root);
static int message_parser_parse_init_data(const cJSON* json_root);
static int message_parser_parse_aoi_update(const cJSON* json_root);
static int message_parser_parse_skill_item_ids(const cJSON* json_root);
static int message_parser_parse_error(const cJSON* json_root);
static int message_parser_parse_colors(const cJSON* colors_json);
static int message_parser_parse_visible_players(const cJSON* players_json);

/* ============================================================================
 * Main Message Processing Entry PointZ
 * ============================================================================ */

int message_parser_process(const char* json_str) {
    if (!json_str) {
        return -1;
    }

    // Parse JSON
    size_t msg_len = strlen(json_str);
    cJSON* root = cJSON_Parse(json_str);
    if (!root) {
        printf("[MESSAGE_PARSER] Failed to parse JSON (length: %zu bytes)\n", msg_len);
        return -1;
    }

    // Get message type
    char type_str[64] = {0};
    if (serial_get_string(root, "type", type_str, sizeof(type_str)) != 0) {
        // Type field missing - try to infer from payload structure
        cJSON* payload = serial_get_object(root, "payload");
        if (payload) {
            int result = -1;

            // Check for init_data indicators
            if (cJSON_HasObjectItem(payload, "gridW") && cJSON_HasObjectItem(payload, "gridH")) {
                result = message_parser_parse_init_data(root);
            }
            // Check for aoi_update indicators
            else if (cJSON_HasObjectItem(payload, "player") && cJSON_HasObjectItem(payload, "playerID")) {
                result = message_parser_parse_aoi_update(root);
            }
            // Check for skill_item_ids indicators
            else if (cJSON_HasObjectItem(payload, "associatedItemIds")) {
                result = message_parser_parse_skill_item_ids(root);
            }

            cJSON_Delete(root);
            return result;
        }

        cJSON_Delete(root);
        return -1;
    }

    int result = 0;

    // Dispatch to appropriate handler
    if (strcmp(type_str, "init_data") == 0) {
        result = message_parser_parse_init_data(root);
    } else if (strcmp(type_str, "metadata") == 0) {
        result = message_parser_parse_metadata(root);
    } else if (strcmp(type_str, "aoi_update") == 0) {
        result = message_parser_parse_aoi_update(root);
    } else if (strcmp(type_str, "skill_item_ids") == 0) {
        result = message_parser_parse_skill_item_ids(root);
    } else if (strcmp(type_str, "error") == 0) {
        result = message_parser_parse_error(root);
    } else if (strcmp(type_str, "ping") == 0) {
        result = 0;
    } else if (strcmp(type_str, "pong") == 0) {
        result = 0;
    } else if (strcmp(type_str, "chat") == 0) {
        cJSON* payload = serial_get_object(root, "payload");
        if (payload) {
            char from_id[64] = {0};
            char text[256] = {0};
            serial_get_string(payload, "from", from_id, sizeof(from_id));
            serial_get_string(payload, "text", text, sizeof(text));
            if (from_id[0] && text[0]) {
                /* Always persist in the badge store (survives overlay close). */
                js_notify_badge_push(from_id, from_id, text);
                /* Live-update the overlay if it's showing this entity's chat. */
                js_interact_overlay_receive_chat(from_id, from_id, text);
            }
        }
        result = 0;
    } else {
        result = -1;
    }

    cJSON_Delete(root);
    return result;
}

/* ============================================================================
 * Init Data Message Parser
 * ============================================================================ */

static int message_parser_parse_colors(const cJSON* colors_json) {
    assert(colors_json);

    // Pre-initialise colours that may be absent from older DB documents
    // so they still have a visible default when the server doesn't send them.
    g_game_state.colors.self_border = (Color){ 220, 190, 60, 240 };

    // Parse each color in the dictionary
    cJSON* item = NULL;
    cJSON_ArrayForEach(item, colors_json) {
        const char* color_name = item->string;
        if (!color_name) continue;

        ColorRGBA rgba;
        if (serial_deserialize_color_rgba(item, &rgba) != 0) {
            continue;
        }

        // Map to game state colors
        Color c = (Color){rgba.r, rgba.g, rgba.b, rgba.a};

        if (strcmp(color_name, "BACKGROUND") == 0) {
            g_game_state.colors.background = c;
        } else if (strcmp(color_name, "GRID_BACKGROUND") == 0) {
            g_game_state.colors.grid_background = c;
        } else if (strcmp(color_name, "FLOOR_BACKGROUND") == 0) {
            g_game_state.colors.floor_background = c;
        } else if (strcmp(color_name, "OBSTACLE") == 0) {
            g_game_state.colors.obstacle = c;
        } else if (strcmp(color_name, "FOREGROUND") == 0) {
            g_game_state.colors.foreground = c;
        } else if (strcmp(color_name, "PLAYER") == 0) {
            g_game_state.colors.player = c;
        } else if (strcmp(color_name, "OTHER_PLAYER") == 0) {
            g_game_state.colors.other_player = c;
        } else if (strcmp(color_name, "PATH") == 0) {
            g_game_state.colors.path = c;
        } else if (strcmp(color_name, "TARGET") == 0) {
            g_game_state.colors.target = c;
        } else if (strcmp(color_name, "AOI") == 0) {
            g_game_state.colors.aoi = c;
        } else if (strcmp(color_name, "DEBUG_TEXT") == 0) {
            g_game_state.colors.debug_text = c;
        } else if (strcmp(color_name, "ERROR_TEXT") == 0) {
            g_game_state.colors.error_text = c;
        } else if (strcmp(color_name, "PORTAL") == 0) {
            g_game_state.colors.portal = c;
        } else if (strcmp(color_name, "PORTAL_INTER_PORTAL") == 0) {
            g_game_state.colors.portal_inter_portal = c;
        } else if (strcmp(color_name, "PORTAL_INTER_RANDOM") == 0) {
            g_game_state.colors.portal_inter_random = c;
        } else if (strcmp(color_name, "PORTAL_INTRA_RANDOM") == 0) {
            g_game_state.colors.portal_intra_random = c;
        } else if (strcmp(color_name, "PORTAL_INTRA_PORTAL") == 0) {
            g_game_state.colors.portal_intra_portal = c;
        } else if (strcmp(color_name, "PORTAL_LABEL") == 0) {
            g_game_state.colors.portal_label = c;
        } else if (strcmp(color_name, "UI_TEXT") == 0) {
            g_game_state.colors.ui_text = c;
        } else if (strcmp(color_name, "MAP_BOUNDARY") == 0) {
            g_game_state.colors.map_boundary = c;
        } else if (strcmp(color_name, "MAP_GRID") == 0) {
            g_game_state.colors.grid = c;
        } else if (strcmp(color_name, "GRID") == 0) {
            g_game_state.colors.grid = c;
        } else if (strcmp(color_name, "FLOOR") == 0) {
            g_game_state.colors.floor = c;
        } else if (strcmp(color_name, "BOT") == 0) {
            g_game_state.colors.bot = c;
        } else if (strcmp(color_name, "GHOST") == 0) {
            g_game_state.colors.ghost = c;
        } else if (strcmp(color_name, "COIN") == 0) {
            g_game_state.colors.coin = c;
        } else if (strcmp(color_name, "WEAPON") == 0) {
            g_game_state.colors.weapon = c;
        } else if (strcmp(color_name, "SKILL") == 0) {
            g_game_state.colors.skill = c;
        } else if (strcmp(color_name, "SELF_BORDER") == 0) {
            g_game_state.colors.self_border = c;
        }

    }
    return 0;
}

static int message_parser_parse_init_data(const cJSON* json_root) {
    assert(json_root);

    // Get payload object
    cJSON* payload = serial_get_object(json_root, "payload");
    if (!payload) {
        return -1;
    }

    // Parse grid configuration
    g_game_state.grid_w = serial_get_int_default(payload, "gridW", 100);
    g_game_state.grid_h = serial_get_int_default(payload, "gridH", 100);
    g_game_state.cell_size = serial_get_float_default(payload, "cellSize", 12.0f);

    // Parse game settings
    g_game_state.fps = serial_get_int_default(payload, "fps", 60);
    if (g_game_state.fps > 0) SetTargetFPS(g_game_state.fps);
    g_game_state.interpolation_ms = serial_get_int_default(payload, "interpolationMs", 200);
    g_game_state.aoi_radius = serial_get_float_default(payload, "aoiRadius", 15.0f);

    // Parse default object dimensions
    g_game_state.default_obj_width = serial_get_float_default(payload, "defaultObjectWidth", 1.0f);
    g_game_state.default_obj_height = serial_get_float_default(payload, "defaultObjectHeight", 1.0f);

    // Parse graphics settings
    g_game_state.camera_smoothing = serial_get_float_default(payload, "cameraSmoothing", 0.15f);
    g_game_state.camera_zoom = serial_get_float_default(payload, "cameraZoom", 1.0f);
    g_game_state.default_width_screen_factor = serial_get_float_default(payload, "defaultWidthScreenFactor", 0.5f);
    g_game_state.default_height_screen_factor = serial_get_float_default(payload, "defaultHeightScreenFactor", 0.5f);

    // Parse UI settings
    // ENABLE_DEV_UI=true forces dev UI on regardless of server; false defers to server
    g_game_state.dev_ui = ENABLE_DEV_UI ? true : serial_get_bool_default(payload, "devUi", false);
    g_game_state.sum_stats_limit = serial_get_int_default(payload, "sumStatsLimit", 9999);

    // Parse colors
    cJSON* colors = serial_get_object(payload, "colors");
    if (colors) {
        message_parser_parse_colors(colors);
    } else {
        // Set default colors if not provided
        g_game_state.colors.background = (Color){30, 30, 30, 255};
        g_game_state.colors.grid_background = (Color){20, 20, 20, 255};
        g_game_state.colors.floor_background = (Color){25, 25, 25, 255};
        g_game_state.colors.foreground = (Color){200, 200, 200, 255};
        g_game_state.colors.target = (Color){255, 0, 0, 255};
        g_game_state.colors.path = (Color){255, 255, 0, 255};
        g_game_state.colors.aoi = (Color){0, 255, 255, 100};
        g_game_state.colors.grid = (Color){255, 0, 0, 80};  // Red with low alpha for visibility
        g_game_state.colors.map_boundary = (Color){255, 255, 255, 255};  // White boundary
        g_game_state.colors.player = (Color){0, 255, 0, 255};
        g_game_state.colors.bot = (Color){255, 128, 0, 255};
        g_game_state.colors.obstacle = (Color){128, 128, 128, 255};
        g_game_state.colors.portal = (Color){255, 0, 255, 255};
        g_game_state.colors.portal_inter_portal = (Color){0, 200, 200, 255};
        g_game_state.colors.portal_inter_random = (Color){80, 130, 255, 255};
        g_game_state.colors.portal_intra_random = (Color){220, 200, 50, 255};
        g_game_state.colors.portal_intra_portal = (Color){200, 80, 200, 255};
        g_game_state.colors.floor = (Color){100, 100, 100, 255};
    }

    // Parse skill map: { "triggerItemId": ["logicEventId1", ...], ... }
    g_game_state.skill_map_count = 0;
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
            serial_get_string(etd, "colorKey",   d->color_key,    sizeof(d->color_key));

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

    // Parse status icon mapping (id → icon filename + border colour)
    g_game_state.status_icon_count = 0;
    cJSON* status_icons_json = cJSON_GetObjectItem(payload, "statusIcons");
    if (status_icons_json && cJSON_IsArray(status_icons_json)) {
        cJSON* si = NULL;
        cJSON_ArrayForEach(si, status_icons_json) {
            if (g_game_state.status_icon_count >= MAX_STATUS_ICONS) break;
            StatusIconConfig* sc = &g_game_state.status_icons[g_game_state.status_icon_count];
            memset(sc, 0, sizeof(StatusIconConfig));
            cJSON* id_json = cJSON_GetObjectItem(si, "id");
            if (id_json && cJSON_IsNumber(id_json)) {
                sc->id = (uint8_t)id_json->valueint;
            }
            serial_get_string(si, "iconId", sc->icon_id, sizeof(sc->icon_id));
            /* Parse borderColor sub-object {r,g,b,a} — server-driven. */
            cJSON* bc = cJSON_GetObjectItem(si, "borderColor");
            if (bc && cJSON_IsObject(bc)) {
                cJSON* cr = cJSON_GetObjectItem(bc, "r");
                cJSON* cg = cJSON_GetObjectItem(bc, "g");
                cJSON* cb = cJSON_GetObjectItem(bc, "b");
                cJSON* ca = cJSON_GetObjectItem(bc, "a");
                sc->border_color = (Color){
                    (unsigned char)(cr && cJSON_IsNumber(cr) ? cr->valueint : 100),
                    (unsigned char)(cg && cJSON_IsNumber(cg) ? cg->valueint : 100),
                    (unsigned char)(cb && cJSON_IsNumber(cb) ? cb->valueint : 100),
                    (unsigned char)(ca && cJSON_IsNumber(ca) ? ca->valueint : 200),
                };
            } else {
                sc->border_color = (Color){ 100, 100, 100, 200 };
            }
            g_game_state.status_icon_count++;
        }
    }
    printf("[MSG_PARSER] Parsed %d status icons\n", g_game_state.status_icon_count);
    for (int dbg = 0; dbg < g_game_state.status_icon_count; dbg++) {
        StatusIconConfig* d = &g_game_state.status_icons[dbg];
        printf("[MSG_PARSER]   icon[%d] id=%d iconId=%s border=(%d,%d,%d,%d)\n",
               dbg, d->id, d->icon_id,
               d->border_color.r, d->border_color.g, d->border_color.b, d->border_color.a);
    }

    if (skill_map_json && cJSON_IsObject(skill_map_json)) {
        cJSON* entry = NULL;
        cJSON_ArrayForEach(entry, skill_map_json) {
            if (g_game_state.skill_map_count >= MAX_SKILL_ENTRIES) break;
            if (!entry->string || !cJSON_IsArray(entry)) continue;

            SkillEntry* se = &g_game_state.skill_map[g_game_state.skill_map_count];
            strncpy(se->trigger_item_id, entry->string, MAX_ID_LENGTH - 1);
            se->trigger_item_id[MAX_ID_LENGTH - 1] = '\0';
            se->logic_event_count = 0;

            cJSON* sid = NULL;
            cJSON_ArrayForEach(sid, entry) {
                if (se->logic_event_count >= MAX_LOGIC_EVENT_IDS) break;
                if (!cJSON_IsString(sid)) continue;
                strncpy(
                    se->logic_event_ids[se->logic_event_count],
                    cJSON_GetStringValue(sid),
                    MAX_ID_LENGTH - 1
                );
                se->logic_event_ids[se->logic_event_count][MAX_ID_LENGTH - 1] = '\0';
                se->logic_event_count++;
            }
            g_game_state.skill_map_count++;
        }
    }

    // Mark as initialized
    g_game_state.init_received = true;

    // Initialize camera if not already done
    game_state_init_camera(800, 600); // Default screen size
    printf("  AOI Radius: %.1f, Dev UI: %s\n", g_game_state.aoi_radius, g_game_state.dev_ui ? "true" : "false");

    return 0;
}

/* ============================================================================
 * Metadata Message Parser (ObjectLayer + Atlas data from Go server)
 * ============================================================================ */

static int message_parser_parse_metadata(const cJSON* json_root) {
    assert(json_root);

    cJSON* payload = serial_get_object(json_root, "payload");
    if (!payload) return -1;

    ObjectLayersManager* mgr = game_render_get_obj_layers_mgr();
    if (!mgr) {
        fprintf(stderr, "[METADATA] ObjectLayersManager not initialized yet\n");
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
                populate_object_layer_from_json(mgr, item_id, entry);
                obj_layers_mgr_schedule_atlas_fetch(mgr, item_id);
                ol_count++;
            }
        }
    }

    // Parse apiBaseUrl if provided (update the Engine API URL for blob fetches)
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
        printf("[METADATA] Equipment rules: %d activeItemTypes, onePerType=%d, requireSkin=%d\n",
               g_game_state.equipment_rules.active_item_type_count,
               g_game_state.equipment_rules.one_per_type,
               g_game_state.equipment_rules.require_skin);
    }

    printf("[METADATA] Cached %d ObjectLayers, scheduled %d atlas REST fetches\n", ol_count, ol_count);
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

            // Save prediction state and current interp_pos before overwrite
            Vector2 prev_interp_pos = g_game_state.player.base.interp_pos;
            Vector2 prev_server_pos = g_game_state.player.base.pos_server;
            Vector2 tap_target = g_game_state.player.tap_target;
            bool has_tap_target = g_game_state.player.has_tap_target;
            float estimated_speed = g_game_state.player.estimated_speed;
            Vector2 velocity = g_game_state.player.velocity;

            // Estimate velocity from server position delta
            if (!first_update) {
                double dt = GetTime() - g_game_state.last_update_time;
                if (dt > 0.001) {
                    float dx = player.base.pos_server.x - prev_server_pos.x;
                    float dy = player.base.pos_server.y - prev_server_pos.y;
                    velocity = (Vector2){dx / (float)dt, dy / (float)dt};
                    float speed = sqrtf(dx * dx + dy * dy) / (float)dt;
                    estimated_speed = estimated_speed > 0.1f ?
                        estimated_speed * 0.7f + speed * 0.3f : speed;
                }

                // Clear tap target if server shows player stopped
                float sdx = player.base.pos_server.x - prev_server_pos.x;
                float sdy = player.base.pos_server.y - prev_server_pos.y;
                if (sdx * sdx + sdy * sdy < 0.0001f) {
                    has_tap_target = false;
                }
            }

            // Update main player state
            memcpy(&g_game_state.player, &player, sizeof(PlayerState));

            // Restore prediction state (memcpy zeroed them)
            g_game_state.player.tap_target = tap_target;
            g_game_state.player.has_tap_target = has_tap_target;
            g_game_state.player.estimated_speed = estimated_speed;
            g_game_state.player.velocity = velocity;

            // For exponential blend: keep interp_pos where it was (no hard reset)
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
                    // Parse obstacle
                    cJSON* pos = cJSON_GetObjectItemCaseSensitive(obj, "Pos");
                    cJSON* dims = cJSON_GetObjectItemCaseSensitive(obj, "Dims");
                    cJSON* id = cJSON_GetObjectItemCaseSensitive(obj, "id");

                    if (pos && dims && id && cJSON_IsString(id)) {
                        WorldObject* obstacle = &g_game_state.obstacles[g_game_state.obstacle_count];
                        strncpy(obstacle->id, id->valuestring, sizeof(obstacle->id) - 1);
                        obstacle->pos.x = serial_get_float_default(pos, "X", 0.0f);
                        obstacle->pos.y = serial_get_float_default(pos, "Y", 0.0f);
                        obstacle->dims.x = serial_get_float_default(dims, "Width", 1.0f);
                        obstacle->dims.y = serial_get_float_default(dims, "Height", 1.0f);
                        g_game_state.obstacle_count++;
                    }
                }
            }
            else if (strcmp(obj_type, "foreground") == 0) {
                if (g_game_state.foreground_count < MAX_OBJECTS) {
                    // Parse foreground
                    cJSON* pos = cJSON_GetObjectItemCaseSensitive(obj, "Pos");
                    cJSON* dims = cJSON_GetObjectItemCaseSensitive(obj, "Dims");
                    cJSON* id = cJSON_GetObjectItemCaseSensitive(obj, "id");

                    if (pos && dims && id && cJSON_IsString(id)) {
                        WorldObject* foreground = &g_game_state.foregrounds[g_game_state.foreground_count];
                        strncpy(foreground->id, id->valuestring, sizeof(foreground->id) - 1);
                        foreground->pos.x = serial_get_float_default(pos, "X", 0.0f);
                        foreground->pos.y = serial_get_float_default(pos, "Y", 0.0f);
                        foreground->dims.x = serial_get_float_default(dims, "Width", 1.0f);
                        foreground->dims.y = serial_get_float_default(dims, "Height", 1.0f);
                        g_game_state.foreground_count++;
                    }
                }
            }
            else if (strcmp(obj_type, "portal") == 0) {
                if (g_game_state.portal_count < MAX_OBJECTS) {
                    // Parse portal
                    cJSON* pos = cJSON_GetObjectItemCaseSensitive(obj, "Pos");
                    cJSON* dims = cJSON_GetObjectItemCaseSensitive(obj, "Dims");
                    cJSON* id = cJSON_GetObjectItemCaseSensitive(obj, "id");
                    cJSON* label = cJSON_GetObjectItemCaseSensitive(obj, "PortalLabel");

                    if (pos && dims && id && cJSON_IsString(id)) {
                        WorldObject* portal = &g_game_state.portals[g_game_state.portal_count];
                        strncpy(portal->id, id->valuestring, sizeof(portal->id) - 1);
                        portal->pos.x = serial_get_float_default(pos, "X", 0.0f);
                        portal->pos.y = serial_get_float_default(pos, "Y", 0.0f);
                        portal->dims.x = serial_get_float_default(dims, "Width", 1.0f);
                        portal->dims.y = serial_get_float_default(dims, "Height", 1.0f);
                        if (label && cJSON_IsString(label)) {
                            strncpy(portal->portal_label, label->valuestring, sizeof(portal->portal_label) - 1);
                        }
                        g_game_state.portal_count++;
                    }
                }
            }
            else if (strcmp(obj_type, "floor") == 0) {
                if (g_game_state.floor_count < MAX_OBJECTS) {
                    // Parse floor
                    cJSON* pos = cJSON_GetObjectItemCaseSensitive(obj, "Pos");
                    cJSON* dims = cJSON_GetObjectItemCaseSensitive(obj, "Dims");
                    cJSON* id = cJSON_GetObjectItemCaseSensitive(obj, "id");
                    cJSON* obj_layers = cJSON_GetObjectItemCaseSensitive(obj, "objectLayers");

                    if (pos && dims && id && cJSON_IsString(id)) {
                        WorldObject* floor = &g_game_state.floors[g_game_state.floor_count];
                        strncpy(floor->id, id->valuestring, sizeof(floor->id) - 1);
                        floor->pos.x = serial_get_float_default(pos, "X", 0.0f);
                        floor->pos.y = serial_get_float_default(pos, "Y", 0.0f);
                        floor->dims.x = serial_get_float_default(dims, "Width", 1.0f);
                        floor->dims.y = serial_get_float_default(dims, "Height", 1.0f);

                        // Parse object layers if present
                        floor->object_layer_count = 0;
                        if (obj_layers && cJSON_IsArray(obj_layers)) {
                            cJSON* layer = NULL;
                            cJSON_ArrayForEach(layer, obj_layers) {
                                if (floor->object_layer_count >= MAX_OBJECT_LAYERS) break;

                                ObjectLayerState* layer_state = &floor->object_layers[floor->object_layer_count];

                                cJSON* item_id = cJSON_GetObjectItemCaseSensitive(layer, "itemId");
                                cJSON* active = cJSON_GetObjectItemCaseSensitive(layer, "active");
                                cJSON* quantity = cJSON_GetObjectItemCaseSensitive(layer, "quantity");

                                if (item_id && cJSON_IsString(item_id)) {
                                    strncpy(layer_state->item_id, item_id->valuestring, sizeof(layer_state->item_id) - 1);
                                }
                                if (active && cJSON_IsBool(active)) {
                                    layer_state->active = cJSON_IsTrue(active);
                                }
                                if (quantity && cJSON_IsNumber(quantity)) {
                                    layer_state->quantity = quantity->valueint;
                                }

                                floor->object_layer_count++;
                            }
                        }

                        g_game_state.floor_count++;
                    }
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

    // Clear existing associations
    g_game_state.associated_item_count = 0;

    // Get associated item IDs array
    cJSON* associated_ids = serial_get_array(payload, "associatedItemIds");
    if (associated_ids) {
        cJSON* item = NULL;
        cJSON_ArrayForEach(item, associated_ids) {
            if (g_game_state.associated_item_count >= MAX_ENTITIES) {
                break;
            }

            if (cJSON_IsString(item)) {
                const char* item_id = cJSON_GetStringValue(item);
                if (item_id) {
                    strncpy(
                        g_game_state.associated_item_ids[g_game_state.associated_item_count],
                        item_id,
                        MAX_ITEM_ID_LENGTH - 1
                    );
                    g_game_state.associated_item_count++;
                }
            }
        }
    }

    printf("[MESSAGE_PARSER] Skill/Item IDs processed: %d associations\n",
           g_game_state.associated_item_count);

    return 0;
}

/* ============================================================================
 * Error Message Parser
 * ============================================================================ */

static int message_parser_parse_error(const cJSON* json_root) {
    assert(json_root);

    printf("[MESSAGE_PARSER] Parsing error message\n");

    // Get payload object
    cJSON* payload = serial_get_object(json_root, "payload");
    if (!payload) {
        printf("[MESSAGE_PARSER] error missing payload\n");
        return -1;
    }

    // Get error message
    char error_msg[MAX_MESSAGE_SIZE] = {0};
    if (serial_get_string(payload, "message", error_msg, sizeof(error_msg)) == 0) {
        strncpy(g_game_state.last_error_message, error_msg, sizeof(g_game_state.last_error_message) - 1);
        g_game_state.error_display_time = GetTime();
        printf("[MESSAGE_PARSER] Server error: %s\n", error_msg);
    } else {
        strncpy(g_game_state.last_error_message, "Unknown server error",
                sizeof(g_game_state.last_error_message) - 1);
        g_game_state.error_display_time = GetTime();
    }

    return 0;
}
