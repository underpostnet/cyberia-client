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
 *   cJSON* j = json_pack_auth(ticket);
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

/* The first message of a connection: the launch ticket, or none for a guest. */
cJSON* json_pack_auth(const char* ticket);

/* The one input message: a batch of client events. Takes ownership of the
 * events array. Each entry is a {type, payload} envelope from a packer below,
 * with seq, frame and timestamp added to its payload. */
cJSON* json_pack_events(cJSON* events);

/* Client event packers. Only client_event_flush calls them. */
cJSON* json_pack_player_action(float target_x, float target_y);

/* Equip or unequip the stack of item_id in bag slot `slot`. */
cJSON* json_pack_item_active(const char* item_id, int slot, bool active);
cJSON* json_pack_player_stasis(bool stasis);
/* `channel` is a chat_channel_t; only a direct line names to_id. */
cJSON* json_pack_chat(int channel, uint32_t to_id, const char* text);

/* The player read dialogue group `dialog_code` with the entity to the end. The
 * server resolves the bound action and quest from its own cache. */
cJSON* json_pack_talk_done(uint32_t entity_id, const char* dialog_code);

/* Abandon an active quest by code — the server moves it to the failed section. */
cJSON* json_pack_quest_abandon(const char* quest_code);

/* Accept the quest the entity offers — the only path to start a mission. */
cJSON* json_pack_quest_accept(uint32_t entity_id, const char* quest_code);

/* Buy `quantity` units of a vendor's catalog item. The server owns the price,
 * the balance check, and the quantity clamp. */
cJSON* json_pack_shop_buy(uint32_t entity_id, const char* item_id,
                          int quantity);

/* Assemble the assembler recipe at `recipe_index`. The server owns the recipe
 * book and the ingredient check; the client names the terminal, the recipe the
 * player triggered and the fee it saw (`quote`). */
cJSON* json_pack_craft_item(uint32_t entity_id, int recipe_index, double quote);

/* Abort the running assembly. The server refunds its ingredients. */
cJSON* json_pack_craft_cancel(void);

/* Move `qty` units of the stack in `from_slot` of box `from` to `to_slot` of
 * box `to`, each box by its wire word. A vault move names its terminal in
 * `entity_id`, else 0. */
cJSON* json_pack_item_move(const char* from, int from_slot, const char* to, int to_slot, int qty,
                           uint32_t entity_id);

/* A party or friend request of type `type`. `key` names the player field
 * (toId), or is NULL for a request that names none. */
cJSON* json_pack_player_target(const char* type, const char* key, uint32_t player_id);
cJSON* json_pack_party_reply(uint32_t from_id, bool accept);

/* The end of the session. `travel` names the world a travel in this page
 * enters, or is empty. */
cJSON* json_pack_session_leave(const char* travel);

/* An answer to a friend request (friend_reply) or the end of a friendship
 * (friend_remove), by friendship id. */
cJSON* json_pack_friendship(const char* type, const char* friendship_id, bool accept);

/* Drop `qty` units of the stack in `slot` of box `box` on the cell (x, y);
 * a negative cell drops at the feet. */
cJSON* json_pack_item_drop(const char* box, int slot, int quantity, float x, float y);

/* Ask the price of a sale (`burn` false) or a burn of the stack in `slot` of
 * box `box` at terminal `entity_id`. */
cJSON* json_pack_item_quote(uint32_t entity_id, bool burn, const char* box, int slot);

/* Confirm a quote (item_sell or item_burn) for `quantity` units. */
cJSON* json_pack_quote_confirm(const char* type, const char* quote_id, int quantity);

/* Ask the market of terminal `entity_id` for the page at cursor `after`
 * ("" the first), of item `item_id` ("" every item) and `mode` ("" every mode). */
cJSON* json_pack_market_open(uint32_t entity_id, const char* item_id, const char* mode, const char* after);

/* List `quantity` units of the stack in `slot` of box `box` at terminal
 * `entity_id`: `mode` fixed or auction, `price` the price or the opening bid. */
cJSON* json_pack_market_list(uint32_t entity_id, const char* box, int slot, int quantity, const char* mode, int price,
                             int hours);

/* Cancel, buy or bid on (`type`) listing `listing_id` at terminal `entity_id`;
 * `price` is the bid, 0 for any other type. */
cJSON* json_pack_market_listing(const char* type, uint32_t entity_id, const char* listing_id, int price);

/* The whole offer of the player in its trade: bag slots with units, and coins. */
cJSON* json_pack_trade_offer(const int* slots, const int* qtys, int count, int coins);

/* A confirmation of the trade offers of `revision`. */
cJSON* json_pack_trade_confirm(uint32_t revision);

/* An answer to a trade request of player `from_id`. */
cJSON* json_pack_trade_reply(uint32_t from_id, bool accept);

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
double serial_get_double_default(const cJSON* json, const char* key, double default_val);
bool  serial_get_bool_default(const cJSON* json, const char* key, bool default_val);

#endif // CYBERIA_UTIL_SERIAL_H
