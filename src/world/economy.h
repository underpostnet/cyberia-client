#ifndef CYBERIA_WORLD_ECONOMY_H
#define CYBERIA_WORLD_ECONOMY_H

#include <cJSON.h>
#include <stdbool.h>

#include "data/action_cache.h"
#include "world/object_layer.h"

/* economy — the economy the world runs, as the server sends it: the operation
 * fees, the wallet maximum and the policy of each item. Every amount the UI
 * shows comes from here, so it never shows an amount the server does not
 * charge. The server stays the authority: a request names the amount the
 * player saw, and the server refuses a different one. */

#define ECONOMY_SALVAGE_MAX 8
#define ECONOMY_ITEMS_MAX   128

typedef struct {
    char item_id[MAX_ITEM_ID_LENGTH];
    int  qty;   /* units per burned unit */
} EconomySalvage;

/* What one item may do in the economy. Coin amounts are per unit. */
typedef struct {
    char           item_id[MAX_ITEM_ID_LENGTH];   /* empty for the defaults */
    bool           trade, world_drop, buyback, burn, market;
    int            buyback_payout, burn_coin_fee, craft_surcharge;
    EconomySalvage salvage[ECONOMY_SALVAGE_MAX];
    int            salvage_count;
} ItemPolicy;

/* Replace the economy with the `economy` object of a metadata message. */
void economy_read(const cJSON* economy);

/* The policy of item_id: its row, else the defaults. */
const ItemPolicy* economy_policy(const char* item_id);

/* The item that shows the wallet. */
const char* economy_coin_item(void);

/* The coins one assembly of a recipe charges: the recipe fee, else the craft
 * surcharge of each produced unit, else the economy craft fee. The server rule. */
int economy_craft_fee(const ActionCraftRecipe* recipe);

/* The coins the wallet still takes before the economy maximum. */
double economy_wallet_room(void);

#endif /* CYBERIA_WORLD_ECONOMY_H */
