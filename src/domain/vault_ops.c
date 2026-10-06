#include "vault_ops.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "domain/local_player.h"
#include "network/game/client_event.h"
#include "object_layer.h"
#include "world_types.h"
#include "util/log.h"
#include "util/utils.h"

_Static_assert(VAULT_OPS_MAX == CLIENT_EVENT_ITEM_OPS_MAX, "one vault session fits one item_ops event");

static struct {
    client_event_item_op_t ops[VAULT_OPS_MAX];
    int                    count;
} s_vault;

/* Deposit takes from the stack and drops it at 0; withdraw adds to the stack
 * or appends a new one, as the server does. */
static void apply_op(const client_event_item_op_t* op) {
    LocalPlayer* lp = &g_local_player;
    int at = -1;
    for (int i = 0; i < lp->inventory_count; i++) {
        if (0 == strcmp(lp->inventory[i].item_id, op->item_id)) { at = i; break; }
    }
    if (op->to_vault) {
        if (0 > at) return;
        lp->inventory[at].quantity -= op->qty;
        if (0 < lp->inventory[at].quantity) return;
        memmove(&lp->inventory[at], &lp->inventory[at + 1],
                (size_t)(lp->inventory_count - at - 1) * sizeof lp->inventory[0]);
        lp->inventory_count--;
        return;
    }
    if (0 <= at) {
        lp->inventory[at].quantity += op->qty;
        return;
    }
    if (!(lp->inventory_count < MAX_OBJECT_LAYERS)) {
        assert(false && "inventory full");
        LOG_ERROR("inventory full");
        abort();
    }
    ObjectLayerState* ols = &lp->inventory[lp->inventory_count++];
    memset(ols, 0, sizeof *ols);
    copy_str(ols->item_id, sizeof ols->item_id, op->item_id);
    ols->quantity = op->qty;
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
    apply_op(op);
}

bool vault_ops_full(void) { return VAULT_OPS_MAX <= s_vault.count; }

void vault_ops_apply_to_inventory(void) {
    for (int i = 0; i < s_vault.count; i++) apply_op(&s_vault.ops[i]);
}

void vault_ops_send(const char* entity_id) {
    if (0 == s_vault.count) return;
    client_event_payload_t p = { .op_count = s_vault.count };
    copy_str(p.entity_id, sizeof p.entity_id, entity_id);
    memcpy(p.ops, s_vault.ops, (size_t)s_vault.count * sizeof p.ops[0]);
    client_event_push(CLIENT_EVENT_ITEM_OPS, p);
    s_vault.count = 0;
}

void vault_ops_clear(void) { s_vault.count = 0; }
