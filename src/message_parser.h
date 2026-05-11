#ifndef MESSAGE_PARSER_H
#define MESSAGE_PARSER_H

#include <stdbool.h>

/**
 * @brief Message types from server
 */
typedef enum {
    MSG_TYPE_UNKNOWN = 0,
    MSG_TYPE_INIT_DATA,
    MSG_TYPE_AOI_UPDATE,
    MSG_TYPE_SKILL_ITEM_IDS,
    MSG_TYPE_METADATA,
    MSG_TYPE_CHAT,
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
 * @return true on success, false on parse error
 */
bool message_parser_parse(const char* json_str);

#endif // MESSAGE_PARSER_H
