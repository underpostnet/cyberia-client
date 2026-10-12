#include "domain/item_use.h"
#include "domain/local_player.h"
#include "game_state.h"
#include "network/game/client_event.h"
#include "object_layer.h"
#include "util/utils.h"
#include "world_types.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

GameState g_game_state;
static ObjectLayer s_gel;
static ObjectLayer s_sword;
static int  s_sent;
static char s_sent_item[MAX_ITEM_ID_LENGTH];

ObjectLayer* lookup_cached_layer(const char* item_id) {
    if (0 == strcmp(item_id, "gel")) return &s_gel;
    if (0 == strcmp(item_id, "sword")) return &s_sword;
    return NULL;
}
client_event_t client_event_push(client_event_kind_t kind, client_event_payload_t p) {
    assert(CLIENT_EVENT_ITEM_USE == kind);
    copy_str(s_sent_item, sizeof s_sent_item, p.item_id);
    s_sent++;
    return (client_event_t){ 0 };
}
void vault_ops_clear(void) {}

static void hold(const char* item_id, int quantity) {
    ObjectLayerState* slot = &g_local_player.inventory[g_local_player.inventory_count++];
    copy_str(slot->item_id, sizeof slot->item_id, item_id);
    slot->quantity = quantity;
}

int main(void) {
    copy_str(s_gel.data.item.type, sizeof s_gel.data.item.type, "consumable");
    copy_str(s_sword.data.item.type, sizeof s_sword.data.item.type, "weapon");
    local_player_reset();
    hold("gel", 2);
    hold("sword", 1);
    g_game_state.player.base.life = 40.0f;
    g_game_state.player.base.max_life = 100.0f;

    /* Only an owned consumable of a living, wounded player is usable. */
    assert(item_use_is_consumable("gel") && !item_use_is_consumable("sword") && !item_use_is_consumable("orb"));
    assert(ITEM_USE_NOT_CONSUMABLE == item_use_state("sword", 0.0));
    assert(ITEM_USE_READY == item_use_state("gel", 0.0));
    g_local_player.inventory[0].quantity = 0;
    assert(ITEM_USE_NOT_OWNED == item_use_state("gel", 0.0));
    g_local_player.inventory[0].quantity = 2;
    local_player_set_status_icon(STATUS_ICON_DEAD);
    assert(ITEM_USE_DEAD == item_use_state("gel", 0.0));
    local_player_set_status_icon(0);
    g_game_state.player.base.life = 100.0f;
    assert(ITEM_USE_FULL_LIFE == item_use_state("gel", 0.0));
    assert(ITEM_USE_FULL_LIFE == item_use_request("gel", 0.0) && 0 == s_sent);
    g_game_state.player.base.life = 40.0f;

    /* One request goes out and holds every other until its ack settles it. */
    assert(ITEM_USE_READY == item_use_request("gel", 0.0));
    assert(1 == s_sent && 0 == strcmp("gel", s_sent_item));
    assert(ITEM_USE_PENDING == item_use_request("gel", 0.0) && 1 == s_sent);
    assert(ITEM_USE_READY == item_use_settle(true, ""));

    /* The snapshot cooldown blocks the next use until it runs out. */
    local_player_set_action_cooldown(0.5f);
    item_use_set_ready_at(1.0);
    assert(ITEM_USE_COOLDOWN == item_use_state("gel", 0.75));
    assert(0.49f < item_use_charge(0.75) && 0.51f > item_use_charge(0.75));
    assert(ITEM_USE_READY == item_use_state("gel", 1.0) && 1.0f == item_use_charge(1.0));

    /* A rejection settles the request and names its reason. */
    assert(ITEM_USE_READY == item_use_request("gel", 1.0) && 2 == s_sent);
    assert(ITEM_USE_FULL_LIFE == item_use_settle(false, "full_life"));
    assert(ITEM_USE_COOLDOWN == item_use_settle(false, "cooldown"));
    assert(ITEM_USE_NOT_OWNED == item_use_settle(false, "not_owned"));
    assert('\0' != item_use_state_text(ITEM_USE_FULL_LIFE)[0] && '\0' == item_use_state_text(ITEM_USE_READY)[0]);

    /* A reset drops the pending request and the cooldown. */
    item_use_request("gel", 1.0);
    item_use_set_ready_at(9.0);
    local_player_reset();
    hold("gel", 1);
    assert(ITEM_USE_READY == item_use_state("gel", 1.0));

    printf("item_use_test OK\n");
    return 0;
}
