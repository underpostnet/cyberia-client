#ifndef CYBERIA_UTIL_SERIAL_H
#define CYBERIA_UTIL_SERIAL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <cJSON.h>

/* Serialization: objects to bytes and bytes to objects.
 *
 * Every message uses the envelope {"type": <name>, "payload": { ... }},
 * both directions. The socket layer only moves the bytes.
 *
 *   cJSON* j = json_pack_player_action(x, y, tick, seq);
 *   RawPack p = serial_pack(j);          // takes ownership of j
 *   socket_send(&ws, p.data, p.len);
 *   free(p.data);
 */

/* RawPack holds packed bytes. The caller frees data. */
typedef struct {
    uint8_t* data;
    size_t   len;
} RawPack;

/* Pack an object to bytes. Frees obj. */
RawPack serial_pack(cJSON* obj);

/* Unpack bytes to an object, or NULL if the bytes are not valid JSON.
 * The caller deletes the result. */
cJSON* serial_unpack(const uint8_t* data, size_t len);

/* ── Message packers — client to server ─────────────────────────────── */

cJSON* json_pack_handshake(const char* client_name, const char* version);

/* json_pack_player_action — TAP event.
 *
 * tick + seq let the server echo back the applied sequence in every
 * snapshot, which the prediction module uses to drain its replay buffer. */
cJSON* json_pack_player_action(float target_x, float target_y,
                               uint32_t client_tick, uint32_t sequence);

cJSON* json_pack_item_active(const char* item_id, bool active);
cJSON* json_pack_freeze_start(const char* reason);
cJSON* json_pack_freeze_end(const char* reason);
cJSON* json_pack_chat(const char* to_id, const char* text);

/* Dialogue messages. The server resolves the bound action and quest from its
 * own cache; the client only reports which entity it talked to and (on
 * complete) which dialogue group it finished reading. */
cJSON* json_pack_dialog_start(const char* entity_id, const char* item_id);
cJSON* json_pack_dialog_complete(const char* entity_id, const char* item_id,
                                 const char* dialog_code);
cJSON* json_pack_dialog_cancel(const char* entity_id, const char* item_id);

/* Abandon an active quest by code — the server moves it to the failed section. */
cJSON* json_pack_quest_abandon(const char* quest_code);

/* Accept the quest the entity offers — the only path to start a mission. */
cJSON* json_pack_quest_accept(const char* entity_id, const char* quest_code);

/* Buy `quantity` units of a vendor's catalog item. The server owns the price,
 * the balance check, and the quantity clamp. */
cJSON* json_pack_shop_buy(const char* entity_id, const char* item_id,
                          int quantity);

/* ── Read helpers ───────────────────────────────────────────────────── */

/* Copy a string field into out. Returns 0 on success, -1 if the field is
 * missing or is not a string. */
int serial_get_string(const cJSON* json, const char* key, char* out, size_t max_len);

/* Get an object or array field, or NULL when absent or of the wrong kind. */
cJSON* serial_get_object(const cJSON* json, const char* key);
cJSON* serial_get_array(const cJSON* json, const char* key);

/* Get a field, or default_val when absent or of the wrong kind. */
int   serial_get_int_default(const cJSON* json, const char* key, int default_val);
/* Reads through the double value, so a tick or sequence above INT_MAX
 * survives. cJSON clamps its int field at INT_MAX. */
uint32_t serial_get_u32_default(const cJSON* json, const char* key, uint32_t default_val);
float serial_get_float_default(const cJSON* json, const char* key, float default_val);
bool  serial_get_bool_default(const cJSON* json, const char* key, bool default_val);

#endif // CYBERIA_UTIL_SERIAL_H
