#include "serial.h"
#include "world_types.h"
#include <string.h>
#include <assert.h>
#include <stdint.h>
#include <raylib.h>

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
 * Basic Type Serialization/Deserialization
 * ============================================================================ */

int serial_deserialize_color(const cJSON* json, Color* out) {
    assert(json && out);

    out->r = serial_get_int_default(json, "r", 255);
    out->g = serial_get_int_default(json, "g", 255);
    out->b = serial_get_int_default(json, "b", 255);
    out->a = serial_get_int_default(json, "a", 255);

    return 0;
}

int serial_deserialize_point(const cJSON* json, Vector2* out) {
    assert(json && out);

    if (serial_get_float(json, "X", &out->x) != 0) return -1;
    if (serial_get_float(json, "Y", &out->y) != 0) return -1;

    return 0;
}

int serial_deserialize_dimensions(const cJSON* json, Vector2* out) {
    assert(json && out);

    if (serial_get_float(json, "Width", &out->x) != 0) return -1;
    if (serial_get_float(json, "Height", &out->y) != 0) return -1;

    return 0;
}

Direction serial_deserialize_direction(const cJSON* json) {
    assert(json);

    if (cJSON_IsNumber(json)) {
        int val = json->valueint;
        if (val >= 0 && val <= 8) {
            return (Direction)val;
        }
    } else if (cJSON_IsString(json)) {
        const char* str = cJSON_GetStringValue(json);
        if (str) {
            if (strcmp(str, "up") == 0) return DIRECTION_UP;
            if (strcmp(str, "down") == 0) return DIRECTION_DOWN;
            if (strcmp(str, "left") == 0) return DIRECTION_LEFT;
            if (strcmp(str, "right") == 0) return DIRECTION_RIGHT;
            if (strcmp(str, "up_left") == 0) return DIRECTION_UP_LEFT;
            if (strcmp(str, "up_right") == 0) return DIRECTION_UP_RIGHT;
            if (strcmp(str, "down_left") == 0) return DIRECTION_DOWN_LEFT;
            if (strcmp(str, "down_right") == 0) return DIRECTION_DOWN_RIGHT;
        }
    }

    return DIRECTION_NONE;
}

ObjectLayerMode serial_deserialize_mode(const cJSON* json) {
    assert(json);

    if (cJSON_IsNumber(json)) {
        int val = json->valueint;
        if (val >= 0 && val <= 2) {
            return (ObjectLayerMode)val;
        }
    } else if (cJSON_IsString(json)) {
        const char* str = cJSON_GetStringValue(json);
        if (str) {
            if (strcmp(str, "idle") == 0) return MODE_IDLE;
            if (strcmp(str, "walking") == 0) return MODE_WALKING;
            if (strcmp(str, "teleporting") == 0) return MODE_TELEPORTING;
        }
    }

    return MODE_IDLE;
}

/* ============================================================================
 * ObjectLayerState Serialization/Deserialization
 * ============================================================================ */

int serial_deserialize_object_layer_state(const cJSON* json, ObjectLayerState* out) {
    assert(json && out);

    // Initialize with defaults
    memset(out, 0, sizeof(ObjectLayerState));
    out->quantity = 1;

    serial_get_string_default(json, "itemId", out->item_id, sizeof(out->item_id), "");
    out->active = serial_get_bool_default(json, "active", false);
    out->quantity = serial_get_int_default(json, "quantity", 1);

    return 0;
}

int serial_deserialize_object_layer_array(const cJSON* json, ObjectLayerState* out, int max_count) {
    if (!json || !out || max_count <= 0) return -1;
    if (!cJSON_IsArray(json)) return -1;

    int count = 0;
    const cJSON* item = NULL;

    cJSON_ArrayForEach(item, json) {
        if (count >= max_count) break;

        if (serial_deserialize_object_layer_state(item, &out[count]) == 0) {
            count++;
        }
    }

    return count;
}

/* ============================================================================
 * Path Serialization/Deserialization
 * ============================================================================ */

int serial_deserialize_path(const cJSON* json, Vector2* out, int max_points) {
    if (!json || !out || max_points <= 0) return -1;
    if (!cJSON_IsArray(json)) return -1;

    int count = 0;
    const cJSON* item = NULL;

    cJSON_ArrayForEach(item, json) {
        if (count >= max_points) break;

        // Path points can be either {X, Y} or simple coordinate objects
        if (serial_deserialize_point(item, &out[count]) == 0) {
            count++;
        }
    }

    return count;
}

/* ============================================================================
 * EntityState Serialization/Deserialization
 * ============================================================================ */

