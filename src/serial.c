#include "serial.h"

#include <assert.h>
#include <stdint.h>
#include <string.h>

/* ============================================================================
 * Pack / unpack
 * ============================================================================ */

RawPack serial_pack(cJSON* obj) {
    assert(obj);
    char* text = cJSON_PrintUnformatted(obj);
    cJSON_Delete(obj);
    assert(text);
    return (RawPack){ .data = (uint8_t*)text, .len = strlen(text) };
}

cJSON* serial_unpack(const uint8_t* data, size_t len) {
    assert(data);
    return cJSON_ParseWithLength((const char*)data, len);
}

/* new_message builds the envelope. *payload points at the payload object. */
static cJSON* new_message(const char* type, cJSON** payload) {
    cJSON* root = cJSON_CreateObject();
    assert(root);
    cJSON_AddStringToObject(root, "type", type);
    *payload = cJSON_AddObjectToObject(root, "payload");
    assert(*payload);
    return root;
}

/* ============================================================================
 * Message packers — client to server
 * ============================================================================ */

cJSON* json_pack_auth(const char* ticket) {
    cJSON* p;
    cJSON* root = new_message("auth", &p);
    if (NULL != ticket && '\0' != ticket[0]) cJSON_AddStringToObject(p, "ticket", ticket);
    return root;
}

cJSON* json_pack_events(cJSON* events) {
    assert(events);
    cJSON* p;
    cJSON* root = new_message("events", &p);
    cJSON_AddItemToObject(p, "events", events);
    return root;
}

cJSON* json_pack_player_action(float target_x, float target_y) {
    cJSON* p;
    cJSON* root = new_message("player_action", &p);
    cJSON_AddNumberToObject(p, "x", target_x);
    cJSON_AddNumberToObject(p, "y", target_y);
    return root;
}

cJSON* json_pack_item_active(const char* item_id, int slot, bool active) {
    assert(item_id);
    cJSON* p;
    cJSON* root = new_message("item_active", &p);
    cJSON_AddStringToObject(p, "itemId", item_id);
    cJSON_AddNumberToObject(p, "slot", slot);
    cJSON_AddBoolToObject(p, "active", active);
    return root;
}

cJSON* json_pack_player_stasis(bool stasis) {
    cJSON* p;
    cJSON* root = new_message("player_stasis", &p);
    cJSON_AddBoolToObject(p, "stasis", stasis);
    return root;
}

cJSON* json_pack_chat(int channel, uint32_t to_id, const char* text) {
    static const char* const words[] = { "world", "party", "direct" };
    assert(0 <= channel && channel < (int)(sizeof words / sizeof words[0]));
    cJSON* p;
    cJSON* root = new_message("chat", &p);
    cJSON_AddStringToObject(p, "channel", words[channel]);
    if (0 != to_id) cJSON_AddNumberToObject(p, "toId", to_id);
    cJSON_AddStringToObject(p, "text", text ? text : "");
    return root;
}

cJSON* json_pack_talk_done(uint32_t entity_id, const char* dialog_code) {
    cJSON* p;
    cJSON* root = new_message("talk_done", &p);
    cJSON_AddNumberToObject(p, "entityId", entity_id);
    cJSON_AddStringToObject(p, "dialogCode", dialog_code ? dialog_code : "");
    return root;
}

cJSON* json_pack_quest_abandon(const char* quest_code) {
    cJSON* p;
    cJSON* root = new_message("quest_abandon", &p);
    cJSON_AddStringToObject(p, "questCode", quest_code ? quest_code : "");
    return root;
}

cJSON* json_pack_quest_accept(uint32_t entity_id, const char* quest_code) {
    cJSON* p;
    cJSON* root = new_message("quest_accept", &p);
    cJSON_AddNumberToObject(p, "entityId", entity_id);
    cJSON_AddStringToObject(p, "questCode", quest_code ? quest_code : "");
    return root;
}

cJSON* json_pack_shop_buy(uint32_t entity_id, const char* item_id, int quantity) {
    cJSON* p;
    cJSON* root = new_message("shop_buy", &p);
    cJSON_AddNumberToObject(p, "entityId", entity_id);
    cJSON_AddStringToObject(p, "itemId", item_id ? item_id : "");
    cJSON_AddNumberToObject(p, "quantity", quantity);
    return root;
}

