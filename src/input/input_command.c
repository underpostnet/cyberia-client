#include "input_command.h"
#include "network/session.h"

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
