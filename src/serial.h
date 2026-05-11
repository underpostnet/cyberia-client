#ifndef SERIAL_H
#define SERIAL_H

#include <stdbool.h>
#include <stddef.h>
#include <cJSON.h>
#include <raylib.h>
#include "game_state.h" // TODO: serializer shouldn't depend on game state, but only on structure

/* ============================================================================
 * Core Serialization/Deserialization Functions
 * ============================================================================ */

int serial_deserialize_color(const cJSON* json, Color* out);

/**
 * @brief Deserialize JSON object to Vector2 (Point)
 * @param json cJSON object with X/Y fields
 * @param out Output Vector2 structure
 * @return 0 on success, -1 on failure
 */
int serial_deserialize_point(const cJSON* json, Vector2* out);

int serial_deserialize_dimensions(const cJSON* json, Vector2* out);

Direction serial_deserialize_direction(const cJSON* json);

ObjectLayerMode serial_deserialize_mode(const cJSON* json);

int serial_deserialize_object_layer_state(const cJSON* json, ObjectLayerState* out);

int serial_deserialize_object_layer_array(const cJSON* json, ObjectLayerState* out, int max_count);

int serial_deserialize_entity_state(const cJSON* json, EntityState* out);

/**
 * @brief Deserialize JSON object to PlayerState (player object)
 * @param json cJSON object with player data
 * @param out Output PlayerState structure
 * @return 0 on success, -1 on failure
 */
int serial_deserialize_player_state(const cJSON* json, PlayerState* out);

int serial_deserialize_bot_state(const cJSON* json, BotState* out);

int serial_deserialize_world_object(const cJSON* json, WorldObject* out);

int serial_deserialize_path(const cJSON* json, Vector2* out, int max_points);

/* ============================================================================
 * Helper Utilities
 * ============================================================================ */

/**
 * @brief Safely get string from cJSON object
 * @param json cJSON object
 * @param key Field name
 * @param out Output buffer
 * @param max_len Maximum length of output buffer
 * @return 0 on success, -1 if field missing or not a string
 */
int serial_get_string(const cJSON* json, const char* key, char* out, size_t max_len);

/**
 * @brief Safely get integer from cJSON object
 * @param json cJSON object
 * @param key Field name
 * @param out Output integer pointer
 * @return 0 on success, -1 if field missing or not a number
 */
int serial_get_int(const cJSON* json, const char* key, int* out);

/**
 * @brief Safely get float from cJSON object
 * @param json cJSON object
 * @param key Field name
 * @param out Output float pointer
 * @return 0 on success, -1 if field missing or not a number
 */
int serial_get_float(const cJSON* json, const char* key, float* out);

/*
 * @brief Safely get double from cJSON object
 * @param json cJSON object
 * @param key Field name
 * @param out Output double pointer
 * @return 0 on success, -1 if field missing or not a number
 * /
// int serial_get_double(const cJSON* json, const char* key, double* out);
*/

/**
 * @brief Safely get boolean from cJSON object
 * @param json cJSON object
 * @param key Field name
 * @param out Output bool pointer
 * @return 0 on success, -1 if field missing or not a boolean
 */
int serial_get_bool(const cJSON* json, const char* key, bool* out);

/**
 * @brief Safely get object from cJSON object
 * @param json cJSON object
 * @param key Field name
 * @return cJSON object pointer or NULL if not found
 */
cJSON* serial_get_object(const cJSON* json, const char* key);

/**
 * @brief Safely get array from cJSON object
 * @param json cJSON object
 * @param key Field name
 * @return cJSON array pointer or NULL if not found
 */
cJSON* serial_get_array(const cJSON* json, const char* key);

/**
 * @brief Get optional string with default value
 * @param json cJSON object
 * @param key Field name
 * @param out Output buffer
 * @param max_len Maximum length of output buffer
 * @param default_val Default value if field missing
 * @return 0 on success
 */
int serial_get_string_default(const cJSON* json, const char* key, char* out, size_t max_len, const char* default_val);

/**
 * @brief Get optional integer with default value
 * @param json cJSON object
 * @param key Field name
 * @param default_val Default value if field missing
 * @return Integer value
 */
int serial_get_int_default(const cJSON* json, const char* key, int default_val);

/**
 * @brief Get optional float with default value
 * @param json cJSON object
 * @param key Field name
 * @param default_val Default value if field missing
 * @return Float value
 */
float serial_get_float_default(const cJSON* json, const char* key, float default_val);

/**
 * @brief Get optional boolean with default value
 * @param json cJSON object
 * @param key Field name
 * @param default_val Default value if field missing
 * @return Boolean value
 */
bool serial_get_bool_default(const cJSON* json, const char* key, bool default_val);

/* ============================================================================
 * Message Creation (Serialization for Outgoing Messages)
 * ============================================================================ */

void serialize_handshake(cJSON* out, const char* client_name, const char* version);
void serialize_player_action(cJSON* out, float target_x, float target_y);
void serialize_item_activation(cJSON* out, const char* item_id, bool active);
void serialize_freeze(cJSON* out, const char* type, const char* reason, const char* entity_id, const char* item_id);

#endif // SERIAL_H
