#include "message_parser.h"
#include "game_state.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Simple JSON parsing without external libraries for now
// TODO: Replace with proper cJSON implementation

int message_parser_process(const char* json_str) {
    if (!json_str) {
        printf("[MESSAGE_PARSER] Null JSON string received\n");
        return -1;
    }

    printf("[MESSAGE_PARSER] Processing message: %.100s%s\n",
           json_str, strlen(json_str) > 100 ? "..." : "");

    MessageType type = get_message_type(json_str);

    switch (type) {
        case MSG_TYPE_INIT_DATA:
            return parse_init_data(json_str);
        case MSG_TYPE_AOI_UPDATE:
            return parse_aoi_update(json_str);
        case MSG_TYPE_SKILL_ITEM_IDS:
            return parse_skill_item_ids(json_str);
        case MSG_TYPE_ERROR:
            return parse_error_message(json_str);
        case MSG_TYPE_PING:
            // Handle ping - could send pong response
            printf("[MESSAGE_PARSER] Received ping\n");
            return 0;
        case MSG_TYPE_PONG:
            // Handle pong
            printf("[MESSAGE_PARSER] Received pong\n");
            return 0;
        case MSG_TYPE_UNKNOWN:
        default:
            printf("[MESSAGE_PARSER] Unknown message type\n");
            return -1;
    }
}

MessageType get_message_type(const char* json_str) {
    if (!json_str) return MSG_TYPE_UNKNOWN;

    // Simple string matching for message types
    // TODO: Replace with proper JSON parsing

    if (strstr(json_str, "\"type\":\"init_data\"") ||
        strstr(json_str, "\"type\": \"init_data\"")) {
        return MSG_TYPE_INIT_DATA;
    }

    if (strstr(json_str, "\"type\":\"aoi_update\"") ||
        strstr(json_str, "\"type\": \"aoi_update\"")) {
        return MSG_TYPE_AOI_UPDATE;
    }

    if (strstr(json_str, "\"type\":\"skill_item_ids\"") ||
        strstr(json_str, "\"type\": \"skill_item_ids\"")) {
        return MSG_TYPE_SKILL_ITEM_IDS;
    }

    if (strstr(json_str, "\"type\":\"error\"") ||
        strstr(json_str, "\"type\": \"error\"")) {
        return MSG_TYPE_ERROR;
    }

    if (strstr(json_str, "\"type\":\"ping\"") ||
        strstr(json_str, "\"type\": \"ping\"")) {
        return MSG_TYPE_PING;
    }

    if (strstr(json_str, "\"type\":\"pong\"") ||
        strstr(json_str, "\"type\": \"pong\"")) {
        return MSG_TYPE_PONG;
    }

    return MSG_TYPE_UNKNOWN;
}

int parse_init_data(const char* json_str) {
    printf("[MESSAGE_PARSER] Parsing init_data message\n");

    // TODO: Proper JSON parsing
    // For now, set some default values to get the client running

    game_state_lock();

    // Set basic defaults that would come from server
    g_game_state.grid_w = 100;
    g_game_state.grid_h = 100;
    g_game_state.cell_size = 12.0f;
    g_game_state.fps = 60;
    g_game_state.interpolation_ms = 200;
    g_game_state.aoi_radius = 15.0f;
    g_game_state.default_obj_width = 1.0f;
    g_game_state.default_obj_height = 1.0f;

    // Graphics settings
    g_game_state.camera_smoothing = 0.15f;
    g_game_state.camera_zoom = 1.0f;
    g_game_state.default_width_screen_factor = 0.5f;
    g_game_state.default_height_screen_factor = 0.5f;

    // Mark as initialized
    g_game_state.init_received = true;

    // Initialize camera if not already done
    if (!g_game_state.camera_initialized) {
        game_state_init_camera(800, 600); // Default screen size
    }

    game_state_unlock();

    printf("[MESSAGE_PARSER] Init data processed successfully\n");
    return 0;
}

int parse_aoi_update(const char* json_str) {
    printf("[MESSAGE_PARSER] Parsing AOI update message\n");

    // TODO: Proper JSON parsing for player positions and entities
    // For now, just update timestamp to show system is working

    game_state_lock();
    g_game_state.last_update_time = GetTime();
    game_state_unlock();

    return 0;
}

int parse_skill_item_ids(const char* json_str) {
    printf("[MESSAGE_PARSER] Parsing skill_item_ids message\n");

    // TODO: Parse item association data
    return 0;
}

int parse_error_message(const char* json_str) {
    printf("[MESSAGE_PARSER] Parsing error message\n");

    // TODO: Extract error message text and display it
    game_state_lock();
    strncpy(g_game_state.last_error_message, "Server error received",
            sizeof(g_game_state.last_error_message) - 1);
    g_game_state.error_display_time = GetTime();
    game_state_unlock();

    return 0;
}

// Helper function stubs for future JSON parsing implementation
int parse_color_rgba(const void* json, ColorRGBA* color) {
    // TODO: Implement JSON color parsing
    if (!color) return -1;

    // Default color
    color->r = 255;
    color->g = 255;
    color->b = 255;
    color->a = 255;

    return 0;
}

int parse_position(const void* json, Vector2* pos) {
    // TODO: Implement JSON position parsing
    if (!pos) return -1;

    pos->x = 0.0f;
    pos->y = 0.0f;

    return 0;
}

int parse_dimensions(const void* json, Vector2* dims) {
    // TODO: Implement JSON dimension parsing
    if (!dims) return -1;

    dims->x = 1.0f;
    dims->y = 1.0f;

    return 0;
}

Direction parse_direction(const void* json) {
    // TODO: Implement JSON direction parsing
    return DIRECTION_NONE;
}

ObjectLayerMode parse_mode(const void* json) {
    // TODO: Implement JSON mode parsing
    return MODE_IDLE;
}

int parse_object_layers(const void* json, ObjectLayerState* layers, int max_layers) {
    // TODO: Implement JSON object layer parsing
    return 0;
}

int parse_player_object(const void* json, PlayerState* player) {
    // TODO: Implement JSON player parsing
    if (!player) return -1;

    return 0;
}

int parse_visible_player(const void* json, PlayerState* player) {
    // TODO: Implement JSON visible player parsing
    if (!player) return -1;

    return 0;
}

int parse_visible_bot(const void* json, BotState* bot) {
    // TODO: Implement JSON bot parsing
    if (!bot) return -1;

    return 0;
}

int parse_world_object(const void* json, WorldObject* obj) {
    // TODO: Implement JSON world object parsing
    if (!obj) return -1;

    return 0;
}

int parse_path(const void* json, Vector2* path, int max_points) {
    // TODO: Implement JSON path parsing
    return 0;
}

int create_player_action_json(float target_x, float target_y, char* output_buffer, size_t buffer_size) {
    if (!output_buffer || buffer_size == 0) return -1;

    int result = snprintf(output_buffer, buffer_size,
                         "{\"type\":\"player_action\",\"payload\":{\"targetX\":%.2f,\"targetY\":%.2f}}",
                         target_x, target_y);

    if (result >= (int)buffer_size || result < 0) {
        return -1; // Buffer too small or encoding error
    }

    return 0;
}

int create_handshake_json(char* output_buffer, size_t buffer_size) {
    if (!output_buffer || buffer_size == 0) return -1;

    int result = snprintf(output_buffer, buffer_size,
                         "{\"type\":\"handshake\",\"client\":\"cyberia-mmo\",\"version\":\"1.0.0\"}");

    if (result >= (int)buffer_size || result < 0) {
        return -1; // Buffer too small or encoding error
    }

    return 0;
}
