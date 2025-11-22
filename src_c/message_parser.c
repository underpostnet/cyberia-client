#include "message_parser.h"
#include "serial.h"
#include "game_state.h"
#include "../lib/cJSON/cJSON.h"
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
        printf("[MESSAGE_PARSER] Failed to parse JSON for type detection\n");
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
 * Main Message Processing Entry Point
 * ============================================================================ */

int message_parser_process(const char* json_str) {
    if (!json_str) {
        printf("[MESSAGE_PARSER] Null JSON string received\n");
        return -1;
    }

    // Parse JSON
    size_t msg_len = strlen(json_str);
    printf("[MESSAGE_PARSER] Parsing JSON message, length: %zu bytes\n", msg_len);
    
    cJSON* root = cJSON_Parse(json_str);
    if (!root) {
        const char* error_ptr = cJSON_GetErrorPtr();
        printf("[MESSAGE_PARSER] ❌ Failed to parse JSON!\n");
        printf("[MESSAGE_PARSER] Error: %s\n", error_ptr ? error_ptr : "unknown");
        printf("[MESSAGE_PARSER] Message length: %zu bytes\n", msg_len);
        printf("[MESSAGE_PARSER] First 300 chars: %.300s\n", json_str);
        printf("[MESSAGE_PARSER] Last 100 chars: %s\n", 
               msg_len > 100 ? json_str + msg_len - 100 : json_str);
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
                printf("[MESSAGE_PARSER] Inferred message type: init_data (no type field)\n");
                result = message_parser_parse_init_data(root);
            }
            // Check for aoi_update indicators
            else if (cJSON_HasObjectItem(payload, "player") && cJSON_HasObjectItem(payload, "playerID")) {
                printf("[MESSAGE_PARSER] Inferred message type: aoi_update (no type field)\n");
                result = message_parser_parse_aoi_update(root);
            }
            // Check for skill_item_ids indicators
            else if (cJSON_HasObjectItem(payload, "associatedItemIds")) {
                printf("[MESSAGE_PARSER] Inferred message type: skill_item_ids (no type field)\n");
                result = message_parser_parse_skill_item_ids(root);
            }
            
            cJSON_Delete(root);
            return result;
        }
        
        printf("[MESSAGE_PARSER] Message missing 'type' field and cannot infer type\n");
        printf("[MESSAGE_PARSER] Message (first 200 chars): %.200s\n", json_str);
        cJSON_Delete(root);
        return -1;
    }

    printf("[MESSAGE_PARSER] Processing message type: %s\n", type_str);

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
        printf("[MESSAGE_PARSER] Received ping\n");
        result = 0;
    } else if (strcmp(type_str, "pong") == 0) {
        printf("[MESSAGE_PARSER] Received pong\n");
        result = 0;
    } else {
        printf("[MESSAGE_PARSER] Unknown message type: %s\n", type_str);
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
        }
        // Legacy lowercase support
        else if (strcmp(color_name, "grid") == 0) {
            g_game_state.colors.grid = c;
        } else if (strcmp(color_name, "floor") == 0) {
            g_game_state.colors.floor = c;
        } else if (strcmp(color_name, "bot") == 0) {
            g_game_state.colors.bot = c;
        }
    }
    
    game_state_unlock();
    return 0;
}

