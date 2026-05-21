#include "input_command.h"
#include "network/session.h"

#include <string.h>

/* Helper — fills the common tick + sequence header. Allocating the
 * sequence in one place ensures it is monotonic regardless of which build
 * helper the caller invoked. */
static void stamp_header(input_command_t* cmd, input_kind_t kind) {
    cmd->kind         = kind;
    cmd->client_tick  = session_server_tick_estimate();
    cmd->sequence     = session_next_input_sequence();
}

input_command_t input_command_build_tap(float grid_x, float grid_y) {
    input_command_t cmd = {0};
    stamp_header(&cmd, INPUT_KIND_PLAYER_ACTION);
    cmd.target_x = grid_x;
    cmd.target_y = grid_y;
    return cmd;
}

input_command_t input_command_build_item_activation(const char* item_id, bool active) {
    input_command_t cmd = {0};
    stamp_header(&cmd, INPUT_KIND_ITEM_ACTIVATION);
    if (item_id) {
        strncpy(cmd.item_id, item_id, INPUT_MAX_ITEM_ID_LEN - 1);
    }
    cmd.active = active;
    return cmd;
}

input_command_t input_command_build_chat(const char* to_id, const char* text) {
    input_command_t cmd = {0};
    stamp_header(&cmd, INPUT_KIND_CHAT);
    if (to_id) {
        strncpy(cmd.item_id, to_id, INPUT_MAX_ITEM_ID_LEN - 1);
    }
    if (text) {
        strncpy(cmd.chat_text, text, INPUT_MAX_CHAT_LEN - 1);
    }
    return cmd;
}

input_command_t input_command_build_freeze(bool start, const char* reason) {
    input_command_t cmd = {0};
    stamp_header(&cmd, start ? INPUT_KIND_FREEZE_START : INPUT_KIND_FREEZE_END);
    if (reason) {
        strncpy(cmd.reason, reason, INPUT_MAX_REASON_LEN - 1);
    }
    return cmd;
}

input_command_t input_command_build_get_items_ids(const char* item_id) {
    input_command_t cmd = {0};
    stamp_header(&cmd, INPUT_KIND_GET_ITEMS_IDS);
    if (item_id) {
        strncpy(cmd.item_id, item_id, INPUT_MAX_ITEM_ID_LEN - 1);
    }
    return cmd;
}
