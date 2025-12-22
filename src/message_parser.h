#ifndef MESSAGE_PARSER_H
#define MESSAGE_PARSER_H

#include "game_state.h"
#include "serial.h"
#include <stdbool.h>
#include <stddef.h>

/**
 * @file message_parser.h
 * @brief JSON message parsing for network protocol
 *
 * This module handles parsing of incoming JSON messages from the server
 * and updating the game state accordingly. Uses cJSON library and the
 * general-purpose serialization framework from serial.h.
 *
 * Refactored based on: src/serial.py and src/network_models.py
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
 * parsing function, updating the game state accordingly.
 *
 * @param json_str Raw JSON message string from server
 * @return 0 on success, -1 on parse error
 */
int message_parser_process(const char* json_str);

// /**
//  * @brief Determine message type from JSON string
//  *
//  * Parses the "type" field from the JSON message to determine
//  * which handler should process it.
//  *
//  * @param json_str JSON message string
//  * @return MessageType enum value
//  */
// MessageType message_parser_get_type(const char* json_str);

/**
 * @brief Parse init_data message and update game state
 *
 * Handles the initial game configuration message that contains
 * grid dimensions, colors, camera settings, etc.
 *
 * Message format (InitPayload from network_models.py):
 * {
 *   "type": "init_data",
 *   "payload": {
 *     "gridW": int,
 *     "gridH": int,
 *     "cellSize": float,
 *     "fps": int,
 *     "interpolationMs": int,
 *     "aoiRadius": float,
 *     "defaultObjectWidth": float,
 *     "defaultObjectHeight": float,
 *     "colors": { ... },
 *     "cameraSmoothing": float,
 *     "cameraZoom": float,
 *     "devUi": bool,
 *     "sumStatsLimit": int
 *   }
 * }
 *
 * @param json_root Parsed cJSON root object
 * @return 0 on success, -1 on failure
 */
int message_parser_parse_init_data(const cJSON* json_root);

/**
 * @brief Parse AOI (Area of Interest) update message
 *
 * Handles player position updates, visible entities, and world objects.
 * This is the most frequent message type during gameplay.
 *
 * Message format (AOIUpdatePayload from network_models.py):
 * {
 *   "type": "aoi_update",
 *   "payload": {
 *     "player": { PlayerObject },
 *     "visiblePlayers": { "id": VisiblePlayer, ... },
 *     "visibleGridObjects": {
 *       "bots": { "id": VisibleBot, ... },
 *       "obstacles": { "id": VisibleObject, ... },
 *       "portals": { "id": VisibleObject, ... },
 *       "floors": { "id": VisibleFloor, ... },
 *       "foregrounds": { "id": VisibleObject, ... }
 *     }
 *   }
 * }
 *
 * @param json_root Parsed cJSON root object
 * @return 0 on success, -1 on failure
 */
int message_parser_parse_aoi_update(const cJSON* json_root);

/**
 * @brief Parse skill/item association message
 *
 * Handles item ID associations for the HUD system.
 *
 * Message format (SkillItemIdsPayload from network_models.py):
 * {
 *   "type": "skill_item_ids",
 *   "payload": {
 *     "requestedItemId": string,
 *     "associatedItemIds": [string, ...]
 *   }
 * }
 *
 * @param json_root Parsed cJSON root object
 * @return 0 on success, -1 on failure
 */
int message_parser_parse_skill_item_ids(const cJSON* json_root);

/**
 * @brief Parse error message from server
 *
 * Message format:
 * {
 *   "type": "error",
 *   "payload": {
 *     "message": string
 *   }
 * }
 *
 * @param json_root Parsed cJSON root object
 * @return 0 on success, -1 on failure
 */
int message_parser_parse_error(const cJSON* json_root);

/**
 * @brief Parse color dictionary from init_data
 *
 * Parses the colors object and updates game state color palette.
 *
 * @param colors_json cJSON object containing color dictionary
 * @return 0 on success, -1 on failure
 */
int message_parser_parse_colors(const cJSON* colors_json);

/**
 * @brief Parse visible players dictionary
 *
 * Parses the visiblePlayers object and updates game state.
 *
 * @param players_json cJSON object containing player dictionary
 * @return 0 on success, -1 on failure
 */
int message_parser_parse_visible_players(const cJSON* players_json);

// /**
//  * @brief Parse visible bots dictionary
//  *
//  * Parses the bots object from visibleGridObjects.
//  *
//  * @param bots_json cJSON object containing bot dictionary
//  * @return 0 on success, -1 on failure
//  */
// int message_parser_parse_visible_bots(const cJSON* bots_json);

// /**
//  * @brief Parse visible obstacles dictionary
//  *
//  * Parses the obstacles object from visibleGridObjects.
//  *
//  * @param obstacles_json cJSON object containing obstacle dictionary
//  * @return 0 on success, -1 on failure
//  */
// int message_parser_parse_visible_obstacles(const cJSON* obstacles_json);

// /**
//  * @brief Parse visible portals dictionary
//  *
//  * Parses the portals object from visibleGridObjects.
//  *
//  * @param portals_json cJSON object containing portal dictionary
//  * @return 0 on success, -1 on failure
//  */
// int message_parser_parse_visible_portals(const cJSON* portals_json);

// /**
//  * @brief Parse visible floors dictionary
//  *
//  * Parses the floors object from visibleGridObjects.
//  *
//  * @param floors_json cJSON object containing floor dictionary
//  * @return 0 on success, -1 on failure
//  */
// int message_parser_parse_visible_floors(const cJSON* floors_json);

// /**
//  * @brief Parse visible foregrounds dictionary
//  *
//  * Parses the foregrounds object from visibleGridObjects.
//  *
//  * @param foregrounds_json cJSON object containing foreground dictionary
//  * @return 0 on success, -1 on failure
//  */
// int message_parser_parse_visible_foregrounds(const cJSON* foregrounds_json);

#endif // MESSAGE_PARSER_H