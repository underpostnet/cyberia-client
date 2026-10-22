/* Player containers and chat on the client: the capacity helpers mirror the
 * server rule, item moves split, stack and swap slotted stacks under the
 * server rules, and the chat log files lines by conversation. */
#include "sim/container_ops.h"
#include "sim/game_state.h"
#include "sim/local_player.h"
#include "net/client_event.h"
#include "ui/chat_log.h"
#include "world/object_layer.h"
#include "world/object_layers_management.h"
#include "utils.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

GameState g_game_state;
static const EntityID OWN = 7;
static ObjectLayer s_coin_layer;
static client_event_kind_t s_pushed_kind;
static client_event_payload_t s_pushed_payload;
static int s_pushed;

int game_state_slot(EntityID id) { return OWN == id ? 0 : -1; }
ObjectLayer* lookup_cached_layer(const char* item_id) {
    return 0 == strcmp("coin", item_id) ? &s_coin_layer : NULL;
}
void client_event_push(client_event_kind_t kind, client_event_payload_t p) {
    s_pushed_kind = kind;
    s_pushed_payload = p;
    s_pushed++;
}
double GetTime(void) { return 0.0; }

/* Carry a stack in the next bag slot; the coin goes to the wallet. */
static void carry(const char* item_id, int qty) {
    int slot = 0;
    for (int i = 0; i < g_local_player.inventory_count; i++) {
        if (g_local_player.inventory[i].slot >= slot) slot = g_local_player.inventory[i].slot + 1;
    }
    ObjectLayerState* s = &g_local_player.inventory[g_local_player.inventory_count++];
    memset(s, 0, sizeof *s);
    copy_str(s->item_id, sizeof s->item_id, item_id);
    s->quantity = qty;
    s->slot = 0 == strcmp("coin", item_id) ? BAG_WALLET_SLOT : slot;
    if (0 == strcmp("coin", item_id)) g_local_player.coins = qty;
}

static void empty_all(void) {
    g_local_player.inventory_count = 0;
    g_local_player.storage.count = 0;
    g_local_player.vault_count = 0;
    g_local_player.coins = 0;
}

static const ObjectLayerState* at(Box box, int slot) { return container_ops_stack(box, slot); }

static void set_level(int level) {
    g_game_state.mask[0] |= COMP_PROGRESS;
    g_game_state.progress[0].level = level;
}

static void test_capacity_follows_the_level(void) {
    set_level(1);
    assert(8 == local_player_open_slots(CYBERIA_INVENTORY_CAPACITY));
    assert(16 == local_player_open_slots(CYBERIA_STORAGE_CAPACITY));
    set_level(49);
    assert(24 == local_player_open_slots(CYBERIA_INVENTORY_CAPACITY));
    set_level(1);

    /* Slot 8 opens at level 4, slot 9 at 7; storage opens two slots per step. */
    g_game_state.max_level = 100;
    assert(1 == local_player_unlock_level(CYBERIA_INVENTORY_CAPACITY, 0));
    assert(4 == local_player_unlock_level(CYBERIA_INVENTORY_CAPACITY, 8));
    assert(7 == local_player_unlock_level(CYBERIA_INVENTORY_CAPACITY, 9));
    assert(6 == local_player_unlock_level(CYBERIA_STORAGE_CAPACITY, 16));
    assert(6 == local_player_unlock_level(CYBERIA_STORAGE_CAPACITY, 17));
    assert(11 == local_player_unlock_level(CYBERIA_STORAGE_CAPACITY, 18));
    /* A slot past the level cap of the world never opens. */
    g_game_state.max_level = 5;
    assert(0 == local_player_unlock_level(CYBERIA_INVENTORY_CAPACITY, 9));
    g_game_state.max_level = 100;
}

