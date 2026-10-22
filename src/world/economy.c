#include "economy.h"

#include "serial.h"
#include "sim/local_player.h"
#include "utils.h"

#include <string.h>

static struct {
    char       coin_item[MAX_ITEM_ID_LENGTH];
    double     max_balance;
    int        craft_fee;
    ItemPolicy defaults;
    ItemPolicy items[ECONOMY_ITEMS_MAX];
    int        item_count;
} s_economy = { .max_balance = 4294967295.0 };

static void read_policy(const cJSON* json, const char* item_id, ItemPolicy* out) {
    memset(out, 0, sizeof *out);
    copy_str(out->item_id, sizeof out->item_id, item_id);
    out->trade           = serial_get_bool_default(json, "trade", false);
    out->world_drop      = serial_get_bool_default(json, "worldDrop", false);
    out->buyback         = serial_get_bool_default(json, "buyback", false);
    out->burn            = serial_get_bool_default(json, "burn", false);
    out->market          = serial_get_bool_default(json, "market", false);
    out->buyback_payout  = serial_get_int_default(json, "buybackPayout", 0);
    out->burn_coin_fee   = serial_get_int_default(json, "burnCoinFee", 0);
    out->craft_surcharge = serial_get_int_default(json, "craftSurcharge", 0);
    const cJSON* line = NULL;
    cJSON_ArrayForEach(line, serial_get_array(json, "salvage")) {
        if (ECONOMY_SALVAGE_MAX <= out->salvage_count) break;
        EconomySalvage* s = &out->salvage[out->salvage_count++];
        serial_get_string(line, "itemId", s->item_id, sizeof s->item_id);
        s->qty = serial_get_int_default(line, "qty", 0);
    }
}

void economy_read(const cJSON* economy) {
    if (!cJSON_IsObject(economy)) return;
    serial_get_string(economy, "coinItemId", s_economy.coin_item, sizeof s_economy.coin_item);
    s_economy.max_balance = serial_get_double_default(economy, "maxBalance", 4294967295.0);
    const cJSON* fees = serial_get_object(economy, "fees");
    s_economy.craft_fee  = serial_get_int_default(fees, "craft", 0);
    read_policy(serial_get_object(economy, "itemDefaults"), "", &s_economy.defaults);
    s_economy.item_count = 0;
    const cJSON* row = NULL;
    cJSON_ArrayForEach(row, serial_get_object(economy, "items")) {
        if (ECONOMY_ITEMS_MAX <= s_economy.item_count) break;
        read_policy(row, row->string, &s_economy.items[s_economy.item_count++]);
    }
}

const ItemPolicy* economy_policy(const char* item_id) {
    for (int i = 0; NULL != item_id && i < s_economy.item_count; i++) {
        if (0 == strcmp(s_economy.items[i].item_id, item_id)) return &s_economy.items[i];
    }
    return &s_economy.defaults;
}

const char* economy_coin_item(void) { return s_economy.coin_item; }

int economy_craft_fee(const ActionCraftRecipe* recipe) {
    if (recipe->has_fee) return recipe->fee;
    int surcharge = 0;
    for (int i = 0; i < recipe->output_count; i++) {
        surcharge += economy_policy(recipe->outputs[i].item_id)->craft_surcharge * recipe->outputs[i].qty;
    }
    return 0 < surcharge ? surcharge : s_economy.craft_fee;
}

double economy_wallet_room(void) { return s_economy.max_balance - (double)local_player_coins(); }
