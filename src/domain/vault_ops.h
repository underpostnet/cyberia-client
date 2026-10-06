#ifndef CYBERIA_DOMAIN_VAULT_OPS_H
#define CYBERIA_DOMAIN_VAULT_OPS_H

#include <stdbool.h>

/* The pending op list of the open vault: what the player deposited and
 * withdrew since the last item_ops event. The list is applied to the local
 * inventory on push and again after each snapshot, so a snapshot does not
 * revert a drag. A dropped socket loses the list. */

#define VAULT_OPS_MAX 64

/* Append one op and apply it to the local inventory. */
void vault_ops_push(const char* item_id, int qty, bool to_vault);

/* True when the list has no room for one more op. */
bool vault_ops_full(void);

/* Apply every pending op to g_local_player.inventory, in order. */
void vault_ops_apply_to_inventory(void);

/* Push one item_ops event for the vault on entity_id, then clear the list.
 * An empty list pushes nothing. */
void vault_ops_send(const char* entity_id);

/* Drop the list without a send. */
void vault_ops_clear(void);

#endif /* CYBERIA_DOMAIN_VAULT_OPS_H */
