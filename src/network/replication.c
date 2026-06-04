#include "network/replication.h"

#include "game_state.h"
#include "serial.h"
#include "input/input_command.h"
#include "input/input_queue.h"
#include "prediction/prediction.h"
#include "network/game_client.h"

#include <raylib.h>

static bool send_event_tap(Vector2 grid, uint32_t client_tick, uint32_t sequence) {
    BinWriter w;
    uplink_player_action(&w, grid.x, grid.y, client_tick, sequence);
    return network_send_binary(w.buf, w.pos);
}

void replication_prepare_input(input_queue_t in_queue) {
    // in_queue is a deep copy, safe to drain.
    input_event_t evt = { 0 };
    while (input_pop(&in_queue, &evt)) {
        if (INPUT_TAP == evt.type) {
            float cell = g_game_state.cell_size > 0.0f ? g_game_state.cell_size : 12.0f;
            float gx = evt.world_position.x / cell;
            float gy = evt.world_position.y / cell;
            input_command_t cmd = input_command_build_tap(gx, gy);
            prediction_enqueue_input(&cmd);
            send_event_tap((Vector2){gx, gy}, cmd.client_tick, cmd.sequence);
            g_game_state.player.tap_target     = (Vector2){gx, gy};
            g_game_state.player.has_tap_target = true;
        }
    }
}