static void test_bag_room_counts_stacks_and_skips_coins(void) {
    g_local_player.inventory_count = 0;
    carry("coin", 50);
    for (int i = 0; i < 8; i++) {
        char id[16];
        snprintf(id, sizeof id, "item-%d", i);
        carry(id, 1);
    }
    assert(8 == local_player_inventory_used());
    assert(!local_player_inventory_has_room("new"));
    assert(local_player_inventory_has_room("item-3"));
    assert(local_player_inventory_has_room("coin"));
}

/* Inside the bag a move onto an empty slot splits, onto the same item stacks,
 * and onto another item swaps two whole stacks. Part of a stack never
 * displaces another item, and a locked slot takes nothing. */
static void test_moves_split_stack_and_swap_in_the_bag(void) {
    empty_all();
    carry("gem", 5);
    carry("ore", 1);
    s_pushed = 0;

    container_ops_move(BOX_BAG, 0, BOX_BAG, 3, 2);
    assert(3 == at(BOX_BAG, 0)->quantity && 2 == at(BOX_BAG, 3)->quantity && 5 == local_player_item_quantity("gem"));
    assert(1 == s_pushed && CLIENT_EVENT_ITEM_MOVE == s_pushed_kind && 3 == s_pushed_payload.to_slot);
    container_ops_move(BOX_BAG, 3, BOX_BAG, 0, 2);
    assert(5 == at(BOX_BAG, 0)->quantity && NULL == at(BOX_BAG, 3));
    container_ops_move(BOX_BAG, 0, BOX_BAG, 1, 5);
    assert(0 == strcmp("ore", at(BOX_BAG, 0)->item_id) && 0 == strcmp("gem", at(BOX_BAG, 1)->item_id));

    container_ops_move(BOX_BAG, 1, BOX_BAG, 0, 2);
    assert(3 == s_pushed && 5 == at(BOX_BAG, 1)->quantity);
    assert(MOVE_REFUSED == container_ops_check(BOX_BAG, 1, BOX_BAG, 8, 1));
    assert(MOVE_SWAP == container_ops_check(BOX_BAG, 1, BOX_BAG, 0, 5));
    assert(MOVE_STACK != container_ops_check(BOX_BAG, 1, BOX_BAG, 0, 5));

    /* An open trade holds every stack where it is. */
    g_local_player.trade.state = TRADE_OPEN;
    assert(MOVE_REFUSED == container_ops_check(BOX_BAG, 1, BOX_BAG, 0, 5));
    g_local_player.trade.state = TRADE_NONE;
}

/* Units cross between the bag and the storage onto an empty slot or onto the
 * same item; another item and the wallet refuse them. */
static void test_moves_cross_between_the_bag_and_storage(void) {
    empty_all();
    carry("gem", 5);
    carry("ore", 1);
    carry("coin", 30);

    container_ops_move(BOX_BAG, 0, BOX_STORAGE, 4, 2);
    container_ops_move(BOX_BAG, 0, BOX_STORAGE, 4, 1);
    assert(3 == at(BOX_STORAGE, 4)->quantity && 2 == at(BOX_BAG, 0)->quantity);
    assert(MOVE_REFUSED == container_ops_check(BOX_BAG, 1, BOX_STORAGE, 4, 1));
    assert(MOVE_REFUSED == container_ops_check(BOX_BAG, BAG_WALLET_SLOT, BOX_STORAGE, 0, 5));
    assert(MOVE_STACK == container_ops_check(BOX_STORAGE, 4, BOX_BAG, 0, 3));

    container_ops_move(BOX_STORAGE, 4, BOX_BAG, 5, 9);
    assert(3 == at(BOX_BAG, 5)->quantity && 0 == g_local_player.storage.count);
}

/* A full bag refuses a new stack and still takes more of an item it holds. */
static void test_a_full_bag_takes_only_more_of_a_held_item(void) {
    empty_all();
    for (int i = 0; i < 8; i++) {
        char id[16];
        snprintf(id, sizeof id, "item-%d", i);
        carry(id, 1);
    }
    g_local_player.storage.count = 2;
    g_local_player.storage.items[0] = (ObjectLayerState){ .item_id = "z", .quantity = 1, .slot = 0 };
    g_local_player.storage.items[1] = (ObjectLayerState){ .item_id = "item-0", .quantity = 4, .slot = 1 };

    assert(MOVE_REFUSED == container_ops_check(BOX_STORAGE, 0, BOX_BAG, 0, 1));
    container_ops_move(BOX_STORAGE, 1, BOX_BAG, 0, 4);
    assert(5 == at(BOX_BAG, 0)->quantity && 1 == g_local_player.storage.count);
}

