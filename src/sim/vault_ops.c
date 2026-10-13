#include "vault_ops.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "local_player.h"
#include "game_state.h"
#include "net/client_event.h"
#include "world/object_layer.h"
#include "world/world_types.h"
#include "log.h"
#include "utils.h"

_Static_assert(VAULT_OPS_MAX == CLIENT_EVENT_ITEM_OPS_MAX, "one vault session fits one item_ops event");

static struct {
    char                   action_code[MAX_ID_LENGTH];   /* the vault of this session */
    client_event_item_op_t ops[VAULT_OPS_MAX];
    int                    count;
} s_vault;

/* Take qty of item_id from the stacks, dropping the stack at 0, or add it,
 * appending a new stack, as the server does. */
static void move_stack(ObjectLayerState* stacks, int* count, int cap,
                       const char* item_id, int qty, bool take) {
    int at = -1;
    for (int i = 0; i < *count; i++) {
        if (0 == strcmp(stacks[i].item_id, item_id)) { at = i; break; }
    }
    if (take) {
        if (0 > at) return;
        stacks[at].quantity -= qty;
        if (0 < stacks[at].quantity) return;
        memmove(&stacks[at], &stacks[at + 1], (size_t)(*count - at - 1) * sizeof stacks[0]);
        (*count)--;
        return;
    }
    if (0 <= at) {
        stacks[at].quantity += qty;
        return;
    }
    if (!(*count < cap)) {
        assert(false && "stack list full");
        LOG_ERROR("stack list full");
        abort();
    }
    ObjectLayerState* ols = &stacks[(*count)++];
    memset(ols, 0, sizeof *ols);
    copy_str(ols->item_id, sizeof ols->item_id, item_id);
    ols->quantity = qty;
}

/* The vault store entry of action_code, made empty on first use. NULL when
 * the code is empty or the store is full. */
static Vault* vault_for(const char* action_code) {
    if (NULL == action_code || '\0' == action_code[0]) return NULL;
    Vault* vault = local_player_vault(action_code);
    if (NULL != vault || VAULTS_MAX <= g_local_player.vault_count) return vault;
    vault = &g_local_player.vaults[g_local_player.vault_count++];
    memset(vault, 0, sizeof *vault);
    copy_str(vault->action_code, sizeof vault->action_code, action_code);
    return vault;
}

/* Apply one op to the inventory and to the vault of action_code. */
static void apply_op(const client_event_item_op_t* op, const char* action_code) {
    LocalPlayer* lp = &g_local_player;
    move_stack(lp->inventory, &lp->inventory_count, MAX_OBJECT_LAYERS, op->item_id, op->qty, op->to_vault);
    Vault* vault = vault_for(action_code);
    if (NULL != vault) move_stack(vault->items, &vault->count, VAULT_SLOTS_MAX, op->item_id, op->qty, !op->to_vault);
}

void vault_ops_bind(const char* action_code) {
    copy_str(s_vault.action_code, sizeof s_vault.action_code, action_code);
}

void vault_ops_push(const char* item_id, int qty, bool to_vault) {
    if (!(s_vault.count < VAULT_OPS_MAX)) {
        assert(false && "vault op list full");
        LOG_ERROR("vault op list full");
        abort();
    }
    client_event_item_op_t* op = &s_vault.ops[s_vault.count++];
    copy_str(op->item_id, sizeof op->item_id, item_id);
    op->qty      = qty;
    op->to_vault = to_vault;
    apply_op(op, s_vault.action_code);
}

bool vault_ops_full(void) { return VAULT_OPS_MAX <= s_vault.count; }

void vault_ops_apply(EntityID entity_id, const client_event_item_op_t* ops, int count) {
    assert(ops || 0 == count);
    /* The terminal names the vault; one out of view leaves the vault as sent. */
    int slot = game_state_slot(entity_id);
    const char* code = 0 <= slot && 0 != (g_game_state.mask[slot] & COMP_AGENT)
                     ? g_game_state.agent[slot].action_code : NULL;
    for (int i = 0; i < count; i++) apply_op(&ops[i], code);
}

void vault_ops_apply_pending(void) {
    for (int i = 0; i < s_vault.count; i++) apply_op(&s_vault.ops[i], s_vault.action_code);
}

void vault_ops_send(EntityID entity_id) {
    if (0 == s_vault.count) return;
    client_event_payload_t p = { .entity_id = entity_id, .op_count = s_vault.count };
    memcpy(p.ops, s_vault.ops, (size_t)s_vault.count * sizeof p.ops[0]);
    client_event_push(CLIENT_EVENT_ITEM_OPS, p);
    s_vault.count = 0;
}

void vault_ops_clear(void) { s_vault.count = 0; }
