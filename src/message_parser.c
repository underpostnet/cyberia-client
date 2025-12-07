#include "message_parser.h"
#include "serial.h"
#include "game_state.h"
#include "config.h"
#include "cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ============================================================================
 * Message Type Detection
 * ============================================================================ */

MessageType message_parser_get_type(const char* json_str) {
    if (!json_str) return MSG_TYPE_UNKNOWN;

    cJSON* root = cJSON_Parse(json_str);
    if (!root) {
        return MSG_TYPE_UNKNOWN;
    }

    MessageType type = MSG_TYPE_UNKNOWN;
    char type_str[64] = {0};

    if (serial_get_string(root, "type", type_str, sizeof(type_str)) == 0) {
        if (strcmp(type_str, "init_data") == 0) {
            type = MSG_TYPE_INIT_DATA;
        } else if (strcmp(type_str, "aoi_update") == 0) {
            type = MSG_TYPE_AOI_UPDATE;
        } else if (strcmp(type_str, "skill_item_ids") == 0) {
            type = MSG_TYPE_SKILL_ITEM_IDS;
        } else if (strcmp(type_str, "error") == 0) {
            type = MSG_TYPE_ERROR;
        } else if (strcmp(type_str, "ping") == 0) {
            type = MSG_TYPE_PING;
        } else if (strcmp(type_str, "pong") == 0) {
            type = MSG_TYPE_PONG;
        }
    }

    cJSON_Delete(root);
    return type;
}

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
    } else {
        result = -1;
    }

    cJSON_Delete(root);
    return result;
}

/* ============================================================================
 * Init Data Message Parser
 * ============================================================================ */

int message_parser_parse_colors(const cJSON* colors_json) {
    if (!colors_json) return -1;

    game_state_lock();

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
        }

    }

    game_state_unlock();
    return 0;
}

int message_parser_parse_init_data(const cJSON* json_root) {
    if (!json_root) return -1;

    // Get payload object
    cJSON* payload = serial_get_object(json_root, "payload");
    if (!payload) {
        return -1;
    }

    game_state_lock();

    // Parse grid configuration
    g_game_state.grid_w = serial_get_int_default(payload, "gridW", 100);
    g_game_state.grid_h = serial_get_int_default(payload, "gridH", 100);
    g_game_state.cell_size = serial_get_float_default(payload, "cellSize", 12.0f);

    // Parse game settings
    g_game_state.fps = serial_get_int_default(payload, "fps", 60);
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
    // If FORCE_DEV_UI is enabled, always set dev_ui to true regardless of server response
    if (FORCE_DEV_UI) {
        g_game_state.dev_ui = true;
    } else {
        g_game_state.dev_ui = serial_get_bool_default(payload, "devUi", false);
    }
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
        g_game_state.colors.floor = (Color){100, 100, 100, 255};
    }

    // Mark as initialized
    g_game_state.init_received = true;

    // Initialize camera if not already done
    if (!g_game_state.camera_initialized) {
        game_state_init_camera(800, 600); // Default screen size
    }

    game_state_unlock();
    printf("  AOI Radius: %.1f, Dev UI: %s\n", g_game_state.aoi_radius, g_game_state.dev_ui ? "true" : "false");

    return 0;
}

/* ============================================================================
 * AOI Update Message Parser
 * ============================================================================ */

