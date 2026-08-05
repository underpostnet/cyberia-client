#ifndef SERIAL_H
#define SERIAL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <cJSON.h>

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
 * Uplink Binary Writer
 * ============================================================================
 *
 * All client→server messages are little-endian binary frames.
 * msgType byte 0 discriminates the message:
 *
 *   0x10  handshake       u8 nameLen + str name, u8 verLen + str version
 *   0x11  player_action   f32 targetX, f32 targetY
 *   0x12  item_activation u8 idLen + str itemId, u8 active (0|1)
 *   0x13  freeze_start    u8 reasonLen + str reason
 *   0x14  freeze_end      u8 reasonLen + str reason
 *   0x15  chat            u8 toIdLen + str toId, u8 textLen + str text
 */

#define UPLINK_HANDSHAKE       0x10
#define UPLINK_PLAYER_ACTION   0x11
#define UPLINK_ITEM_ACTIVATION 0x12
#define UPLINK_FREEZE_START    0x13
#define UPLINK_FREEZE_END      0x14
#define UPLINK_CHAT            0x15
/* 0x16 retired (was get_items_ids) */
#define UPLINK_DLG_START       0x17
#define UPLINK_DLG_COMPLETE    0x18
#define UPLINK_DLG_CANCEL      0x19
#define UPLINK_QUEST_ABANDON   0x1A
#define UPLINK_QUEST_ACCEPT    0x1B
#define UPLINK_SHOP_BUY        0x1C

typedef struct {
    uint8_t  buf[256];
    uint16_t pos;
} BinWriter;

void bw_init(BinWriter* w, uint8_t msg_type);
void bw_u8(BinWriter* w, uint8_t v);
void bw_u32(BinWriter* w, uint32_t v);
void bw_f32(BinWriter* w, float v);
/* Write a length-prefixed string (1-byte length prefix, max 255 chars). */
void bw_str(BinWriter* w, const char* s);

/* ── Message builders ── */
void uplink_handshake(BinWriter* w, const char* client_name, const char* version);

/* uplink_player_action — TAP event.
 *
 * Wire layout:  [u8 kind][f32 x][f32 y][u32 clientTick][u32 sequence]
 *
 * client_tick + sequence let the server echo back lastAckedSequence in
 * every snapshot header, which the client's prediction module uses to
 * drain its replay buffer.  Without them the buffer would grow unbounded
 * and prediction_reconcile would replay every historical tap on every
 * snapshot — the cause of the "tap → freeze / oscillation" symptom. */
void uplink_player_action(BinWriter* w, float target_x, float target_y,
                          uint32_t client_tick, uint32_t sequence);

void uplink_item_activation(BinWriter* w, const char* item_id, bool active);
void uplink_freeze_start(BinWriter* w, const char* reason);
void uplink_freeze_end(BinWriter* w, const char* reason);
void uplink_chat(BinWriter* w, const char* to_id, const char* text);

/* Dialogue interaction frames. The server resolves the bound action and
 * quest from its own cache; the client only reports which entity it talked
 * to and (on complete) which dialogue group it finished reading. */
void uplink_dlg_start(BinWriter* w, const char* entity_id, const char* item_id);
void uplink_dlg_complete(BinWriter* w, const char* entity_id, const char* item_id,
                         const char* dialog_code);
void uplink_dlg_cancel(BinWriter* w, const char* entity_id, const char* item_id);

/* Abandon an active quest by code — server moves it to the failed section. */
void uplink_quest_abandon(BinWriter* w, const char* quest_code);

/* Accept the quest the entity offers — the only path to start a mission. */
void uplink_quest_accept(BinWriter* w, const char* entity_id, const char* quest_code);

/* Buy `quantity` units of a vendor action's catalog item. The server owns the
 * price, the balance check, and the quantity clamp; the client only names the
 * vendor, the item, and how many the player confirmed. */
void uplink_shop_buy(BinWriter* w, const char* entity_id, const char* item_id,
                     uint8_t quantity);

#endif // SERIAL_H
