#include "game_state.h"
#include "util/log.h"

#include <assert.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include <raylib.h>

/* Authoritative world-state mirror. Camera, dev-UI, stasis flag, and
 * per-frame UI bookkeeping have been moved to their owning modules; what
 * remains here is strictly gameplay/world data. */
GameState g_game_state = {0};

/* Bump-allocated object layers. game_state_layer_pool_reset() is the only
 * free. */
ObjectLayerState g_layer_pool[LAYER_POOL_SIZE] = {0};
static int s_layer_pool_head = 0;

void game_state_layer_pool_reset(void) {
    s_layer_pool_head = 0;
}

int game_state_layer_alloc(int count) {
    if (!(0 <= count)) {
        assert(false && "negative entity count");
        LOG_ERROR("negative entity count");
        abort();
    }
    if (LAYER_POOL_SIZE - s_layer_pool_head < count) return -1;
    int offset = s_layer_pool_head;
    s_layer_pool_head += count;
    return offset;
}

/* The id → slot index: open addressing, linear probe. An entry holds slot + 1,
 * 0 is empty. Twice the slot count keeps the probes short. */
#define INDEX_CAP (2 * ENTITY_CAP)
static int32_t s_index[INDEX_CAP];
static int     s_cursor = 0;

static uint32_t index_home(EntityID id) {
    return (id * 2654435761u) & (INDEX_CAP - 1);
}

int game_state_slot(EntityID id) {
    for (uint32_t h = index_home(id); 0 != s_index[h]; h = (h + 1) & (INDEX_CAP - 1)) {
        if (id == g_game_state.ids[s_index[h] - 1]) return s_index[h] - 1;
    }
    return -1;
}

int game_state_spawn(EntityID id) {
    assert(0 != id);
    int slot = game_state_slot(id);
    if (0 <= slot) return slot;
    /* ponytail: linear scan from a cursor, a free-list head if a profile says so. */
    for (int n = 0; ENTITY_CAP > n; n++) {
        int i = (s_cursor + n) % ENTITY_CAP;
        if (0 != g_game_state.ids[i]) continue;
        g_game_state.ids[i]  = id;
        g_game_state.mask[i] = 0;
        uint32_t h = index_home(id);
        while (0 != s_index[h]) h = (h + 1) & (INDEX_CAP - 1);
        s_index[h] = i + 1;
        s_cursor = i + 1;
        return i;
    }
    return -1;
}

void game_state_despawn(EntityID id) {
    uint32_t h = index_home(id);
    while (0 != s_index[h] && id != g_game_state.ids[s_index[h] - 1]) h = (h + 1) & (INDEX_CAP - 1);
    if (0 == s_index[h]) return;
    int slot = s_index[h] - 1;
    g_game_state.ids[slot]  = 0;
    g_game_state.mask[slot] = 0;
    /* Backward-shift delete: move each later entry of the run into the gap
     * when the gap lies between its home and its place. */
    s_index[h] = 0;
    for (uint32_t j = (h + 1) & (INDEX_CAP - 1); 0 != s_index[j]; j = (j + 1) & (INDEX_CAP - 1)) {
        uint32_t home = index_home(g_game_state.ids[s_index[j] - 1]);
        if (((j - home) & (INDEX_CAP - 1)) >= ((j - h) & (INDEX_CAP - 1))) {
            s_index[h] = s_index[j];
            s_index[j] = 0;
            h = j;
        }
    }
}

const EntityTransform* game_state_own_transform(void) {
    static const EntityTransform none = {0};
    int slot = game_state_slot(g_game_state.player.base.id);
    return 0 <= slot ? &g_game_state.transform[slot] : &none;
}

const RenderState* game_state_own_render(void) {
    static const RenderState none = {0};
    int slot = game_state_slot(g_game_state.player.base.id);
    return 0 <= slot ? &g_game_state.render[slot] : &none;
}

void game_state_reset(void) {
    /* The own player keeps its slot, so it still draws until the next session
     * sends a new own id; that snapshot frees the old slot. */
    for (int i = 0; ENTITY_CAP > i; i++) {
        if (0 != g_game_state.ids[i] && g_game_state.player.base.id != g_game_state.ids[i])
            game_state_despawn(g_game_state.ids[i]);
    }
    g_game_state.init_received        = false;
    g_game_state.instance_code[0]     = '\0';
    g_game_state.other_player_count   = 0;
    g_game_state.bot_count            = 0;
    g_game_state.resource_count       = 0;
    g_game_state.scenery_count        = 0;
    g_game_state.portal_count         = 0;
    g_game_state.dead_item_id_count   = 0;
    game_state_layer_pool_reset();
}

PlayerState* game_state_find_player(EntityID id) {
    for (int i = 0; i < g_game_state.other_player_count; i++) {
        if (id == g_game_state.other_players[i].base.id)
            return &g_game_state.other_players[i];
    }
    return NULL;
}

BotState* game_state_find_bot(EntityID id) {
    for (int i = 0; i < g_game_state.bot_count; i++) {
        if (id == g_game_state.bots[i].base.id)
            return &g_game_state.bots[i];
    }
    return NULL;
}

const EntityState* game_state_find_entity(EntityID id) {
    if (0 == id || id == g_game_state.player.base.id)
        return &g_game_state.player.base;
    const BotState* bot = game_state_find_bot(id);
    if (NULL != bot) return &bot->base;
    const PlayerState* player = game_state_find_player(id);
    if (NULL != player) return &player->base;
    for (int i = 0; g_game_state.resource_count > i; i++) {
        if (id == g_game_state.resources[i].base.id) return &g_game_state.resources[i].base;
    }
    return NULL;
}
