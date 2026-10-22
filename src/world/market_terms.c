#include "market_terms.h"

#include "serial.h"
#include "utils.h"
#include "world/economy.h"

#include <string.h>

void market_terms_read(const cJSON* payload, MarketTerms* out) {
    memset(out, 0, sizeof *out);
    char status[16] = {0}, selection[16] = {0};
    serial_get_string(payload, "code", out->code, sizeof out->code);
    serial_get_string(payload, "scope", out->scope, sizeof out->scope);
    serial_get_string(payload, "scopeKey", out->scope_key, sizeof out->scope_key);
    serial_get_string(payload, "status", status, sizeof status);
    serial_get_string(payload, "selection", selection, sizeof selection);
    out->active    = 0 == strcmp(status, "active");
    out->by_policy = 0 == strcmp(selection, "policy");
    const cJSON* entry = NULL;
    cJSON_ArrayForEach(entry, serial_get_array(payload, "itemIds")) {
        if (MARKET_TERMS_ITEMS_MAX <= out->item_count) break;
        if (cJSON_IsString(entry)) copy_str(out->item_ids[out->item_count++], MAX_ITEM_ID_LENGTH, entry->valuestring);
    }
    cJSON_ArrayForEach(entry, serial_get_array(payload, "listingModes")) {
        if (!cJSON_IsString(entry)) continue;
        out->fixed   = out->fixed || 0 == strcmp("fixed", entry->valuestring);
        out->auction = out->auction || 0 == strcmp("auction", entry->valuestring);
    }
    cJSON_ArrayForEach(entry, serial_get_array(payload, "hours")) {
        if (CYBERIA_MARKET_HOURS_MAX <= out->hour_count) break;
        if (cJSON_IsNumber(entry)) out->hours[out->hour_count++] = entry->valueint;
    }
    out->listing_fee = serial_get_int_default(payload, "listingFee", 0);
    out->sale_fee    = serial_get_int_default(payload, "saleFee", 0);
    out->price_min   = serial_get_int_default(payload, "priceMin", 1);
    out->price_max   = serial_get_int_default(payload, "priceMax", 1);
}

bool market_terms_lists(const MarketTerms* terms, const char* item_id) {
    if (!economy_policy(item_id)->market || 0 == strcmp(item_id, economy_coin_item())) return false;
    if (terms->by_policy) return true;
    for (int i = 0; i < terms->item_count; i++) {
        if (0 == strcmp(terms->item_ids[i], item_id)) return true;
    }
    return false;
}

bool market_terms_takes_listings(const MarketTerms* terms) {
    return terms->active && (terms->fixed || terms->auction) && 0 < terms->hour_count;
}
