#include "world/economy.h"
#include "world/market_terms.h"
#include "serial.h"
#include "sim/local_player.h"

#include <assert.h>
#include <cJSON.h>
#include <stdio.h>
#include <string.h>

LocalPlayer g_local_player;

static const char* ECONOMY_JSON =
    "{\"coinItemId\":\"coin\",\"maxBalance\":1000,"
    "\"fees\":{\"portal\":3,\"craft\":5},"
    "\"itemDefaults\":{\"trade\":true,\"worldDrop\":true,\"buyback\":false,\"burn\":false},"
    "\"items\":{"
    "\"hatchet\":{\"trade\":true,\"worldDrop\":false,\"buyback\":true,\"buybackPayout\":8,\"burn\":true,"
    "\"burnCoinFee\":1,\"salvage\":[{\"itemId\":\"scrap-alloy\",\"qty\":1}]},"
    "\"plank\":{\"trade\":true,\"worldDrop\":true,\"craftSurcharge\":2,\"market\":true}}}";

static ActionCraftRecipe recipe(const char* output, int qty) {
    ActionCraftRecipe r = { .output_count = 1, .ingredient_count = 2 };
    strcpy(r.outputs[0].item_id, output);
    r.outputs[0].qty = qty;
    strcpy(r.ingredients[0].item_id, "rustbark-log");
    r.ingredients[0].qty = 2;
    strcpy(r.ingredients[1].item_id, "scrap-alloy");
    r.ingredients[1].qty = 1;
    return r;
}

int main(void) {
    cJSON* economy = cJSON_Parse(ECONOMY_JSON);
    assert(economy);
    economy_read(economy);
    cJSON_Delete(economy);

    /* An item without a row takes the defaults; a row replaces them whole. */
    const ItemPolicy* log = economy_policy("rustbark-log");
    assert(log->trade && log->world_drop && !log->buyback && '\0' == log->item_id[0]);
    const ItemPolicy* hatchet = economy_policy("hatchet");
    assert(hatchet->trade && !hatchet->world_drop && hatchet->buyback && 8 == hatchet->buyback_payout);
    assert(hatchet->burn && 1 == hatchet->burn_coin_fee && 1 == hatchet->salvage_count);
    assert(0 == strcmp("scrap-alloy", hatchet->salvage[0].item_id) && 1 == hatchet->salvage[0].qty);
    assert(0 == strcmp("coin", economy_coin_item()));

    /* A market is denied unless a policy allows it; a trade permission grants none. */
    assert(economy_policy("plank")->market && !hatchet->market && !log->market);

    /* Craft fee: the recipe fee, else the surcharge of each produced unit, else the economy fee.
     * Two ingredients make one output kind; the ingredients never add a fee. */
    ActionCraftRecipe planks = recipe("plank", 3);
    assert(6 == economy_craft_fee(&planks));
    planks.has_fee = true;
    planks.fee = 0;
    assert(0 == economy_craft_fee(&planks));
    ActionCraftRecipe axe = recipe("hatchet", 1);
    assert(5 == economy_craft_fee(&axe));

    /* The wallet takes coins up to the economy maximum. */
    g_local_player.coins = 990;
    assert(10.0 == economy_wallet_room());

    /* A market operation names its terminal, and a listing its stack and terms. */
    cJSON* list = json_pack_market_list(7, "bag", 2, 3, "auction", 40, 12);
    const cJSON* p = cJSON_GetObjectItemCaseSensitive(list, "payload");
    assert(0 == strcmp("market_list", cJSON_GetObjectItemCaseSensitive(list, "type")->valuestring));
    assert(7 == serial_get_int_default(p, "entityId", 0) && 3 == serial_get_int_default(p, "quantity", 0));
    assert(40 == serial_get_int_default(p, "price", 0) && 12 == serial_get_int_default(p, "hours", 0));
    cJSON_Delete(list);
    cJSON* buy = json_pack_market_listing("market_buy", 7, "market_list-ab", 0);
    p = cJSON_GetObjectItemCaseSensitive(buy, "payload");
    assert(NULL == cJSON_GetObjectItemCaseSensitive(p, "price"));
    cJSON_Delete(buy);
    cJSON* open = json_pack_market_open(7, "", "auction", "");
    p = cJSON_GetObjectItemCaseSensitive(open, "payload");
    assert(0 == strcmp("auction", cJSON_GetObjectItemCaseSensitive(p, "mode")->valuestring));
    cJSON_Delete(open);

    /* A policy terminal lists what the economy allows, with no item list of its own. */
    MarketTerms terms;
    cJSON* view = cJSON_Parse(
        "{\"code\":\"wods\",\"scope\":\"universe\",\"scopeKey\":\"cyberia\",\"status\":\"active\","
        "\"selection\":\"policy\",\"itemIds\":[],\"listingModes\":[\"fixed\",\"auction\"],\"hours\":[1,12,48],"
        "\"listingFee\":2,\"saleFee\":1,\"priceMin\":5,\"priceMax\":500}");
    market_terms_read(view, &terms);
    assert(terms.active && terms.by_policy && terms.fixed && terms.auction && 3 == terms.hour_count);
    assert(2 == terms.listing_fee && 1 == terms.sale_fee && 5 == terms.price_min && 500 == terms.price_max);
    assert(0 == strcmp("cyberia", terms.scope_key) && market_terms_takes_listings(&terms));
    assert(market_terms_lists(&terms, "plank") && !market_terms_lists(&terms, "hatchet"));
    assert(!market_terms_lists(&terms, "coin"));
    cJSON_Delete(view);

    /* An item terminal lists only its items, and only those the economy allows. */
    view = cJSON_Parse(
        "{\"code\":\"wods\",\"status\":\"active\",\"selection\":\"items\",\"itemIds\":[\"hatchet\"],"
        "\"listingModes\":[\"fixed\"],\"hours\":[1]}");
    market_terms_read(view, &terms);
    assert(terms.fixed && !terms.auction && !market_terms_lists(&terms, "hatchet") && !market_terms_lists(&terms, "plank"));
    cJSON_Delete(view);

    /* A retired market, or a terminal without listing modes, takes no new listing. */
    view = cJSON_Parse("{\"status\":\"retired\",\"listingModes\":[\"fixed\"],\"hours\":[1]}");
    market_terms_read(view, &terms);
    assert(!market_terms_takes_listings(&terms));
    cJSON_Delete(view);
    view = cJSON_Parse("{\"status\":\"active\",\"listingModes\":[],\"hours\":[1]}");
    market_terms_read(view, &terms);
    assert(!market_terms_takes_listings(&terms));
    cJSON_Delete(view);

    puts("economy_test OK");
    return 0;
}
