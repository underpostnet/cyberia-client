#ifndef MESSAGE_PARSER_H
#define MESSAGE_PARSER_H

#include "game_state.h"
#include "serial.h"

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

#endif // MESSAGE_PARSER_H