/* A vault move names the bound terminal, trades coins with the wallet, and
 * replays the same way. A replay for a terminal out of view changes nothing. */
static void test_vault_moves_name_their_terminal_and_replay(void) {
    empty_all();
    carry("coin", 30);
    container_ops_bind_vault(3, "bank", 4);

    container_ops_move(BOX_BAG, BAG_WALLET_SLOT, BOX_VAULT, 2, 20);
    assert(3 == s_pushed_payload.entity_id && 10 == local_player_coins());
    assert(20 == at(BOX_VAULT, 2)->quantity && 0 == strcmp("bank", g_local_player.vaults[0].action_code));
    assert(MOVE_REFUSED == container_ops_check(BOX_BAG, BAG_WALLET_SLOT, BOX_VAULT, 4, 1));

    client_event_payload_t sent = s_pushed_payload;
    empty_all();
    carry("coin", 30);
    container_ops_apply(&sent);
    assert(10 == local_player_coins() && 20 == at(BOX_VAULT, 2)->quantity);

    sent.entity_id = 99;
    container_ops_apply(&sent);
    assert(10 == local_player_coins());
}

static void test_chat_log_files_lines_by_conversation(void) {
    chat_log_reset();
    chat_log_push(chat_channel(CHAT_CHANNEL_WORLD), 2, "Guest-AB", "hello", 1.7e12, false);
    chat_log_push(chat_channel(CHAT_CHANNEL_WORLD), OWN, "Guest-CD", "hi", 1.7e12, true);
    chat_log_push(chat_direct(2), 2, "Guest-AB", "psst", 1.7e12, false);
    chat_log_push_system(chat_channel(CHAT_CHANNEL_PARTY), "You are not in a party.");

    assert(1 == chat_log_unread(chat_channel(CHAT_CHANNEL_WORLD)));
    assert(1 == chat_log_unread_direct());
    assert(0 == chat_log_unread(chat_channel(CHAT_CHANNEL_PARTY)));
    const ChatHistory* party = chat_log_get(chat_channel(CHAT_CHANNEL_PARTY));
    assert(NULL != party && 0 == party->lines[0].from_id && 0.0 < party->lines[0].at_ms);
    assert(1.7e12 == chat_log_get(chat_channel(CHAT_CHANNEL_WORLD))->lines[0].at_ms);

    chat_log_mark_read(chat_channel(CHAT_CHANNEL_WORLD));
    assert(0 == chat_log_unread(chat_channel(CHAT_CHANNEL_WORLD)));

    for (int i = 0; i < CHAT_LOG_LINES + 5; i++) chat_log_push(chat_direct(2), 2, "Guest-AB", "spam", 1.7e12, false);
    assert(CHAT_LOG_LINES == chat_log_get(chat_direct(2))->count);

    chat_log_reset();
    assert(NULL == chat_log_get(chat_channel(CHAT_CHANNEL_WORLD)));
}

int main(void) {
    copy_str(s_coin_layer.data.item.type, sizeof s_coin_layer.data.item.type, "coin");
    g_local_player.id = OWN;
    g_game_state.mask[0] |= COMP_VITALS;
    test_capacity_follows_the_level();
    test_bag_room_counts_stacks_and_skips_coins();
    test_moves_split_stack_and_swap_in_the_bag();
    test_moves_cross_between_the_bag_and_storage();
    test_a_full_bag_takes_only_more_of_a_held_item();
    test_vault_moves_name_their_terminal_and_replay();
    test_chat_log_files_lines_by_conversation();
    puts("player_containers_test OK");
    return 0;
}