int message_parser_parse_visible_players(const cJSON* players_json) {
    if (!players_json) return 0;

    // Mark all existing players as not seen in this update
    bool player_seen[MAX_ENTITIES] = {false};

    // Parse all visible players from server
    cJSON* player_obj = NULL;
    cJSON_ArrayForEach(player_obj, players_json) {
        PlayerState player;
        memset(&player, 0, sizeof(PlayerState));

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

int message_parser_parse_visible_bots(const cJSON* bots_json) {
    if (!bots_json) return 0;

    // Mark all existing bots as not seen in this update
    bool bot_seen[MAX_ENTITIES] = {false};

    // Parse all visible bots from server
    cJSON* bot_obj = NULL;
    cJSON_ArrayForEach(bot_obj, bots_json) {
        BotState bot;
        memset(&bot, 0, sizeof(BotState));

        if (serial_deserialize_bot_state(bot_obj, &bot) == 0) {
            // Update or add bot - this preserves smooth interpolation for existing bots
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

    // Remove bots that are no longer visible
    for (int i = g_game_state.bot_count - 1; i >= 0; i--) {
        if (!bot_seen[i]) {
            game_state_remove_bot(g_game_state.bots[i].base.id);
        }
    }

    return 0;
}

int message_parser_parse_visible_obstacles(const cJSON* obstacles_json) {
    if (!obstacles_json) return 0;

    // Clear existing obstacles
    g_game_state.obstacle_count = 0;

    cJSON* obj = NULL;
    cJSON_ArrayForEach(obj, obstacles_json) {
        if (g_game_state.obstacle_count >= MAX_OBJECTS) {
            printf("[MESSAGE_PARSER] Max obstacles reached\n");
            break;
        }

        WorldObject world_obj;
        memset(&world_obj, 0, sizeof(WorldObject));

        if (serial_deserialize_world_object(obj, &world_obj) == 0) {
            g_game_state.obstacles[g_game_state.obstacle_count++] = world_obj;
        }
    }

    return 0;
}

int message_parser_parse_visible_portals(const cJSON* portals_json) {
    if (!portals_json) return 0;

    // Clear existing portals
    g_game_state.portal_count = 0;

    cJSON* obj = NULL;
    cJSON_ArrayForEach(obj, portals_json) {
        if (g_game_state.portal_count >= MAX_OBJECTS) {
            printf("[MESSAGE_PARSER] Max portals reached\n");
            break;
        }

        WorldObject world_obj;
        memset(&world_obj, 0, sizeof(WorldObject));

        if (serial_deserialize_world_object(obj, &world_obj) == 0) {
            g_game_state.portals[g_game_state.portal_count++] = world_obj;
        }
    }

    return 0;
}

int message_parser_parse_visible_floors(const cJSON* floors_json) {
    if (!floors_json) return 0;

    // Clear existing floors
    g_game_state.floor_count = 0;

    cJSON* obj = NULL;
    cJSON_ArrayForEach(obj, floors_json) {
        if (g_game_state.floor_count >= MAX_OBJECTS) {
            printf("[MESSAGE_PARSER] Max floors reached\n");
            break;
        }

        WorldObject world_obj;
        memset(&world_obj, 0, sizeof(WorldObject));

        if (serial_deserialize_world_object(obj, &world_obj) == 0) {
            g_game_state.floors[g_game_state.floor_count++] = world_obj;
        }
    }

    return 0;
}

int message_parser_parse_visible_foregrounds(const cJSON* foregrounds_json) {
    if (!foregrounds_json) return 0;

    // Clear existing foregrounds
    g_game_state.foreground_count = 0;

    cJSON* obj = NULL;
    cJSON_ArrayForEach(obj, foregrounds_json) {
        if (g_game_state.foreground_count >= MAX_OBJECTS) {
            printf("[MESSAGE_PARSER] Max foregrounds reached\n");
            break;
        }

        WorldObject world_obj;
        memset(&world_obj, 0, sizeof(WorldObject));

        if (serial_deserialize_world_object(obj, &world_obj) == 0) {
            g_game_state.foregrounds[g_game_state.foreground_count++] = world_obj;
        }
    }

    return 0;
}

int message_parser_parse_aoi_update(const cJSON* json_root) {
    if (!json_root) return -1;

    // Get payload object
    cJSON* payload = serial_get_object(json_root, "payload");
    if (!payload) {
        return -1;
    }

    game_state_lock();

    // Parse main player object
    cJSON* player_obj = serial_get_object(payload, "player");
    if (player_obj) {
        PlayerState player;
        memset(&player, 0, sizeof(PlayerState));

        if (serial_deserialize_player_state(player_obj, &player) == 0) {
            // Preserve previous interpolated position for smooth transitions
            Vector2 prev_interp_pos = g_game_state.player.base.interp_pos;
            bool first_update = (g_game_state.player_id[0] == '\0');

            // Update main player state
            memcpy(&g_game_state.player, &player, sizeof(PlayerState));

            // If not first update, set pos_prev to last interpolated position
            if (!first_update) {
                g_game_state.player.base.pos_prev = prev_interp_pos;
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
                BotState bot;
                memset(&bot, 0, sizeof(BotState));

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

    game_state_unlock();

    return 0;
}

/* ============================================================================
 * Skill/Item IDs Message Parser
 * ============================================================================ */

int message_parser_parse_skill_item_ids(const cJSON* json_root) {
    if (!json_root) return -1;

    printf("[MESSAGE_PARSER] Parsing skill_item_ids message\n");

    // Get payload object
    cJSON* payload = serial_get_object(json_root, "payload");
    if (!payload) {
        printf("[MESSAGE_PARSER] skill_item_ids missing payload\n");
        return -1;
    }

    game_state_lock();

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

    game_state_unlock();

    printf("[MESSAGE_PARSER] Skill/Item IDs processed: %d associations\n",
           g_game_state.associated_item_count);

    return 0;
}

/* ============================================================================
 * Error Message Parser
 * ============================================================================ */

int message_parser_parse_error(const cJSON* json_root) {
    if (!json_root) return -1;

    printf("[MESSAGE_PARSER] Parsing error message\n");

    // Get payload object
    cJSON* payload = serial_get_object(json_root, "payload");
    if (!payload) {
        printf("[MESSAGE_PARSER] error missing payload\n");
        return -1;
    }

    game_state_lock();

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

    game_state_unlock();

    return 0;
}
