#ifndef CYBERIA_DOMAIN_VAULT_OPS_H
#define CYBERIA_DOMAIN_VAULT_OPS_H

#include <stdbool.h>

#include "net/client_event.h"
#include "world/world_types.h"

/* The pending op list of the open vault: what the player deposited and
 * withdrew since the last item_ops event. An op applies to the local inventory
 * and to the vault store (g_local_player.vaults) on push and again after each
 * snapshot, so a snapshot does not revert a drag. A dropped socket loses the
 * list. */

#define VAULT_OPS_MAX 64

/* Name the vault of the session: the action code of its terminal. */
void vault_ops_bind(const char* action_code);

/* Append one op and apply it. */
void vault_ops_push(const char* item_id, int qty, bool to_vault);

/* True when the list has no room for one more op. */
bool vault_ops_full(void);

/* Apply the ops of one sent item_ops event at terminal entity_id, in order. */
void vault_ops_apply(EntityID entity_id, const client_event_item_op_t* ops, int count);

/* Apply every pending op, in order. */
void vault_ops_apply_pending(void);

/* Push one item_ops event for the vault on entity_id, then clear the list.
 * An empty list pushes nothing. */
void vault_ops_send(EntityID entity_id);

/* Drop the list without a send. */
void vault_ops_clear(void);

#endif /* CYBERIA_DOMAIN_VAULT_OPS_H */
