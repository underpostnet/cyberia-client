#ifndef SERIAL_H
#define SERIAL_H

#include <stdbool.h>
#include <stddef.h>
#include "../lib/cJSON/cJSON.h"
#include "game_state.h"

/**
 * @file serial.h
 * @brief General-purpose JSON serialization/deserialization framework
 * 
 * This module provides C equivalents to Python's serial.py functionality,
 * enabling bidirectional conversion between C structs and JSON.
 * 
 * Based on: src/serial.py
 * 
 * Key Features:
 * - Type-safe deserialization from JSON to C structs
 * - Serialization from C structs to JSON
 * - Support for nested structures
 * - Array/list handling
 * - Error reporting and validation
 */

/* ============================================================================
 * Core Serialization/Deserialization Functions
 * ============================================================================ */

/**
 * @brief Deserialize JSON string to ColorRGBA struct
 * @param json cJSON object containing color data
 * @param out Output ColorRGBA structure
 * @return 0 on success, -1 on failure
 */
int serial_deserialize_color_rgba(const cJSON* json, ColorRGBA* out);

/**
 * @brief Serialize ColorRGBA struct to cJSON object
 * @param color Input ColorRGBA structure
 * @return cJSON object (caller must free) or NULL on failure
 */
cJSON* serial_serialize_color_rgba(const ColorRGBA* color);

/**
 * @brief Deserialize JSON object to Vector2 (Point)
 * @param json cJSON object with X/Y fields
 * @param out Output Vector2 structure
 * @return 0 on success, -1 on failure
 */
int serial_deserialize_point(const cJSON* json, Vector2* out);

/**
 * @brief Serialize Vector2 (Point) to cJSON object
 * @param point Input Vector2 structure
 * @return cJSON object (caller must free) or NULL on failure
 */
cJSON* serial_serialize_point(const Vector2* point);

/**
 * @brief Deserialize JSON object to Vector2 (Dimensions)
 * @param json cJSON object with Width/Height fields
 * @param out Output Vector2 structure
 * @return 0 on success, -1 on failure
 */
int serial_deserialize_dimensions(const cJSON* json, Vector2* out);

/**
 * @brief Serialize Vector2 (Dimensions) to cJSON object
 * @param dims Input Vector2 structure
 * @return cJSON object (caller must free) or NULL on failure
 */
cJSON* serial_serialize_dimensions(const Vector2* dims);

/**
 * @brief Deserialize JSON string to Direction enum
 * @param json cJSON number or string
 * @return Direction enum value
 */
Direction serial_deserialize_direction(const cJSON* json);

/**
 * @brief Serialize Direction enum to cJSON number
 * @param dir Direction value
 * @return cJSON number (caller must free) or NULL on failure
 */
cJSON* serial_serialize_direction(Direction dir);

/**
 * @brief Deserialize JSON string to ObjectLayerMode enum
 * @param json cJSON number or string
 * @return ObjectLayerMode enum value
 */
ObjectLayerMode serial_deserialize_mode(const cJSON* json);

/**
 * @brief Serialize ObjectLayerMode enum to cJSON number
 * @param mode Mode value
 * @return cJSON number (caller must free) or NULL on failure
 */
cJSON* serial_serialize_mode(ObjectLayerMode mode);

/**
 * @brief Deserialize JSON object to ObjectLayerState
 * @param json cJSON object with itemId, active, quantity
 * @param out Output ObjectLayerState structure
 * @return 0 on success, -1 on failure
 */
int serial_deserialize_object_layer_state(const cJSON* json, ObjectLayerState* out);

/**
 * @brief Serialize ObjectLayerState to cJSON object
 * @param state Input ObjectLayerState structure
 * @return cJSON object (caller must free) or NULL on failure
 */
cJSON* serial_serialize_object_layer_state(const ObjectLayerState* state);

/**
 * @brief Deserialize JSON array to ObjectLayerState array
 * @param json cJSON array of object layer states
 * @param out Output array of ObjectLayerState
 * @param max_count Maximum number of elements to deserialize
 * @return Number of elements deserialized, or -1 on error
 */
int serial_deserialize_object_layer_array(const cJSON* json, ObjectLayerState* out, int max_count);

