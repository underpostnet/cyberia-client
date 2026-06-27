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

void game_state_reset(void) {
    g_game_state.init_received        = false;
    g_game_state.player_id[0]         = '\0';
    g_game_state.other_player_count   = 0;
    g_game_state.bot_count            = 0;
    g_game_state.resource_count       = 0;
    g_game_state.obstacle_count       = 0;
    g_game_state.foreground_count     = 0;
    g_game_state.static_count         = 0;
    g_game_state.portal_count         = 0;
    g_game_state.floor_count          = 0;
    g_game_state.full_inventory_count = 0;
}

static GameStateEntityRemovedFn s_entity_removed_cb = NULL;

void game_state_set_entity_removed_cb(GameStateEntityRemovedFn cb) {
    s_entity_removed_cb = cb;
}

/* Generic entity-slot ops over a parallel array of fixed-size records whose
 * first member is an EntityState (PlayerState, BotState). Keyed by base.id,
 * which sits at offset 0 of every record. */
static void* slot_at(void* array, size_t elem_size, int i) {
    return (char*)array + (size_t)i * elem_size;
}

static int entity_slot_update(void* array, size_t elem_size, int* count, int max,
                              const void* incoming, const char* dbg_name) {
    const EntityState* in = incoming;
    for (int i = 0; i < *count; i++) {
        EntityState* e = slot_at(array, elem_size, i);
        if (0 == strcmp(e->id, in->id)) {
            Vector2 prev = e->interp_pos;
            memcpy(e, incoming, elem_size);
            e->pos_prev   = prev;
            e->interp_pos = prev;
            return 0;
        }
    }
    if (*count >= max) {
        LOG_WARN("%s full — dropping update for %s", dbg_name, in->id);
        return -1;
    }
    memcpy(slot_at(array, elem_size, *count), incoming, elem_size);
    (*count)++;
    return 0;
}

static void entity_slot_remove(void* array, size_t elem_size, int* count, const char* id) {
    for (int i = 0; i < *count; i++) {
        EntityState* e = slot_at(array, elem_size, i);
        if (0 == strcmp(e->id, id)) {
            memmove(slot_at(array, elem_size, i),
                    slot_at(array, elem_size, i + 1),
                    (size_t)(*count - 1 - i) * elem_size);
            (*count)--;
            if (s_entity_removed_cb) { s_entity_removed_cb(id); }
            return;
        }
    }
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

int game_state_update_player(const PlayerState* player) {
    assert(player);
    return entity_slot_update(g_game_state.other_players, sizeof(PlayerState),
                              &g_game_state.other_player_count, MAX_ENTITIES,
                              player, "other_players");
}

int game_state_update_bot(const BotState* bot) {
    assert(bot);
    return entity_slot_update(g_game_state.bots, sizeof(BotState),
                              &g_game_state.bot_count, MAX_ENTITIES,
                              bot, "bots");
}

void game_state_remove_player(const char* id) {
    assert(id);
    entity_slot_remove(g_game_state.other_players, sizeof(PlayerState),
                       &g_game_state.other_player_count, id);
}

void game_state_remove_bot(const char* id) {
    assert(id);
    entity_slot_remove(g_game_state.bots, sizeof(BotState),
                       &g_game_state.bot_count, id);
}

void game_state_toggle_dev_ui(void) {
    presentation_runtime_toggle_dev_ui();
}
