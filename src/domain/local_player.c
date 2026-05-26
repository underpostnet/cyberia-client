#include "local_player.h"

#include <string.h>

#define LOCAL_PLAYER_DEFAULT_MOVE_SPEED 3.0f

static struct {
    bool          frozen;
    uint8_t       status_icon;
    float         move_speed;
    LocalFctEvent fct[LOCAL_FCT_PENDING_MAX];
    int           fct_count;
} g_local = {
    .move_speed = LOCAL_PLAYER_DEFAULT_MOVE_SPEED,
};

void local_player_reset(void) {
    g_local.frozen      = false;
    g_local.status_icon = 0;
    g_local.move_speed  = LOCAL_PLAYER_DEFAULT_MOVE_SPEED;
    g_local.fct_count   = 0;
}

void local_player_set_frozen(bool frozen) { g_local.frozen = frozen; }
bool local_player_is_frozen(void)         { return g_local.frozen; }

void    local_player_set_status_icon(uint8_t id) { g_local.status_icon = id; }
uint8_t local_player_status_icon(void)           { return g_local.status_icon; }

void  local_player_set_move_speed(float speed) {
    if (speed > 0.0f) g_local.move_speed = speed;
}
float local_player_move_speed(void) { return g_local.move_speed; }

bool local_player_fct_push(const LocalFctEvent* ev) {
    if (!ev || g_local.fct_count >= LOCAL_FCT_PENDING_MAX) return false;
    g_local.fct[g_local.fct_count++] = *ev;
    return true;
}

int local_player_fct_count(void) { return g_local.fct_count; }

const LocalFctEvent* local_player_fct_at(int idx) {
    if (idx < 0 || idx >= g_local.fct_count) return NULL;
    return &g_local.fct[idx];
}

void local_player_fct_clear(void) { g_local.fct_count = 0; }
