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

cJSON* json_pack_handshake(const char* client_name, const char* version) {
    cJSON* p;
    cJSON* root = new_message("handshake", &p);
    cJSON_AddStringToObject(p, "name", client_name ? client_name : "cyberia-mmo");
    cJSON_AddStringToObject(p, "version", version ? version : "1.0.0");
    return root;
}

cJSON* json_pack_player_action(float target_x, float target_y,
                               uint32_t client_tick, uint32_t sequence) {
    cJSON* p;
    cJSON* root = new_message("player_action", &p);
    cJSON_AddNumberToObject(p, "x", target_x);
    cJSON_AddNumberToObject(p, "y", target_y);
    cJSON_AddNumberToObject(p, "tick", client_tick);
    cJSON_AddNumberToObject(p, "seq", sequence);
    return root;
}

cJSON* json_pack_item_active(const char* item_id, bool active) {
    assert(item_id);
    cJSON* p;
    cJSON* root = new_message("item_active", &p);
    cJSON_AddStringToObject(p, "itemId", item_id);
    cJSON_AddBoolToObject(p, "active", active);
    return root;
}

static cJSON* pack_freeze(const char* type, const char* reason) {
    cJSON* p;
    cJSON* root = new_message(type, &p);
    cJSON_AddStringToObject(p, "reason", reason ? reason : "");
    return root;
}

cJSON* json_pack_freeze_start(const char* reason) { return pack_freeze("freeze_start", reason); }
cJSON* json_pack_freeze_end(const char* reason)   { return pack_freeze("freeze_end", reason); }

cJSON* json_pack_chat(const char* to_id, const char* text) {
    cJSON* p;
    cJSON* root = new_message("chat", &p);
    cJSON_AddStringToObject(p, "toId", to_id ? to_id : "");
    cJSON_AddStringToObject(p, "text", text ? text : "");
    return root;
}

cJSON* json_pack_dialog_start(const char* entity_id, const char* item_id) {
    cJSON* p;
    cJSON* root = new_message("dialog_start", &p);
    cJSON_AddStringToObject(p, "entityId", entity_id ? entity_id : "");
    cJSON_AddStringToObject(p, "itemId", item_id ? item_id : "");
    return root;
}

cJSON* json_pack_dialog_complete(const char* entity_id, const char* item_id,
                                 const char* dialog_code) {
    cJSON* p;
    cJSON* root = new_message("dialog_complete", &p);
    cJSON_AddStringToObject(p, "entityId", entity_id ? entity_id : "");
    cJSON_AddStringToObject(p, "itemId", item_id ? item_id : "");
    cJSON_AddStringToObject(p, "dialogCode", dialog_code ? dialog_code : "");
    return root;
}

cJSON* json_pack_dialog_cancel(const char* entity_id, const char* item_id) {
    cJSON* p;
    cJSON* root = new_message("dialog_cancel", &p);
    cJSON_AddStringToObject(p, "entityId", entity_id ? entity_id : "");
    cJSON_AddStringToObject(p, "itemId", item_id ? item_id : "");
    return root;
}

cJSON* json_pack_quest_abandon(const char* quest_code) {
    cJSON* p;
    cJSON* root = new_message("quest_abandon", &p);
    cJSON_AddStringToObject(p, "questCode", quest_code ? quest_code : "");
    return root;
}

cJSON* json_pack_quest_accept(const char* entity_id, const char* quest_code) {
    cJSON* p;
    cJSON* root = new_message("quest_accept", &p);
    cJSON_AddStringToObject(p, "entityId", entity_id ? entity_id : "");
    cJSON_AddStringToObject(p, "questCode", quest_code ? quest_code : "");
    return root;
}

cJSON* json_pack_shop_buy(const char* entity_id, const char* item_id, int quantity) {
    cJSON* p;
    cJSON* root = new_message("shop_buy", &p);
    cJSON_AddStringToObject(p, "entityId", entity_id ? entity_id : "");
    cJSON_AddStringToObject(p, "itemId", item_id ? item_id : "");
    cJSON_AddNumberToObject(p, "quantity", quantity);
    return root;
}

cJSON* json_pack_craft_item(const char* entity_id, int recipe_index) {
    cJSON* p;
    cJSON* root = new_message("craft_item", &p);
    cJSON_AddStringToObject(p, "entityId", entity_id ? entity_id : "");
    cJSON_AddNumberToObject(p, "recipeIndex", recipe_index);
    return root;
}

cJSON* json_pack_craft_cancel(void) {
    cJSON* p;
    return new_message("craft_cancel", &p);
}

static cJSON* pack_storage_slots(const char* type, const char* entity_id,
                                 int from_index, int to_index) {
    cJSON* p;
    cJSON* root = new_message(type, &p);
    cJSON_AddStringToObject(p, "entityId", entity_id ? entity_id : "");
    cJSON_AddNumberToObject(p, "fromIndex", from_index);
    cJSON_AddNumberToObject(p, "toIndex", to_index);
    return root;
}

cJSON* json_pack_storage_open(const char* entity_id) {
    cJSON* p;
    cJSON* root = new_message("storage_open", &p);
    cJSON_AddStringToObject(p, "entityId", entity_id ? entity_id : "");
    return root;
}

cJSON* json_pack_storage_move(const char* entity_id, int from_index, int to_index,
                              int quantity) {
    cJSON* root = pack_storage_slots("storage_move", entity_id, from_index, to_index);
    cJSON_AddNumberToObject(cJSON_GetObjectItemCaseSensitive(root, "payload"),
                            "quantity", quantity);
    return root;
}

cJSON* json_pack_storage_swap(const char* entity_id, int from_index, int to_index) {
    return pack_storage_slots("storage_swap", entity_id, from_index, to_index);
}

cJSON* json_pack_storage_transfer(const char* entity_id, const char* item_id, int quantity,
                                  bool deposit, int from_index, int to_index) {
    cJSON* root = pack_storage_slots("storage_transfer", entity_id, from_index, to_index);
    cJSON* p = cJSON_GetObjectItemCaseSensitive(root, "payload");
    cJSON_AddStringToObject(p, "itemId", item_id ? item_id : "");
    cJSON_AddNumberToObject(p, "quantity", quantity);
    cJSON_AddBoolToObject(p, "deposit", deposit);
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
    assert(json && key);
    const cJSON* item = cJSON_GetObjectItemCaseSensitive(json, key);
    if (!item || !cJSON_IsNumber(item)) return default_val;
    return (float)item->valuedouble;
}

bool serial_get_bool_default(const cJSON* json, const char* key, bool default_val) {
    assert(json && key);
    const cJSON* item = cJSON_GetObjectItemCaseSensitive(json, key);
    if (!item || !cJSON_IsBool(item)) return default_val;
    return cJSON_IsTrue(item);
}
