#ifndef MESSAGE_PARSER_H
#define MESSAGE_PARSER_H

#include "game_state.h"
#include <stdbool.h>
#include <stddef.h>

/**
 * @file message_parser.h
 * @brief JSON message parsing for network protocol
 * 
 * This module handles parsing of incoming JSON messages from the server
 * and updating the game state accordingly. It replaces the Python JSON
 * parsing functionality with C implementations using cJSON library.
 */

/**
 * @brief Message types from server
 */
typedef enum {
    MSG_TYPE_UNKNOWN = 0,
    MSG_TYPE_INIT_DATA,
    MSG_TYPE_AOI_UPDATE,
    MSG_TYPE_SKILL_ITEM_IDS,
    MSG_TYPE_ERROR,
    MSG_TYPE_PING,
    MSG_TYPE_PONG
} MessageType;

/**
 * @brief Parse and process incoming message from server
 * 
 * This is the main entry point for processing all server messages.
 * It determines the message type and dispatches to the appropriate
 * parsing function.
 * 
 * @param json_str Raw JSON message string from server
 * @return 0 on success, -1 on parse error
 */
int message_parser_process(const char* json_str);

/**
 * @brief Parse init_data message and update game state
 * 
 * Handles the initial game configuration message that contains
 * grid dimensions, colors, camera settings, etc.
 * 
 * @param json_str JSON message string
 * @return 0 on success, -1 on failure
 */
int parse_init_data(const char* json_str);

/**
 * @brief Parse AOI (Area of Interest) update message
 * 
 * Handles player position updates, visible entities, and world objects.
 * This is the most frequent message type during gameplay.
 * 
 * @param json_str JSON message string
 * @return 0 on success, -1 on failure
 */
int parse_aoi_update(const char* json_str);

/**
 * @brief Parse skill/item association message
 * 
 * Handles item ID associations for the HUD system.
 * 
 * @param json_str JSON message string
 * @return 0 on success, -1 on failure
 */
int parse_skill_item_ids(const char* json_str);

/**
 * @brief Parse error message from server
 * 
 * @param json_str JSON message string
 * @return 0 on success, -1 on failure
 */
int parse_error_message(const char* json_str);

/**
 * @brief Determine message type from JSON string
 * 
 * @param json_str JSON message string
 * @return MessageType enum value
 */
MessageType get_message_type(const char* json_str);

/**
 * @brief Parse color from JSON object
 * 
 * Helper function to parse RGBA color values from JSON.
 * 
 * @param json JSON object containing color data
 * @param color Output color structure
 * @return 0 on success, -1 on failure
 */
int parse_color_rgba(const void* json, ColorRGBA* color);

/**
 * @brief Parse Vector2 position from JSON object
 * 
 * Helper function to parse X,Y coordinates from JSON.
 * 
 * @param json JSON object containing position data
 * @param pos Output Vector2 structure
 * @return 0 on success, -1 on failure
 */
int parse_position(const void* json, Vector2* pos);

/**
 * @brief Parse Vector2 dimensions from JSON object
 * 
 * Helper function to parse Width,Height from JSON.
 * 
 * @param json JSON object containing dimensions data
 * @param dims Output Vector2 structure
 * @return 0 on success, -1 on failure
 */
int parse_dimensions(const void* json, Vector2* dims);

/**
 * @brief Parse Direction enum from JSON
 * 
 * @param json JSON object or value containing direction
 * @return Direction enum value
 */
Direction parse_direction(const void* json);

/**
 * @brief Parse ObjectLayerMode enum from JSON
 * 
 * @param json JSON object or value containing mode
 * @return ObjectLayerMode enum value
 */
ObjectLayerMode parse_mode(const void* json);

/**
 * @brief Parse object layer states from JSON array
 * 
 * @param json JSON array containing object layer data
 * @param layers Output array of ObjectLayerState
 * @param max_layers Maximum number of layers to parse
 * @return Number of layers parsed, or -1 on error
 */
int parse_object_layers(const void* json, ObjectLayerState* layers, int max_layers);

/**
 * @brief Parse player object from JSON
 * 
 * @param json JSON object containing player data
 * @param player Output PlayerState structure
 * @return 0 on success, -1 on failure
 */
int parse_player_object(const void* json, PlayerState* player);

/**
 * @brief Parse visible player from JSON
 * 
 * @param json JSON object containing visible player data
 * @param player Output PlayerState structure (base EntityState fields)
 * @return 0 on success, -1 on failure
 */
int parse_visible_player(const void* json, PlayerState* player);

/**
 * @brief Parse visible bot from JSON
 * 
 * @param json JSON object containing bot data
 * @param bot Output BotState structure
 * @return 0 on success, -1 on failure
 */
int parse_visible_bot(const void* json, BotState* bot);

/**
 * @brief Parse world object from JSON
 * 
 * @param json JSON object containing world object data
 * @param obj Output WorldObject structure
 * @return 0 on success, -1 on failure
 */
int parse_world_object(const void* json, WorldObject* obj);

/**
 * @brief Parse path array from JSON
 * 
 * @param json JSON array containing path points
 * @param path Output array of Vector2 points
 * @param max_points Maximum number of points to parse
 * @return Number of points parsed, or -1 on error
 */
int parse_path(const void* json, Vector2* path, int max_points);

/**
 * @brief Create player action JSON message
 * 
 * Helper function to create JSON messages to send to server.
 * 
 * @param target_x Target X coordinate
 * @param target_y Target Y coordinate
 * @param output_buffer Buffer to store JSON string
 * @param buffer_size Size of output buffer
 * @return 0 on success, -1 on failure
 */
int create_player_action_json(float target_x, float target_y, char* output_buffer, size_t buffer_size);

/**
 * @brief Create handshake JSON message
 * 
 * @param output_buffer Buffer to store JSON string
 * @param buffer_size Size of output buffer
 * @return 0 on success, -1 on failure
 */
int create_handshake_json(char* output_buffer, size_t buffer_size);

#endif // MESSAGE_PARSER_H