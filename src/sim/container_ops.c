#include "container_ops.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "local_player.h"
#include "game_state.h"
#include "net/client_event.h"
#include "world/object_layer.h"
#include "world/object_layers_management.h"
#include "world/world_types.h"
#include "log.h"
#include "utils.h"

/* The vault the open panel shows. */
static struct {
    EntityID terminal_id;
    char     action_code[MAX_ID_LENGTH];
    int      capacity;
} s_vault;

/* One box as a list of slotted stacks. */
typedef struct {
    ObjectLayerState* items;
    int*              count;
    int               cap;
} StackList;

void container_ops_bind_vault(EntityID terminal_id, const char* action_code, int capacity) {
    s_vault.terminal_id = terminal_id;
    s_vault.capacity    = capacity;
    copy_str(s_vault.action_code, sizeof s_vault.action_code, action_code);
}

/* The vault store at `terminal`, made empty on first use. NULL when the
 * terminal is out of view or the store is full. */
static Stacks* vault_of(EntityID terminal) {
    const char* code = s_vault.action_code;
    if (terminal != s_vault.terminal_id) {
        int slot = game_state_slot(terminal);
        if (0 > slot || 0 == (g_game_state.mask[slot] & COMP_AGENT)) return NULL;
        code = g_game_state.agent[slot].action_code;
    }
    if ('\0' == code[0]) return NULL;
    Vault* vault = local_player_vault(code);
    if (NULL != vault) return &vault->stacks;
    if (VAULTS_MAX <= g_local_player.vault_count) return NULL;
    vault = &g_local_player.vaults[g_local_player.vault_count++];
    memset(vault, 0, sizeof *vault);
    copy_str(vault->action_code, sizeof vault->action_code, code);
    return &vault->stacks;
}

static bool list_of(Box box, EntityID terminal, StackList* out) {
    Stacks* stacks = NULL;
    switch (box) {
        case BOX_BAG:
            *out = (StackList){ g_local_player.inventory, &g_local_player.inventory_count, INVENTORY_LAYERS_MAX };
            return true;
        case BOX_STORAGE: stacks = &g_local_player.storage; break;
        case BOX_VAULT:   stacks = vault_of(terminal); break;
    }
    if (NULL == stacks) return false;
    *out = (StackList){ stacks->items, &stacks->count, VAULT_SLOTS_MAX };
    return true;
}

/* The coin stack the wallet shows, or NULL. */
static ObjectLayerState* wallet(void) {
    for (int i = 0; i < g_local_player.inventory_count; i++) {
        if (local_player_is_coin(g_local_player.inventory[i].item_id)) return &g_local_player.inventory[i];
    }
    return NULL;
}

static ObjectLayerState* place(Box box, int slot, EntityID terminal) {
    if (BOX_BAG == box && BAG_WALLET_SLOT == slot) return wallet();
    StackList list;
    if (0 > slot || !list_of(box, terminal, &list)) return NULL;
    for (int i = 0; i < *list.count; i++) {
        if (slot == list.items[i].slot) return &list.items[i];
    }
    return NULL;
}

static int vault_capacity(EntityID terminal) {
    return terminal == s_vault.terminal_id ? s_vault.capacity : VAULT_SLOTS_MAX;
}

/* Whether a slot may receive item_id: the wallet takes coins alone, the
 * storage takes no coins, and a slot must be open. */
static bool takes(Box box, int slot, const char* item_id, EntityID terminal) {
    bool coin = local_player_is_coin(item_id);
    switch (box) {
        case BOX_BAG:
            if (BAG_WALLET_SLOT == slot) return coin && NULL != wallet();
            return !coin && 0 <= slot && slot < local_player_open_slots(CYBERIA_INVENTORY_CAPACITY);
        case BOX_STORAGE: return !coin && 0 <= slot && slot < local_player_open_slots(CYBERIA_STORAGE_CAPACITY);
        case BOX_VAULT:   return 0 <= slot && slot < vault_capacity(terminal);
    }
    return false;
}

static const char* item_type_of(const char* item_id) {
    ObjectLayer* layer = lookup_cached_layer(item_id);
    return NULL != layer ? layer->data.item.type : "";
}

/* Whether a worn stack may be taken off to be banked: requireSkin keeps the
 * last worn skin on. */
static bool bankable_while_worn(const ObjectLayerState* worn) {
    if (!g_game_state.equipment_rules.require_skin || 0 != strcmp(item_type_of(worn->item_id), "skin")) return true;
    for (int i = 0; i < g_local_player.inventory_count; i++) {
        const ObjectLayerState* other = &g_local_player.inventory[i];
        if (other->active && 0 != strcmp(other->item_id, worn->item_id) &&
            0 == strcmp(item_type_of(other->item_id), "skin")) return true;
    }
    return false;
}

