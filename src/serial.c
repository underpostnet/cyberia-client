#include "serial.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ============================================================================
 * Helper Utilities Implementation
 * ============================================================================ */

int serial_get_string(const cJSON* json, const char* key, char* out, size_t max_len) {
    if (!json || !key || !out || max_len == 0) return -1;
    
    const cJSON* item = cJSON_GetObjectItemCaseSensitive(json, key);
    if (!item || !cJSON_IsString(item)) return -1;
    
    const char* value = cJSON_GetStringValue(item);
    if (!value) return -1;
    
    strncpy(out, value, max_len - 1);
    out[max_len - 1] = '\0';
    return 0;
}

int serial_get_int(const cJSON* json, const char* key, int* out) {
    if (!json || !key || !out) return -1;
    
    const cJSON* item = cJSON_GetObjectItemCaseSensitive(json, key);
    if (!item || !cJSON_IsNumber(item)) return -1;
    
    *out = item->valueint;
    return 0;
}

int serial_get_float(const cJSON* json, const char* key, float* out) {
    if (!json || !key || !out) return -1;
    
    const cJSON* item = cJSON_GetObjectItemCaseSensitive(json, key);
    if (!item || !cJSON_IsNumber(item)) return -1;
    
    *out = (float)item->valuedouble;
    return 0;
}

int serial_get_double(const cJSON* json, const char* key, double* out) {
    if (!json || !key || !out) return -1;
    
    const cJSON* item = cJSON_GetObjectItemCaseSensitive(json, key);
    if (!item || !cJSON_IsNumber(item)) return -1;
    
    *out = item->valuedouble;
    return 0;
}

int serial_get_bool(const cJSON* json, const char* key, bool* out) {
    if (!json || !key || !out) return -1;
    
    const cJSON* item = cJSON_GetObjectItemCaseSensitive(json, key);
    if (!item || !cJSON_IsBool(item)) return -1;
    
    *out = cJSON_IsTrue(item);
    return 0;
}

cJSON* serial_get_object(const cJSON* json, const char* key) {
    if (!json || !key) return NULL;
    
    cJSON* item = cJSON_GetObjectItemCaseSensitive(json, key);
    if (!item || !cJSON_IsObject(item)) return NULL;
    
    return item;
}

