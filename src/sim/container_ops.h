#ifndef CYBERIA_SIM_CONTAINER_OPS_H
#define CYBERIA_SIM_CONTAINER_OPS_H

#include <stdbool.h>

#include "net/client_event.h"
#include "world/object_layer.h"
#include "world/world_types.h"

/* Item moves: units of one stack to a slot of the bag or of the open
 * container, a vault at its terminal or the personal storage. Every slot holds
 * one stack, and one item can fill several stacks. A move applies to the local
 * stores (g_local_player.inventory, .vaults, .storage) on push and again after
 * each snapshot until the server consumes it, under the rules of
 * cyberia-server game/content/storage.go. */

/* The bag slot of the coin: the wallet takes no cell. */
#define BAG_WALLET_SLOT (-1)

/* What a move does. */
typedef enum {
    MOVE_REFUSED,  /* nothing */
    MOVE_PLACE,    /* the units go to an empty slot */
    MOVE_STACK,    /* the units join the stack of the same item */
    MOVE_SWAP,     /* two whole stacks of one box trade slots */
} MoveKind;

/* Name the open container: the vault `action_code` of `capacity` slots at
 * terminal `terminal_id`, or the personal storage. */
void container_ops_bind_vault(EntityID terminal_id, const char* action_code, int capacity);
void container_ops_bind_storage(void);

/* The stack in a place, or NULL. The wallet reads as the coin stack. */
const ObjectLayerState* container_ops_stack(Box box, int slot);

/* What moving `qty` units of the stack in the first place to the second does. */
MoveKind container_ops_check(Box from, int from_slot, Box to, int to_slot, int qty);

/* Apply one move and push its event. A refused move does nothing. */
void container_ops_move(Box from, int from_slot, Box to, int to_slot, int qty);

/* Apply the move of one sent item_move event again. */
void container_ops_apply(const client_event_payload_t* p);

#endif /* CYBERIA_SIM_CONTAINER_OPS_H */
