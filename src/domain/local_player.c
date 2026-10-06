#include "local_player.h"

#include <string.h>

#include "domain/vault_ops.h"
#include "network/game/client_event.h"
#include "util/utils.h"

#define LOCAL_PLAYER_DEFAULT_MOVE_SPEED 3.0f
/* Mirrors entityBaseActionCooldownMs, the instance default. Only in force
 * until the first snapshot carries the player's own, Utility-reduced value. */
#define LOCAL_PLAYER_DEFAULT_ACTION_COOLDOWN_S 0.5f

static struct {
    bool          stasis;
    uint8_t       status_icon;
    float         move_speed;
    float         action_cooldown_s;
    bool          on_portal;
    float         portal_hold_progress;
    LocalFctEvent fct[LOCAL_FCT_PENDING_MAX];
    int           fct_count;
} g_local = {
    .move_speed        = LOCAL_PLAYER_DEFAULT_MOVE_SPEED,
    .action_cooldown_s = LOCAL_PLAYER_DEFAULT_ACTION_COOLDOWN_S,
};

LocalPlayer g_local_player = {0};

void local_player_reset(void) {
    memset(&g_local_player, 0, sizeof(g_local_player));
    g_local.stasis               = false;
    g_local.status_icon          = 0;
    g_local.move_speed           = LOCAL_PLAYER_DEFAULT_MOVE_SPEED;
    g_local.action_cooldown_s    = LOCAL_PLAYER_DEFAULT_ACTION_COOLDOWN_S;
    g_local.on_portal            = false;
    g_local.portal_hold_progress = 0.0f;
    g_local.fct_count            = 0;
    vault_ops_clear();
}

void local_player_set_stasis(bool stasis) { g_local.stasis = stasis; }
bool local_player_in_stasis(void)         { return g_local.stasis; }

/* Push one event that names an entity and an item. */
static void push_entity_item(client_event_kind_t kind, const char* entity_id,
                             const char* item_id) {
    client_event_payload_t p = {0};
    copy_str(p.entity_id, sizeof p.entity_id, entity_id);
    copy_str(p.item_id, sizeof p.item_id, item_id);
    client_event_push(kind, p);
}

void local_player_request_quest_abandon(const char* quest_code) {
    push_entity_item(CLIENT_EVENT_QUEST_ABANDON, NULL, quest_code);
}

void local_player_request_quest_accept(const char* entity_id, const char* quest_code) {
    push_entity_item(CLIENT_EVENT_QUEST_ACCEPT, entity_id, quest_code);
}

void local_player_request_shop_buy(const char* entity_id, const char* item_id,
                                   int quantity) {
    if (quantity < 1) quantity = 1;
    if (quantity > 255) quantity = 255;
    client_event_payload_t p = { .quantity = quantity };
    copy_str(p.entity_id, sizeof p.entity_id, entity_id);
    copy_str(p.item_id, sizeof p.item_id, item_id);
    client_event_push(CLIENT_EVENT_SHOP_BUY, p);
}

void local_player_request_craft(const char* entity_id, int recipe_index) {
    client_event_payload_t p = { .recipe_index = recipe_index };
    copy_str(p.entity_id, sizeof p.entity_id, entity_id);
    client_event_push(CLIENT_EVENT_CRAFT_ITEM, p);
}

void local_player_request_craft_cancel(void) {
    client_event_push(CLIENT_EVENT_CRAFT_CANCEL, (client_event_payload_t){0});
}

void local_player_request_storage_open(const char* entity_id) {
    push_entity_item(CLIENT_EVENT_STORAGE_OPEN, entity_id, NULL);
}

void    local_player_set_status_icon(uint8_t id) { g_local.status_icon = id; }
uint8_t local_player_status_icon(void)           { return g_local.status_icon; }

void  local_player_set_move_speed(float speed) {
    if (speed > 0.0f) g_local.move_speed = speed;
}
float local_player_move_speed(void) { return g_local.move_speed; }

void local_player_set_action_cooldown(float seconds) {
    if (seconds > 0.0f) g_local.action_cooldown_s = seconds;
}
float local_player_action_cooldown(void) { return g_local.action_cooldown_s; }

void local_player_set_portal_hold(bool on_portal, float progress) {
    g_local.on_portal = on_portal;
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    g_local.portal_hold_progress = progress;
}
bool  local_player_on_portal(void)             { return g_local.on_portal; }
float local_player_portal_hold_progress(void)  { return g_local.portal_hold_progress; }

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
