#ifndef CYBERIA_WORLD_MARKET_TERMS_H
#define CYBERIA_WORLD_MARKET_TERMS_H

#include <cJSON.h>
#include <stdbool.h>

#include "content/stat_contract_generated.h"
#include "world/object_layer.h"

/* market_terms — what a market terminal shows of its market, as the server
 * sends it with each view: the shared terms of the market profile, and what a
 * seller lists at this terminal. The server and the engine stay the
 * authority: a listing outside these terms is refused there. */

#define MARKET_TERMS_CODE_MAX  64
#define MARKET_TERMS_ITEMS_MAX 16

typedef struct {
    char code[MARKET_TERMS_CODE_MAX];
    char scope[16];                        /* instance | universe */
    char scope_key[MARKET_TERMS_CODE_MAX]; /* the world or the universe that shares it */
    bool active;                           /* a retired market takes no new listing */
    bool by_policy;                        /* every item the economy allows, else item_ids only */
    char item_ids[MARKET_TERMS_ITEMS_MAX][MAX_ITEM_ID_LENGTH];
    int  item_count;
    bool fixed, auction;                   /* the modes a seller lists in at this terminal */
    int  hours[CYBERIA_MARKET_HOURS_MAX];  /* the durations a listing may run */
    int  hour_count;
    int  listing_fee, sale_fee, price_min, price_max;
} MarketTerms;

/* Read the terms of a market view message. */
void market_terms_read(const cJSON* payload, MarketTerms* out);

/* Whether a seller may list item_id here: the terminal names it (or lists by
 * policy), the economy policy allows a listing, and it is not the coin. */
bool market_terms_lists(const MarketTerms* terms, const char* item_id);

/* Whether this terminal takes new listings at all. */
bool market_terms_takes_listings(const MarketTerms* terms);

#endif /* CYBERIA_WORLD_MARKET_TERMS_H */