/**
 * @brief Serialize ObjectLayerState array to cJSON array
 * @param states Input array of ObjectLayerState
 * @param count Number of elements in array
 * @return cJSON array (caller must free) or NULL on failure
 */
cJSON* serial_serialize_object_layer_array(const ObjectLayerState* states, int count);

/**
 * @brief Deserialize JSON object to EntityState (base entity)
 * @param json cJSON object with entity data
 * @param out Output EntityState structure
 * @return 0 on success, -1 on failure
 */
int serial_deserialize_entity_state(const cJSON* json, EntityState* out);

/**
 * @brief Serialize EntityState to cJSON object
 * @param entity Input EntityState structure
 * @return cJSON object (caller must free) or NULL on failure
 */
cJSON* serial_serialize_entity_state(const EntityState* entity);

/**
 * @brief Deserialize JSON object to PlayerState (player object)
 * @param json cJSON object with player data
 * @param out Output PlayerState structure
 * @return 0 on success, -1 on failure
 */
int serial_deserialize_player_state(const cJSON* json, PlayerState* out);

/**
 * @brief Serialize PlayerState to cJSON object
 * @param player Input PlayerState structure
 * @return cJSON object (caller must free) or NULL on failure
 */
cJSON* serial_serialize_player_state(const PlayerState* player);

/**
 * @brief Deserialize JSON object to BotState
 * @param json cJSON object with bot data
 * @param out Output BotState structure
 * @return 0 on success, -1 on failure
 */
int serial_deserialize_bot_state(const cJSON* json, BotState* out);

/**
 * @brief Serialize BotState to cJSON object
 * @param bot Input BotState structure
 * @return cJSON object (caller must free) or NULL on failure
 */
cJSON* serial_serialize_bot_state(const BotState* bot);

/**
 * @brief Deserialize JSON object to WorldObject
 * @param json cJSON object with world object data
 * @param out Output WorldObject structure
 * @return 0 on success, -1 on failure
 */
int serial_deserialize_world_object(const cJSON* json, WorldObject* out);

/**
 * @brief Serialize WorldObject to cJSON object
 * @param obj Input WorldObject structure
 * @return cJSON object (caller must free) or NULL on failure
 */
cJSON* serial_serialize_world_object(const WorldObject* obj);

/**
 * @brief Deserialize JSON array to path (Vector2 array)
 * @param json cJSON array of points
 * @param out Output array of Vector2
 * @param max_points Maximum number of points to deserialize
 * @return Number of points deserialized, or -1 on error
 */
int serial_deserialize_path(const cJSON* json, Vector2* out, int max_points);

/**
 * @brief Serialize path (Vector2 array) to cJSON array
 * @param path Input array of Vector2
 * @param count Number of points in path
 * @return cJSON array (caller must free) or NULL on failure
 */
cJSON* serial_serialize_path(const Vector2* path, int count);

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

/**
 * @brief Safely get double from cJSON object
 * @param json cJSON object
 * @param key Field name
 * @param out Output double pointer
 * @return 0 on success, -1 if field missing or not a number
 */
int serial_get_double(const cJSON* json, const char* key, double* out);

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

/**
 * @brief Create handshake JSON message
 * @param client_name Client identifier
 * @param version Client version
 * @return JSON string (caller must free) or NULL on failure
 */
char* serial_create_handshake(const char* client_name, const char* version);

/**
 * @brief Create player action JSON message
 * @param target_x Target X coordinate
 * @param target_y Target Y coordinate
 * @return JSON string (caller must free) or NULL on failure
 */
char* serial_create_player_action(float target_x, float target_y);

/**
 * @brief Create ping JSON message
 * @return JSON string (caller must free) or NULL on failure
 */
char* serial_create_ping(void);

/**
 * @brief Create pong JSON message
 * @return JSON string (caller must free) or NULL on failure
 */
char* serial_create_pong(void);

/**
 * @brief Create item activation message
 * @param item_id Item ID to activate
 * @param activate True to activate, false to deactivate
 * @return JSON string (caller must free) or NULL on failure
 */
char* serial_create_item_action(const char* item_id, bool activate);

#endif // SERIAL_H