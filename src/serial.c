#include "serial.h"
#include <string.h>
#include <assert.h>
#include <stdint.h>

/* ============================================================================
 * Helper Utilities Implementation
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

int serial_get_int(const cJSON* json, const char* key, int* out) {
    assert(json && key && out);

    const cJSON* item = cJSON_GetObjectItemCaseSensitive(json, key);
    if (!item || !cJSON_IsNumber(item)) return -1;

    *out = item->valueint;
    return 0;
}

int serial_get_float(const cJSON* json, const char* key, float* out) {
    assert(json && key && out);

    const cJSON* item = cJSON_GetObjectItemCaseSensitive(json, key);
    if (!item || !cJSON_IsNumber(item)) return -1;

    *out = (float)item->valuedouble;
    return 0;
}

int serial_get_bool(const cJSON* json, const char* key, bool* out) {
    assert(json && key && out);

    const cJSON* item = cJSON_GetObjectItemCaseSensitive(json, key);
    if (!item || !cJSON_IsBool(item)) return -1;

    *out = cJSON_IsTrue(item);
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

int serial_get_string_default(const cJSON* json, const char* key, char* out, size_t max_len, const char* default_val) {
    if (!out || max_len == 0) return -1;

    if (serial_get_string(json, key, out, max_len) == 0) {
        return 0;
    }

    if (default_val) {
        strncpy(out, default_val, max_len - 1);
        out[max_len - 1] = '\0';
    } else {
        out[0] = '\0';
    }
    return 0;
}

int serial_get_int_default(const cJSON* json, const char* key, int default_val) {
    int value;
    if (serial_get_int(json, key, &value) == 0) {
        return value;
    }
    return default_val;
}

float serial_get_float_default(const cJSON* json, const char* key, float default_val) {
    float value;
    if (serial_get_float(json, key, &value) == 0) {
        return value;
    }
    return default_val;
}

bool serial_get_bool_default(const cJSON* json, const char* key, bool default_val) {
    bool value;
    if (serial_get_bool(json, key, &value) == 0) {
        return value;
    }
    return default_val;
}

/* ============================================================================
 * Uplink Binary Writer
 * ============================================================================ */

void bw_init(BinWriter* w, uint8_t msg_type) {
    w->pos = 0;
    w->buf[w->pos++] = msg_type;
}

void bw_u8(BinWriter* w, uint8_t v) {
    if (w->pos >= sizeof(w->buf)) return;
    w->buf[w->pos++] = v;
}

void bw_f32(BinWriter* w, float v) {
    if (w->pos + 4 > sizeof(w->buf)) return;
    uint32_t bits;
    memcpy(&bits, &v, 4);
    w->buf[w->pos++] = (uint8_t)(bits);
    w->buf[w->pos++] = (uint8_t)(bits >> 8);
    w->buf[w->pos++] = (uint8_t)(bits >> 16);
    w->buf[w->pos++] = (uint8_t)(bits >> 24);
}

void bw_u32(BinWriter* w, uint32_t v) {
    if (w->pos + 4 > sizeof(w->buf)) return;
    w->buf[w->pos++] = (uint8_t)(v);
    w->buf[w->pos++] = (uint8_t)(v >> 8);
    w->buf[w->pos++] = (uint8_t)(v >> 16);
    w->buf[w->pos++] = (uint8_t)(v >> 24);
}

void bw_str(BinWriter* w, const char* s) {
    if (!s) s = "";
    size_t len = strlen(s);
    if (len > 255) len = 255;
    bw_u8(w, (uint8_t)len);
    if (w->pos + len > sizeof(w->buf)) return;
    memcpy(w->buf + w->pos, s, len);
    w->pos += (uint16_t)len;
}

void uplink_handshake(BinWriter* w, const char* client_name, const char* version) {
    bw_init(w, UPLINK_HANDSHAKE);
    bw_str(w, client_name ? client_name : "cyberia-mmo");
    bw_str(w, version     ? version     : "1.0.0");
}

void uplink_player_action(BinWriter* w, float target_x, float target_y,
                          uint32_t client_tick, uint32_t sequence) {
    bw_init(w, UPLINK_PLAYER_ACTION);
    bw_f32(w, target_x);
    bw_f32(w, target_y);
    bw_u32(w, client_tick);
    bw_u32(w, sequence);
}

void uplink_item_activation(BinWriter* w, const char* item_id, bool active) {
    assert(item_id);
    bw_init(w, UPLINK_ITEM_ACTIVATION);
    bw_str(w, item_id);
    bw_u8(w, active ? 1 : 0);
}

void uplink_freeze_start(BinWriter* w, const char* reason) {
    bw_init(w, UPLINK_FREEZE_START);
    bw_str(w, reason ? reason : "");
}

void uplink_freeze_end(BinWriter* w, const char* reason) {
    bw_init(w, UPLINK_FREEZE_END);
    bw_str(w, reason ? reason : "");
}

void uplink_chat(BinWriter* w, const char* to_id, const char* text) {
    bw_init(w, UPLINK_CHAT);
    bw_str(w, to_id ? to_id : "");
    bw_str(w, text  ? text  : "");
}

void uplink_dlg_start(BinWriter* w, const char* entity_id, const char* item_id) {
    bw_init(w, UPLINK_DLG_START);
    bw_str(w, entity_id ? entity_id : "");
    bw_str(w, item_id   ? item_id   : "");
}

void uplink_dlg_complete(BinWriter* w, const char* entity_id, const char* item_id,
                         const char* dialog_code) {
    bw_init(w, UPLINK_DLG_COMPLETE);
    bw_str(w, entity_id   ? entity_id   : "");
    bw_str(w, item_id     ? item_id     : "");
    bw_str(w, dialog_code ? dialog_code : "");
}

void uplink_dlg_cancel(BinWriter* w, const char* entity_id, const char* item_id) {
    bw_init(w, UPLINK_DLG_CANCEL);
    bw_str(w, entity_id ? entity_id : "");
    bw_str(w, item_id   ? item_id   : "");
}
void uplink_quest_abandon(BinWriter* w, const char* quest_code) {
    bw_init(w, UPLINK_QUEST_ABANDON);
    bw_str(w, quest_code ? quest_code : "");
}
void uplink_quest_accept(BinWriter* w, const char* entity_id, const char* quest_code) {
    bw_init(w, UPLINK_QUEST_ACCEPT);
    bw_str(w, entity_id  ? entity_id  : "");
    bw_str(w, quest_code ? quest_code : "");
}
