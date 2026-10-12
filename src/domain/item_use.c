#include "item_use.h"

#include "domain/local_player.h"
#include "game_state.h"
#include "network/game/client_event.h"
#include "object_layers_management.h"
#include "util/utils.h"
#include "world_types.h"

#include <string.h>

static bool   s_pending  = false;
static double s_ready_at = 0.0;

bool item_use_is_consumable(const char* item_id) {
    const ObjectLayer* data = lookup_cached_layer(item_id);
    return NULL != data && 0 == strcmp(data->data.item.type, "consumable");
}

float item_use_charge(double now) {
    double left = s_ready_at - now;
    if (0.0 >= left) return 1.0f;
    double charge = 1.0 - left / (double)local_player_action_cooldown();
    return 0.0 < charge ? (float)charge : 0.0f;
}

item_use_state_e item_use_state(const char* item_id, double now) {
    const EntityState* self = &g_game_state.player.base;
    if (!item_use_is_consumable(item_id))                 return ITEM_USE_NOT_CONSUMABLE;
    if (1 > local_player_item_quantity(item_id))          return ITEM_USE_NOT_OWNED;
    if (STATUS_ICON_DEAD == local_player_status_icon())   return ITEM_USE_DEAD;
    if (s_pending)                                        return ITEM_USE_PENDING;
    if (1.0f > item_use_charge(now))                      return ITEM_USE_COOLDOWN;
    if (self->life >= self->max_life)                     return ITEM_USE_FULL_LIFE;
    return ITEM_USE_READY;
}

const char* item_use_state_text(item_use_state_e state) {
    switch (state) {
    case ITEM_USE_READY:          return "";
    case ITEM_USE_NOT_CONSUMABLE: return "Not usable";
    case ITEM_USE_NOT_OWNED:      return "None left";
    case ITEM_USE_DEAD:           return "Not while fragmented";
    case ITEM_USE_PENDING:        return "Using...";
    case ITEM_USE_COOLDOWN:       return "Not ready";
    case ITEM_USE_FULL_LIFE:      return "Life is full";
    }
    return "";
}

item_use_state_e item_use_request(const char* item_id, double now) {
    item_use_state_e state = item_use_state(item_id, now);
    if (ITEM_USE_READY != state) return state;
    client_event_payload_t p = {0};
    copy_str(p.item_id, sizeof p.item_id, item_id);
    client_event_push(CLIENT_EVENT_ITEM_USE, p);
    s_pending = true;
    return state;
}

item_use_state_e item_use_settle(bool ok, const char* reason) {
    s_pending = false;
    if (ok || NULL == reason)                  return ITEM_USE_READY;
    if (0 == strcmp(reason, "full_life"))      return ITEM_USE_FULL_LIFE;
    if (0 == strcmp(reason, "cooldown"))       return ITEM_USE_COOLDOWN;
    if (0 == strcmp(reason, "dead"))           return ITEM_USE_DEAD;
    if (0 == strcmp(reason, "not_owned"))      return ITEM_USE_NOT_OWNED;
    return ITEM_USE_NOT_CONSUMABLE;
}

void item_use_set_ready_at(double time_s) { s_ready_at = time_s; }

void item_use_reset(void) {
    s_pending  = false;
    s_ready_at = 0.0;
}