cJSON* serial_get_array(const cJSON* json, const char* key) {
    if (!json || !key) return NULL;
    
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

int serial_deserialize_color_rgba(const cJSON* json, ColorRGBA* out) {
    if (!json || !out) return -1;
    
    out->r = serial_get_int_default(json, "r", 255);
    out->g = serial_get_int_default(json, "g", 255);
    out->b = serial_get_int_default(json, "b", 255);
    out->a = serial_get_int_default(json, "a", 255);
    
    return 0;
}

cJSON* serial_serialize_color_rgba(const ColorRGBA* color) {
    if (!color) return NULL;
    
    cJSON* json = cJSON_CreateObject();
    if (!json) return NULL;
    
    cJSON_AddNumberToObject(json, "r", color->r);
    cJSON_AddNumberToObject(json, "g", color->g);
    cJSON_AddNumberToObject(json, "b", color->b);
    cJSON_AddNumberToObject(json, "a", color->a);
    
    return json;
}

int serial_deserialize_point(const cJSON* json, Vector2* out) {
    if (!json || !out) return -1;
    
    if (serial_get_float(json, "X", &out->x) != 0) return -1;
    if (serial_get_float(json, "Y", &out->y) != 0) return -1;
    
    return 0;
}

cJSON* serial_serialize_point(const Vector2* point) {
    if (!point) return NULL;
    
    cJSON* json = cJSON_CreateObject();
    if (!json) return NULL;
    
    cJSON_AddNumberToObject(json, "X", point->x);
    cJSON_AddNumberToObject(json, "Y", point->y);
    
    return json;
}

int serial_deserialize_dimensions(const cJSON* json, Vector2* out) {
    if (!json || !out) return -1;
    
    if (serial_get_float(json, "Width", &out->x) != 0) return -1;
    if (serial_get_float(json, "Height", &out->y) != 0) return -1;
    
    return 0;
}

cJSON* serial_serialize_dimensions(const Vector2* dims) {
    if (!dims) return NULL;
    
    cJSON* json = cJSON_CreateObject();
    if (!json) return NULL;
    
    cJSON_AddNumberToObject(json, "Width", dims->x);
    cJSON_AddNumberToObject(json, "Height", dims->y);
    
    return json;
}

Direction serial_deserialize_direction(const cJSON* json) {
    if (!json) return DIRECTION_NONE;
    
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

cJSON* serial_serialize_direction(Direction dir) {
    return cJSON_CreateNumber((int)dir);
}

ObjectLayerMode serial_deserialize_mode(const cJSON* json) {
    if (!json) return MODE_IDLE;
    
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

cJSON* serial_serialize_mode(ObjectLayerMode mode) {
    return cJSON_CreateNumber((int)mode);
}

/* ============================================================================
 * ObjectLayerState Serialization/Deserialization
 * ============================================================================ */

int serial_deserialize_object_layer_state(const cJSON* json, ObjectLayerState* out) {
    if (!json || !out) return -1;
    
    // Initialize with defaults
    memset(out, 0, sizeof(ObjectLayerState));
    out->quantity = 1;
    
    serial_get_string_default(json, "itemId", out->item_id, sizeof(out->item_id), "");
    out->active = serial_get_bool_default(json, "active", false);
    out->quantity = serial_get_int_default(json, "quantity", 1);
    
    return 0;
}

cJSON* serial_serialize_object_layer_state(const ObjectLayerState* state) {
    if (!state) return NULL;
    
    cJSON* json = cJSON_CreateObject();
    if (!json) return NULL;
    
    cJSON_AddStringToObject(json, "itemId", state->item_id);
    cJSON_AddBoolToObject(json, "active", state->active);
    cJSON_AddNumberToObject(json, "quantity", state->quantity);
    
    return json;
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

cJSON* serial_serialize_object_layer_array(const ObjectLayerState* states, int count) {
    if (!states || count <= 0) return NULL;
    
    cJSON* array = cJSON_CreateArray();
    if (!array) return NULL;
    
    for (int i = 0; i < count; i++) {
        cJSON* item = serial_serialize_object_layer_state(&states[i]);
        if (item) {
            cJSON_AddItemToArray(array, item);
        }
    }
    
    return array;
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

cJSON* serial_serialize_path(const Vector2* path, int count) {
    if (!path || count <= 0) return NULL;
    
    cJSON* array = cJSON_CreateArray();
    if (!array) return NULL;
    
    for (int i = 0; i < count; i++) {
        cJSON* point = serial_serialize_point(&path[i]);
        if (point) {
            cJSON_AddItemToArray(array, point);
        }
    }
    
    return array;
}

/* ============================================================================
 * EntityState Serialization/Deserialization
 * ============================================================================ */

int serial_deserialize_entity_state(const cJSON* json, EntityState* out) {
    if (!json || !out) return -1;
    
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

cJSON* serial_serialize_entity_state(const EntityState* entity) {
    if (!entity) return NULL;
    
    cJSON* json = cJSON_CreateObject();
    if (!json) return NULL;
    
    cJSON_AddStringToObject(json, "id", entity->id);
    
    cJSON* pos = serial_serialize_point(&entity->pos_server);
    if (pos) cJSON_AddItemToObject(json, "Pos", pos);
    
    cJSON* dims = serial_serialize_dimensions(&entity->dims);
    if (dims) cJSON_AddItemToObject(json, "Dims", dims);
    
    cJSON_AddItemToObject(json, "direction", serial_serialize_direction(entity->direction));
    cJSON_AddItemToObject(json, "mode", serial_serialize_mode(entity->mode));
    
    cJSON_AddNumberToObject(json, "life", entity->life);
    cJSON_AddNumberToObject(json, "maxLife", entity->max_life);
    
    if (entity->respawn_in >= 0) {
        cJSON_AddNumberToObject(json, "respawnIn", entity->respawn_in);
    } else {
        cJSON_AddNullToObject(json, "respawnIn");
    }
    
    if (entity->object_layer_count > 0) {
        cJSON* layers = serial_serialize_object_layer_array(
            entity->object_layers, entity->object_layer_count
        );
        if (layers) cJSON_AddItemToObject(json, "objectLayers", layers);
    }
    
    return json;
}

/* ============================================================================
 * PlayerState Serialization/Deserialization
 * ============================================================================ */

int serial_deserialize_player_state(const cJSON* json, PlayerState* out) {
    if (!json || !out) return -1;
    
    // Deserialize base entity
    if (serial_deserialize_entity_state(json, &out->base) != 0) {
        return -1;
    }
    
    // Map ID
    out->map_id = serial_get_int_default(json, "MapID", 0);
    
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

cJSON* serial_serialize_player_state(const PlayerState* player) {
    if (!player) return NULL;
    
    cJSON* json = serial_serialize_entity_state(&player->base);
    if (!json) return NULL;
    
    cJSON_AddNumberToObject(json, "MapID", player->map_id);
    
    cJSON* target = serial_serialize_point(&player->target_pos);
    if (target) cJSON_AddItemToObject(json, "targetPos", target);
    
    if (player->path_count > 0) {
        cJSON* path = serial_serialize_path(player->path, player->path_count);
        if (path) cJSON_AddItemToObject(json, "path", path);
    }
    
    return json;
}

/* ============================================================================
 * BotState Serialization/Deserialization
 * ============================================================================ */

int serial_deserialize_bot_state(const cJSON* json, BotState* out) {
    if (!json || !out) return -1;
    
    // Deserialize base entity
    if (serial_deserialize_entity_state(json, &out->base) != 0) {
        return -1;
    }
    
    // Behavior
    serial_get_string_default(json, "behavior", out->behavior, sizeof(out->behavior), "");
    
    return 0;
}

cJSON* serial_serialize_bot_state(const BotState* bot) {
    if (!bot) return NULL;
    
    cJSON* json = serial_serialize_entity_state(&bot->base);
    if (!json) return NULL;
    
    cJSON_AddStringToObject(json, "behavior", bot->behavior);
    
    return json;
}

/* ============================================================================
 * WorldObject Serialization/Deserialization
 * ============================================================================ */

int serial_deserialize_world_object(const cJSON* json, WorldObject* out) {
    if (!json || !out) return -1;
    
    // Initialize
    memset(out, 0, sizeof(WorldObject));
    
    // Required fields
    if (serial_get_string(json, "id", out->id, sizeof(out->id)) != 0) {
        return -1;
    }
    
    serial_get_string_default(json, "Type", out->type, sizeof(out->type), "");
    
    // Position
    cJSON* pos_obj = serial_get_object(json, "Pos");
    if (pos_obj) {
        serial_deserialize_point(pos_obj, &out->pos);
    }
    
    // Dimensions
    cJSON* dims_obj = serial_get_object(json, "Dims");
    if (dims_obj) {
        serial_deserialize_dimensions(dims_obj, &out->dims);
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

cJSON* serial_serialize_world_object(const WorldObject* obj) {
    if (!obj) return NULL;
    
    cJSON* json = cJSON_CreateObject();
    if (!json) return NULL;
    
    cJSON_AddStringToObject(json, "id", obj->id);
    cJSON_AddStringToObject(json, "Type", obj->type);
    
    cJSON* pos = serial_serialize_point(&obj->pos);
    if (pos) cJSON_AddItemToObject(json, "Pos", pos);
    
    cJSON* dims = serial_serialize_dimensions(&obj->dims);
    if (dims) cJSON_AddItemToObject(json, "Dims", dims);
    
    if (obj->portal_label[0] != '\0') {
        cJSON_AddStringToObject(json, "PortalLabel", obj->portal_label);
    }
    
    if (obj->object_layer_count > 0) {
        cJSON* layers = serial_serialize_object_layer_array(
            obj->object_layers, obj->object_layer_count
        );
        if (layers) cJSON_AddItemToObject(json, "objectLayers", layers);
    }
    
    return json;
}

/* ============================================================================
 * Message Creation Functions
 * ============================================================================ */

char* serial_create_handshake(const char* client_name, const char* version) {
    cJSON* json = cJSON_CreateObject();
    if (!json) return NULL;
    
    cJSON_AddStringToObject(json, "type", "handshake");
    cJSON_AddStringToObject(json, "client", client_name ? client_name : "cyberia-mmo");
    cJSON_AddStringToObject(json, "version", version ? version : "1.0.0");
    
    char* str = cJSON_PrintUnformatted(json);
    cJSON_Delete(json);
    
    return str;
}

char* serial_create_player_action(float target_x, float target_y) {
    cJSON* json = cJSON_CreateObject();
    if (!json) return NULL;
    
    cJSON_AddStringToObject(json, "type", "player_action");
    
    cJSON* payload = cJSON_CreateObject();
    if (payload) {
        cJSON_AddNumberToObject(payload, "targetX", target_x);
        cJSON_AddNumberToObject(payload, "targetY", target_y);
        cJSON_AddItemToObject(json, "payload", payload);
    }
    
    char* str = cJSON_PrintUnformatted(json);
    cJSON_Delete(json);
    
    return str;
}

char* serial_create_ping(void) {
    cJSON* json = cJSON_CreateObject();
    if (!json) return NULL;
    
    cJSON_AddStringToObject(json, "type", "ping");
    
    char* str = cJSON_PrintUnformatted(json);
    cJSON_Delete(json);
    
    return str;
}

char* serial_create_pong(void) {
    cJSON* json = cJSON_CreateObject();
    if (!json) return NULL;
    
    cJSON_AddStringToObject(json, "type", "pong");
    
    char* str = cJSON_PrintUnformatted(json);
    cJSON_Delete(json);
    
    return str;
}

char* serial_create_item_action(const char* item_id, bool activate) {
    if (!item_id) return NULL;
    
    cJSON* json = cJSON_CreateObject();
    if (!json) return NULL;
    
    cJSON_AddStringToObject(json, "type", "item_action");
    
    cJSON* payload = cJSON_CreateObject();
    if (payload) {
        cJSON_AddStringToObject(payload, "itemId", item_id);
        cJSON_AddBoolToObject(payload, "activate", activate);
        cJSON_AddItemToObject(json, "payload", payload);
    }
    
    char* str = cJSON_PrintUnformatted(json);
    cJSON_Delete(json);
    
    return str;
}