int serial_deserialize_entity_state(const cJSON* json, EntityState* out) {
    assert(json && out);

    // Initialize
    memset(out, 0, sizeof(EntityState));

    // Required fields
    if (serial_get_string(json, "id", out->id, sizeof(out->id)) != 0) {
        return -1;
    }

    // Position
    cJSON* pos_obj = serial_get_object(json, "Pos");
    if (pos_obj) {
        serial_deserialize_point(pos_obj, &out->pos_server);
        out->pos_prev = out->pos_server;
        out->interp_pos = out->pos_server;
    }

    // Dimensions
    cJSON* dims_obj = serial_get_object(json, "Dims");
    if (dims_obj) {
        serial_deserialize_dimensions(dims_obj, &out->dims);
    }

    // Direction and mode
    cJSON* dir_obj = cJSON_GetObjectItemCaseSensitive(json, "direction");
    if (dir_obj) {
        out->direction = serial_deserialize_direction(dir_obj);
    }

    cJSON* mode_obj = cJSON_GetObjectItemCaseSensitive(json, "mode");
    if (mode_obj) {
        out->mode = serial_deserialize_mode(mode_obj);
    }

    // Life and max life
    out->life = serial_get_float_default(json, "life", 100.0f);
    out->max_life = serial_get_float_default(json, "maxLife", 100.0f);

    // Respawn
    cJSON* respawn_obj = cJSON_GetObjectItemCaseSensitive(json, "respawnIn");
    if (respawn_obj && cJSON_IsNumber(respawn_obj)) {
        out->respawn_in = (float)respawn_obj->valuedouble;
    } else {
        out->respawn_in = -1.0f; // No respawn
    }

    // Object layers
    cJSON* layers_obj = serial_get_array(json, "objectLayers");
    if (layers_obj) {
        out->object_layer_count = serial_deserialize_object_layer_array(
            layers_obj, out->object_layers, MAX_OBJECT_LAYERS
        );
        if (out->object_layer_count < 0) {
            out->object_layer_count = 0;
        }
    }

    out->last_update = GetTime();

    return 0;
}

/* ============================================================================
 * PlayerState Serialization/Deserialization
 * ============================================================================ */

int serial_deserialize_player_state(const cJSON* json, PlayerState* out) {
    assert(json && out);

    // Deserialize base entity
    if (serial_deserialize_entity_state(json, &out->base) != 0) {
        return -1;
    }

    serial_get_string_default(json, "MapCode", out->map_code, sizeof(out->map_code), "");

    // Target position
    cJSON* target_obj = serial_get_object(json, "targetPos");
    if (target_obj) {
        serial_deserialize_point(target_obj, &out->target_pos);
    }

    // Path
    cJSON* path_obj = serial_get_array(json, "path");
    if (path_obj) {
        out->path_count = serial_deserialize_path(path_obj, out->path, MAX_PATH_POINTS);
        if (out->path_count < 0) {
            out->path_count = 0;
        }
    } else {
        out->path_count = 0;
    }

    return 0;
}

/* ============================================================================
 * BotState Serialization/Deserialization
 * ============================================================================ */

int serial_deserialize_bot_state(const cJSON* json, BotState* out) {
    assert(json && out);

    // Deserialize base entity
    if (serial_deserialize_entity_state(json, &out->base) != 0) {
        return -1;
    }

    // Behavior
    serial_get_string_default(json, "behavior", out->behavior, sizeof(out->behavior), "");

    return 0;
}

/* ============================================================================
 * WorldObject Serialization/Deserialization
 * ============================================================================ */

int serial_deserialize_world_object(const cJSON* json, WorldObject* out) {
    assert(json && out);

    // Initialize
    memset(out, 0, sizeof(WorldObject));

    // Required fields
    if (serial_get_string(json, "id", out->id, sizeof(out->id)) != 0) {
        return -1;
    }

    serial_get_string_default(json, "Type", out->type, sizeof(out->type), "");

    // Position
    cJSON* pos_obj = serial_get_object(json, "Pos");
    if (!pos_obj || serial_deserialize_point(pos_obj, &out->pos) != 0) {
        return -1;
    }

    // Dimensions
    cJSON* dims_obj = serial_get_object(json, "Dims");
    if (!dims_obj || serial_deserialize_dimensions(dims_obj, &out->dims) != 0) {
        return -1;
    }

    // Portal label (optional)
    serial_get_string_default(json, "PortalLabel", out->portal_label, sizeof(out->portal_label), "");

    // Object layers
    cJSON* layers_obj = serial_get_array(json, "objectLayers");
    if (layers_obj) {
        out->object_layer_count = serial_deserialize_object_layer_array(
            layers_obj, out->object_layers, MAX_OBJECT_LAYERS
        );
        if (out->object_layer_count < 0) {
            out->object_layer_count = 0;
        }
    }

    return 0;
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

void uplink_get_items_ids(BinWriter* w, const char* item_id) {
    assert(item_id);
    bw_init(w, UPLINK_GET_ITEMS_IDS);
    bw_str(w, item_id);
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