int message_parser_parse_init_data(const cJSON* json_root) {
    if (!json_root) return -1;
    
    printf("[MESSAGE_PARSER] Parsing init_data message\n");
    
    // Get payload object
    cJSON* payload = serial_get_object(json_root, "payload");
    if (!payload) {
        printf("[MESSAGE_PARSER] init_data missing payload\n");
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
    g_game_state.dev_ui = serial_get_bool_default(payload, "devUi", false);
    g_game_state.sum_stats_limit = serial_get_int_default(payload, "sumStatsLimit", 9999);
    
    // Parse colors
    cJSON* colors = serial_get_object(payload, "colors");
    if (colors) {
        message_parser_parse_colors(colors);
    } else {
        // Set default colors if not provided
        g_game_state.colors.background = (Color){30, 30, 30, 255};
        g_game_state.colors.foreground = (Color){200, 200, 200, 255};
        g_game_state.colors.target = (Color){255, 0, 0, 255};
        g_game_state.colors.path = (Color){255, 255, 0, 255};
        g_game_state.colors.aoi = (Color){0, 255, 255, 100};
        g_game_state.colors.grid = (Color){50, 50, 50, 255};
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
    
    printf("[MESSAGE_PARSER] Init data processed successfully\n");
    printf("  Grid: %dx%d, Cell Size: %.1f\n", g_game_state.grid_w, g_game_state.grid_h, g_game_state.cell_size);
    printf("  FPS: %d, Interpolation: %dms\n", g_game_state.fps, g_game_state.interpolation_ms);
    printf("  AOI Radius: %.1f, Dev UI: %s\n", g_game_state.aoi_radius, g_game_state.dev_ui ? "true" : "false");
    
    return 0;
}

/* ============================================================================
 * AOI Update Message Parser
 * ============================================================================ */

int message_parser_parse_visible_players(const cJSON* players_json) {
    if (!players_json) return 0;
    
    // Clear existing other players
    g_game_state.other_player_count = 0;
    
    cJSON* player_obj = NULL;
    cJSON_ArrayForEach(player_obj, players_json) {
        if (g_game_state.other_player_count >= MAX_ENTITIES) {
            printf("[MESSAGE_PARSER] Max visible players reached\n");
            break;
        }
        
        PlayerState player;
        memset(&player, 0, sizeof(PlayerState));
        
        // Deserialize player as entity (VisiblePlayer is subset of PlayerState)
        if (serial_deserialize_entity_state(player_obj, &player.base) == 0) {
            // Add to other players list
            game_state_update_player(&player);
        }
    }
    
    return 0;
}

int message_parser_parse_visible_bots(const cJSON* bots_json) {
    if (!bots_json) return 0;
    
    // Clear existing bots
    g_game_state.bot_count = 0;
    
    cJSON* bot_obj = NULL;
    cJSON_ArrayForEach(bot_obj, bots_json) {
        if (g_game_state.bot_count >= MAX_ENTITIES) {
            printf("[MESSAGE_PARSER] Max visible bots reached\n");
            break;
        }
        
        BotState bot;
        memset(&bot, 0, sizeof(BotState));
        
        if (serial_deserialize_bot_state(bot_obj, &bot) == 0) {
            game_state_update_bot(&bot);
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
        printf("[MESSAGE_PARSER] aoi_update missing payload\n");
        return -1;
    }
    
    game_state_lock();
    
    // Parse main player object
    cJSON* player_obj = serial_get_object(payload, "player");
    if (player_obj) {
        PlayerState player;
        memset(&player, 0, sizeof(PlayerState));
        
        if (serial_deserialize_player_state(player_obj, &player) == 0) {
            // Update main player state
            memcpy(&g_game_state.player, &player, sizeof(PlayerState));
            
            // Store player ID if not set
            if (g_game_state.player_id[0] == '\0') {
                strncpy(g_game_state.player_id, player.base.id, sizeof(g_game_state.player_id) - 1);
            }
        }
    }
    
    // Parse visible players
    cJSON* visible_players = serial_get_object(payload, "visiblePlayers");
    if (visible_players) {
        message_parser_parse_visible_players(visible_players);
    }
    
    // Parse visible grid objects
    cJSON* visible_grid_objects = serial_get_object(payload, "visibleGridObjects");
    if (visible_grid_objects) {
        // Parse bots
        cJSON* bots = serial_get_object(visible_grid_objects, "bots");
        if (bots) {
            message_parser_parse_visible_bots(bots);
        }
        
        // Parse obstacles
        cJSON* obstacles = serial_get_object(visible_grid_objects, "obstacles");
        if (obstacles) {
            message_parser_parse_visible_obstacles(obstacles);
        }
        
        // Parse portals
        cJSON* portals = serial_get_object(visible_grid_objects, "portals");
        if (portals) {
            message_parser_parse_visible_portals(portals);
        }
        
        // Parse floors
        cJSON* floors = serial_get_object(visible_grid_objects, "floors");
        if (floors) {
            message_parser_parse_visible_floors(floors);
        }
        
        // Parse foregrounds
        cJSON* foregrounds = serial_get_object(visible_grid_objects, "foregrounds");
        if (foregrounds) {
            message_parser_parse_visible_foregrounds(foregrounds);
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