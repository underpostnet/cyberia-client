#include "game_state.h"

#include <assert.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include <raylib.h>

#include "domain/presentation_runtime.h"
#include "util/log.h"

/* Authoritative world-state mirror. Camera, dev-UI, frozen flag, and
 * per-frame UI bookkeeping have been moved to their owning modules; what
 * remains here is strictly gameplay/world data. */
GameState g_game_state = {0};

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

int game_state_update_player(const PlayerState* player) {
    assert(player);
    PlayerState* existing = game_state_find_player(player->base.id);
    if (existing) {
        Vector2 prev = existing->base.interp_pos;
        *existing = *player;
        existing->base.pos_prev   = prev;
        existing->base.interp_pos = prev;
        return 0;
    }
    if (g_game_state.other_player_count >= MAX_ENTITIES) {
        LOG_WARN("other_players full — dropping update for %s", player->base.id);
        return -1;
    }
    g_game_state.other_players[g_game_state.other_player_count++] = *player;
    return 0;
}

int game_state_update_bot(const BotState* bot) {
    assert(bot);
    BotState* existing = game_state_find_bot(bot->base.id);
    if (existing) {
        Vector2 prev = existing->base.interp_pos;
        *existing = *bot;
        existing->base.pos_prev   = prev;
        existing->base.interp_pos = prev;
        return 0;
    }
    if (g_game_state.bot_count >= MAX_ENTITIES) {
        LOG_WARN("bots full — dropping update for %s", bot->base.id);
        return -1;
    }
    g_game_state.bots[g_game_state.bot_count++] = *bot;
    return 0;
}

void game_state_remove_player(const char* id) {
    assert(id);
    for (int i = 0; i < g_game_state.other_player_count; i++) {
        if (strcmp(g_game_state.other_players[i].base.id, id) == 0) {
            for (int j = i; j < g_game_state.other_player_count - 1; j++) {
                g_game_state.other_players[j] = g_game_state.other_players[j + 1];
            }
            g_game_state.other_player_count--;
            return;
        }
    }
}

void game_state_remove_bot(const char* id) {
    assert(id);
    for (int i = 0; i < g_game_state.bot_count; i++) {
        if (strcmp(g_game_state.bots[i].base.id, id) == 0) {
            for (int j = i; j < g_game_state.bot_count - 1; j++) {
                g_game_state.bots[j] = g_game_state.bots[j + 1];
            }
            g_game_state.bot_count--;
            return;
        }
    }
}

void game_state_toggle_dev_ui(void) {
    presentation_runtime_toggle_dev_ui();
}
