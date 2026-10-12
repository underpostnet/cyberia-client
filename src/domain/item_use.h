#ifndef CYBERIA_DOMAIN_ITEM_USE_H
#define CYBERIA_DOMAIN_ITEM_USE_H

#include <stdbool.h>

/* Consumable use by the local player: whether an owned item may be used now,
 * the item_use request, and its cooldown. The server applies the effect and
 * spends the unit; the snapshot and item_use_ack carry the result back. */

typedef enum {
    ITEM_USE_READY,
    ITEM_USE_NOT_CONSUMABLE,
    ITEM_USE_NOT_OWNED,
    ITEM_USE_DEAD,
    ITEM_USE_PENDING,
    ITEM_USE_COOLDOWN,
    ITEM_USE_FULL_LIFE,
} item_use_state_e;

/* True for an item of the consumable type: the only type Use applies to. */
bool item_use_is_consumable(const char* item_id);

/* Whether the local player may use `item_id` at `now`, on the GetTime() clock. */
item_use_state_e item_use_state(const char* item_id, double now);

/* Short text for a state that blocks use; "" for ITEM_USE_READY. */
const char* item_use_state_text(item_use_state_e state);

/* Send item_use when the item may be used. Returns the state it was in. */
item_use_state_e item_use_request(const char* item_id, double now);

/* item_use_ack settles the request. Returns the state a rejection reason
 * names, or ITEM_USE_READY when the server applied the use. */
item_use_state_e item_use_settle(bool ok, const char* reason);

/* Authoritative end of the item-use cooldown, on the GetTime() clock. */
void  item_use_set_ready_at(double time_s);
/* Elapsed part of the item-use cooldown at `now`, 0..1. The server times it by
 * the action cooldown rule. */
float item_use_charge(double now);

void item_use_reset(void);

#endif /* CYBERIA_DOMAIN_ITEM_USE_H */