cJSON* json_pack_craft_item(uint32_t entity_id, int recipe_index, double quote) {
    cJSON* p;
    cJSON* root = new_message("craft_item", &p);
    cJSON_AddNumberToObject(p, "entityId", entity_id);
    cJSON_AddNumberToObject(p, "recipeIndex", recipe_index);
    cJSON_AddNumberToObject(p, "quote", quote);
    return root;
}

cJSON* json_pack_craft_cancel(void) {
    cJSON* p;
    return new_message("craft_cancel", &p);
}


cJSON* json_pack_item_move(const char* from, int from_slot, const char* to, int to_slot, int qty,
                           uint32_t entity_id) {
    assert(from && to);
    cJSON* p;
    cJSON* root = new_message("item_move", &p);
    cJSON_AddStringToObject(p, "from", from);
    cJSON_AddNumberToObject(p, "fromSlot", from_slot);
    cJSON_AddStringToObject(p, "to", to);
    cJSON_AddNumberToObject(p, "toSlot", to_slot);
    cJSON_AddNumberToObject(p, "qty", qty);
    if (0 != entity_id) cJSON_AddNumberToObject(p, "entityId", entity_id);
    return root;
}

cJSON* json_pack_player_target(const char* type, const char* key, uint32_t player_id) {
    assert(type);
    cJSON* p;
    cJSON* root = new_message(type, &p);
    if (NULL != key) cJSON_AddNumberToObject(p, key, player_id);
    return root;
}

cJSON* json_pack_session_leave(const char* travel) {
    assert(travel);
    cJSON* p;
    cJSON* root = new_message("session_leave", &p);
    if ('\0' != travel[0]) cJSON_AddStringToObject(p, "travel", travel);
    return root;
}

cJSON* json_pack_party_reply(uint32_t from_id, bool accept) {
    cJSON* p;
    cJSON* root = new_message("party_reply", &p);
    cJSON_AddNumberToObject(p, "fromId", from_id);
    cJSON_AddBoolToObject(p, "accept", accept);
    return root;
}

cJSON* json_pack_friendship(const char* type, const char* friendship_id, bool accept) {
    assert(type && friendship_id);
    cJSON* p;
    cJSON* root = new_message(type, &p);
    cJSON_AddStringToObject(p, "friendshipId", friendship_id);
    cJSON_AddBoolToObject(p, "accept", accept);
    return root;
}

cJSON* json_pack_item_drop(const char* box, int slot, int quantity, float x, float y) {
    assert(box);
    cJSON* p;
    cJSON* root = new_message("item_drop", &p);
    cJSON_AddStringToObject(p, "box", box);
    cJSON_AddNumberToObject(p, "slot", slot);
    cJSON_AddNumberToObject(p, "quantity", quantity);
    cJSON_AddNumberToObject(p, "x", x);
    cJSON_AddNumberToObject(p, "y", y);
    return root;
}

cJSON* json_pack_item_quote(uint32_t entity_id, bool burn, const char* box, int slot) {
    assert(box);
    cJSON* p;
    cJSON* root = new_message("item_quote", &p);
    cJSON_AddNumberToObject(p, "entityId", entity_id);
    cJSON_AddStringToObject(p, "kind", burn ? "burn" : "sell");
    cJSON_AddStringToObject(p, "box", box);
    cJSON_AddNumberToObject(p, "slot", slot);
    return root;
}

cJSON* json_pack_market_open(uint32_t entity_id, const char* item_id, const char* mode, const char* after) {
    assert(item_id && mode && after);
    cJSON* p;
    cJSON* root = new_message("market_open", &p);
    cJSON_AddNumberToObject(p, "entityId", entity_id);
    cJSON_AddStringToObject(p, "itemId", item_id);
    cJSON_AddStringToObject(p, "mode", mode);
    cJSON_AddStringToObject(p, "after", after);
    return root;
}

cJSON* json_pack_market_list(uint32_t entity_id, const char* box, int slot, int quantity, const char* mode, int price,
                             int hours) {
    assert(box && mode);
    cJSON* p;
    cJSON* root = new_message("market_list", &p);
    cJSON_AddNumberToObject(p, "entityId", entity_id);
    cJSON_AddStringToObject(p, "box", box);
    cJSON_AddNumberToObject(p, "slot", slot);
    cJSON_AddNumberToObject(p, "quantity", quantity);
    cJSON_AddStringToObject(p, "mode", mode);
    cJSON_AddNumberToObject(p, "price", price);
    cJSON_AddNumberToObject(p, "hours", hours);
    return root;
}

