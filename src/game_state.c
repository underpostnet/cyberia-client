#include "game_state.h"

#include <assert.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include <raylib.h>

/* Authoritative world-state mirror. Camera, dev-UI, frozen flag, and
 * per-frame UI bookkeeping have been moved to their owning modules; what
 * remains here is strictly gameplay/world data. */
GameState g_game_state = {0};

void game_state_reset(void) {
    g_game_state.init_received        = false;
    g_game_state.player_id[0]         = '\0';
    g_game_state.instance_code[0]     = '\0';
    g_game_state.other_player_count   = 0;
    g_game_state.bot_count            = 0;
    g_game_state.resource_count       = 0;
    g_game_state.obstacle_count       = 0;
    g_game_state.foreground_count     = 0;
    g_game_state.static_count         = 0;
    g_game_state.portal_count         = 0;
    g_game_state.floor_count          = 0;
    g_game_state.full_inventory_count = 0;
    g_game_state.dead_item_id_count   = 0;
}

PlayerState* game_state_find_player(const char* id) {
    assert(id);
    for (int i = 0; i < g_game_state.other_player_count; i++) {
        if (strcmp(g_game_state.other_players[i].base.id, id) == 0)
            return &g_game_state.other_players[i];
    }
    return NULL;
}

BotState* game_state_find_bot(const char* id) {
    assert(id);
    for (int i = 0; i < g_game_state.bot_count; i++) {
        if (strcmp(g_game_state.bots[i].base.id, id) == 0)
            return &g_game_state.bots[i];
    }
    return NULL;
}