static MoveKind check(Box from, int from_slot, Box to, int to_slot, int qty, EntityID terminal) {
    StackList reach;
    if ((from == to && from_slot == to_slot) || local_player_trading()) return MOVE_REFUSED;
    if (0 < game_state_vitals(g_local_player.id)->respawn_in || !list_of(to, terminal, &reach)) return MOVE_REFUSED;
    const ObjectLayerState* src = place(from, from_slot, terminal);
    if (NULL == src || 1 > src->quantity || 1 > qty || !takes(to, to_slot, src->item_id, terminal)) return MOVE_REFUSED;
    bool whole = qty >= src->quantity;
    if (BOX_BAG == from && src->active && whole && BOX_BAG != to && !bankable_while_worn(src)) return MOVE_REFUSED;
    const ObjectLayerState* dst = place(to, to_slot, terminal);
    if (NULL == dst) {
        /* A new bag stack needs a free slot, unless a whole bag stack only moves. */
        bool new_stack = BOX_BAG == to && BAG_WALLET_SLOT != to_slot && !(BOX_BAG == from && whole);
        bool room = local_player_inventory_used() < local_player_open_slots(CYBERIA_INVENTORY_CAPACITY);
        return new_stack && !room ? MOVE_REFUSED : MOVE_PLACE;
    }
    if (0 == strcmp(dst->item_id, src->item_id)) return MOVE_STACK;
    if (from == to && whole && takes(from, from_slot, dst->item_id, terminal)) return MOVE_SWAP;
    return MOVE_REFUSED;
}

static void take(Box box, int slot, int qty, EntityID terminal) {
    if (BOX_BAG == box && BAG_WALLET_SLOT == slot) {
        g_local_player.coins -= qty;
        wallet()->quantity -= qty;
        return;
    }
    StackList list;
    bool found = list_of(box, terminal, &list);
    assert(found);
    for (int i = 0; i < *list.count; i++) {
        if (slot != list.items[i].slot) continue;
        if (0 < (list.items[i].quantity -= qty)) return;
        memmove(&list.items[i], &list.items[i + 1], (size_t)(*list.count - i - 1) * sizeof list.items[0]);
        (*list.count)--;
        return;
    }
}

/* Add qty to a place: a new stack, or more of its stack. A worn stack keeps
 * its flag on the stack it joins. */
static void put(Box box, int slot, const char* item_id, int qty, bool worn, EntityID terminal) {
    if (BOX_BAG == box && BAG_WALLET_SLOT == slot) {
        g_local_player.coins += qty;
        wallet()->quantity += qty;
        return;
    }
    ObjectLayerState* stack = place(box, slot, terminal);
    if (NULL != stack) {
        stack->quantity += qty;
        stack->active = stack->active || worn;
        return;
    }
    StackList list;
    bool found = list_of(box, terminal, &list);
    assert(found);
    if (!(*list.count < list.cap)) {
        assert(false && "stack list full");
        LOG_ERROR("stack list full");
        abort();
    }
    stack = &list.items[(*list.count)++];
    memset(stack, 0, sizeof *stack);
    copy_str(stack->item_id, sizeof stack->item_id, item_id);
    stack->quantity = qty;
    stack->slot     = slot;
    stack->active   = worn;
}

static void apply(const client_event_payload_t* p) {
    EntityID terminal = p->entity_id;
    MoveKind kind = check(p->from, p->from_slot, p->to, p->to_slot, p->quantity, terminal);
    if (MOVE_REFUSED == kind) return;
    ObjectLayerState* src = place(p->from, p->from_slot, terminal);
    if (MOVE_SWAP == kind) {
        ObjectLayerState* dst = place(p->to, p->to_slot, terminal);
        dst->slot = p->from_slot;
        src->slot = p->to_slot;
        return;
    }
    char item_id[MAX_ITEM_ID_LENGTH];
    copy_str(item_id, sizeof item_id, src->item_id);
    int  qty  = p->quantity < src->quantity ? p->quantity : src->quantity;
    bool worn = BOX_BAG == p->from && BOX_BAG == p->to && src->active && qty == src->quantity;
    take(p->from, p->from_slot, qty, terminal);
    put(p->to, p->to_slot, item_id, qty, worn, terminal);
}

const ObjectLayerState* container_ops_stack(Box box, int slot) {
    return place(box, slot, s_vault.terminal_id);
}

MoveKind container_ops_check(Box from, int from_slot, Box to, int to_slot, int qty) {
    return check(from, from_slot, to, to_slot, qty, s_vault.terminal_id);
}

void container_ops_move(Box from, int from_slot, Box to, int to_slot, int qty) {
    client_event_payload_t p = { .from = from, .from_slot = from_slot, .to = to, .to_slot = to_slot,
                                 .quantity = qty };
    if (BOX_VAULT == from || BOX_VAULT == to) p.entity_id = s_vault.terminal_id;
    if (MOVE_REFUSED == check(from, from_slot, to, to_slot, qty, p.entity_id)) return;
    apply(&p);
    client_event_push(CLIENT_EVENT_ITEM_MOVE, p);
}

void container_ops_apply(const client_event_payload_t* p) {
    assert(p);
    apply(p);
}