cJSON* json_pack_market_listing(const char* type, uint32_t entity_id, const char* listing_id, int price) {
    assert(type && listing_id);
    cJSON* p;
    cJSON* root = new_message(type, &p);
    cJSON_AddNumberToObject(p, "entityId", entity_id);
    cJSON_AddStringToObject(p, "listingId", listing_id);
    if (0 < price) cJSON_AddNumberToObject(p, "price", price);
    return root;
}

cJSON* json_pack_quote_confirm(const char* type, const char* quote_id, int quantity) {
    assert(type && quote_id);
    cJSON* p;
    cJSON* root = new_message(type, &p);
    cJSON_AddStringToObject(p, "quoteId", quote_id);
    cJSON_AddNumberToObject(p, "quantity", quantity);
    return root;
}

cJSON* json_pack_trade_offer(const int* slots, const int* qtys, int count, int coins) {
    cJSON* p;
    cJSON* root = new_message("trade_offer", &p);
    cJSON* items = cJSON_AddArrayToObject(p, "items");
    for (int i = 0; i < count; i++) {
        cJSON* line = cJSON_CreateObject();
        cJSON_AddNumberToObject(line, "slot", slots[i]);
        cJSON_AddNumberToObject(line, "quantity", qtys[i]);
        cJSON_AddItemToArray(items, line);
    }
    cJSON_AddNumberToObject(p, "coins", coins);
    return root;
}

cJSON* json_pack_trade_confirm(uint32_t revision) {
    cJSON* p;
    cJSON* root = new_message("trade_confirm", &p);
    cJSON_AddNumberToObject(p, "revision", revision);
    return root;
}

cJSON* json_pack_trade_reply(uint32_t from_id, bool accept) {
    cJSON* p;
    cJSON* root = new_message("trade_reply", &p);
    cJSON_AddNumberToObject(p, "fromId", from_id);
    cJSON_AddBoolToObject(p, "accept", accept);
    return root;
}

/* ============================================================================
 * Read helpers
 * ============================================================================ */

int serial_get_string(const cJSON* json, const char* key, char* out, size_t max_len) {
    assert(json && key && out && max_len > 0);

    const cJSON* item = cJSON_GetObjectItemCaseSensitive(json, key);
    if (!item || !cJSON_IsString(item)) return -1;

    const char* value = cJSON_GetStringValue(item);
    if (!value) return -1;

    strncpy(out, value, max_len - 1);
    out[max_len - 1] = '\0';
    return 0;
}

cJSON* serial_get_object(const cJSON* json, const char* key) {
    assert(json && key);
    cJSON* item = cJSON_GetObjectItemCaseSensitive(json, key);
    if (!item || !cJSON_IsObject(item)) return NULL;
    return item;
}

cJSON* serial_get_array(const cJSON* json, const char* key) {
    assert(json && key);
    cJSON* item = cJSON_GetObjectItemCaseSensitive(json, key);
    if (!item || !cJSON_IsArray(item)) return NULL;
    return item;
}

int serial_get_int_default(const cJSON* json, const char* key, int default_val) {
    assert(json && key);
    const cJSON* item = cJSON_GetObjectItemCaseSensitive(json, key);
    if (!item || !cJSON_IsNumber(item)) return default_val;
    return item->valueint;
}

uint32_t serial_get_u32_default(const cJSON* json, const char* key, uint32_t default_val) {
    assert(json && key);
    const cJSON* item = cJSON_GetObjectItemCaseSensitive(json, key);
    if (!item || !cJSON_IsNumber(item) || item->valuedouble < 0.0) return default_val;
    return (uint32_t)item->valuedouble;
}

float serial_get_float_default(const cJSON* json, const char* key, float default_val) {
    return (float)serial_get_double_default(json, key, (double)default_val);
}

double serial_get_double_default(const cJSON* json, const char* key, double default_val) {
    assert(json && key);
    const cJSON* item = cJSON_GetObjectItemCaseSensitive(json, key);
    if (!item || !cJSON_IsNumber(item)) return default_val;
    return item->valuedouble;
}

bool serial_get_bool_default(const cJSON* json, const char* key, bool default_val) {
    assert(json && key);
    const cJSON* item = cJSON_GetObjectItemCaseSensitive(json, key);
    if (!item || !cJSON_IsBool(item)) return default_val;
    return cJSON_IsTrue(item);
}
