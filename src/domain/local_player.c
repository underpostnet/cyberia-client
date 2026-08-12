#include "local_player.h"

#include <raylib.h>
#include <string.h>

#include "network/game_client.h"
#include "util/serial.h"
#include "util/log.h"

#define LOCAL_PLAYER_DEFAULT_MOVE_SPEED 3.0f
/* Mirrors entityBaseActionCooldownMs, the instance default. Only in force
 * until the first snapshot carries the player's own, Utility-reduced value. */
#define LOCAL_PLAYER_DEFAULT_ACTION_COOLDOWN_S 0.5f
#define LOCAL_FREEZE_TIMEOUT_S          30.0
#define LOCAL_FREEZE_REASON_MAX         32

static struct {
    bool          frozen;
    uint8_t       status_icon;
    float         move_speed;
    float         action_cooldown_s;
    bool          on_portal;
    float         portal_hold_progress;
    LocalFctEvent fct[LOCAL_FCT_PENDING_MAX];
    int           fct_count;

    bool          freeze_pending;
    double        freeze_deadline;
    char          freeze_reason[LOCAL_FREEZE_REASON_MAX];
} g_local = {
    .move_speed        = LOCAL_PLAYER_DEFAULT_MOVE_SPEED,
    .action_cooldown_s = LOCAL_PLAYER_DEFAULT_ACTION_COOLDOWN_S,
};

static void send_freeze_frame(bool start, const char* reason) {
    bool rc = network_send(start ? json_pack_freeze_start(reason)
                                 : json_pack_freeze_end(reason));
    LOG_INFO("[LOCAL_PLAYER] WS -> freeze_%s (reason=%s) rc=%d",
             start ? "start" : "end", reason ? reason : "", rc);
}

void local_player_reset(void) {
    g_local.frozen               = false;
    g_local.status_icon          = 0;
    g_local.move_speed           = LOCAL_PLAYER_DEFAULT_MOVE_SPEED;
    g_local.action_cooldown_s    = LOCAL_PLAYER_DEFAULT_ACTION_COOLDOWN_S;
    g_local.on_portal            = false;
    g_local.portal_hold_progress = 0.0f;
    g_local.fct_count            = 0;
    g_local.freeze_pending  = false;
    g_local.freeze_deadline = 0.0;
    g_local.freeze_reason[0] = '\0';
}

void local_player_set_frozen(bool frozen) { g_local.frozen = frozen; }
bool local_player_is_frozen(void)         { return g_local.frozen; }

void local_player_request_freeze(bool start, const char* reason) {
    send_freeze_frame(start, reason);
    if (start) {
        strncpy(g_local.freeze_reason, reason ? reason : "", LOCAL_FREEZE_REASON_MAX - 1);
        g_local.freeze_reason[LOCAL_FREEZE_REASON_MAX - 1] = '\0';
        g_local.freeze_pending  = true;
        g_local.freeze_deadline = GetTime() + LOCAL_FREEZE_TIMEOUT_S;
    } else {
        g_local.freeze_pending = false;
    }
}

void local_player_keep_freeze(void) {
    if (!g_local.freeze_pending) return;
    g_local.freeze_deadline = GetTime() + LOCAL_FREEZE_TIMEOUT_S;
}

static void arm_freeze_watchdog(const char* reason) {
    strncpy(g_local.freeze_reason, reason ? reason : "", LOCAL_FREEZE_REASON_MAX - 1);
    g_local.freeze_reason[LOCAL_FREEZE_REASON_MAX - 1] = '\0';
    g_local.freeze_pending  = true;
    g_local.freeze_deadline = GetTime() + LOCAL_FREEZE_TIMEOUT_S;
}

void local_player_request_dialogue_start(const char* entity_id, const char* item_id) {
    network_send(json_pack_dialog_start(entity_id, item_id));
    arm_freeze_watchdog("dialogue");
}

/* Disarm the watchdog only while it still belongs to the dialogue. A caller
 * that re-bridged to another modal a moment earlier (modal_dialogue hands the
 * freeze back to "interact" before emitting the dlg frame) owns the watchdog
 * now, and clearing it here would silently disable local_player_keep_freeze
 * for the rest of that session. */
static void release_dialogue_watchdog(void) {
    if (0 == strcmp(g_local.freeze_reason, "dialogue")) g_local.freeze_pending = false;
}

void local_player_request_dialogue_complete(const char* entity_id, const char* item_id,
                                            const char* dialog_code) {
    network_send(json_pack_dialog_complete(entity_id, item_id, dialog_code));
    release_dialogue_watchdog();
}

void local_player_request_dialogue_cancel(const char* entity_id, const char* item_id) {
    network_send(json_pack_dialog_cancel(entity_id, item_id));
    release_dialogue_watchdog();
}

void local_player_request_quest_abandon(const char* quest_code) {
    network_send(json_pack_quest_abandon(quest_code));
}

void local_player_request_quest_accept(const char* entity_id, const char* quest_code) {
    network_send(json_pack_quest_accept(entity_id, quest_code));
}

void local_player_request_shop_buy(const char* entity_id, const char* item_id,
                                   int quantity) {
    if (quantity < 1) quantity = 1;
    if (quantity > 255) quantity = 255;
    network_send(json_pack_shop_buy(entity_id, item_id, quantity));
}

void local_player_request_craft(const char* entity_id, int recipe_index) {
    network_send(json_pack_craft_item(entity_id, recipe_index));
}

void local_player_request_craft_cancel(void) {
    network_send(json_pack_craft_cancel());
}

void local_player_request_storage_open(const char* entity_id) {
    network_send(json_pack_storage_open(entity_id));
}

void local_player_request_storage_move(const char* entity_id, int from_index, int to_index,
                                       int quantity) {
    network_send(json_pack_storage_move(entity_id, from_index, to_index, quantity));
}

void local_player_request_storage_swap(const char* entity_id, int from_index, int to_index) {
    network_send(json_pack_storage_swap(entity_id, from_index, to_index));
}

void local_player_request_storage_transfer(const char* entity_id, const char* item_id,
                                           int quantity, bool deposit,
                                           int from_index, int to_index) {
    network_send(json_pack_storage_transfer(entity_id, item_id, quantity, deposit,
                                            from_index, to_index));
}

void local_player_on_tick(void) {
    if (!g_local.freeze_pending) { return; }
    if (GetTime() < g_local.freeze_deadline) { return; }
    LOG_WARN("[LOCAL_PLAYER] freeze watchdog fired (reason=%s) — auto freeze_end",
             g_local.freeze_reason);
    send_freeze_frame(false, g_local.freeze_reason);
    g_local.freeze_pending = false;
}

void    local_player_set_status_icon(uint8_t id) { g_local.status_icon = id; }
uint8_t local_player_status_icon(void)           { return g_local.status_icon; }

void  local_player_set_move_speed(float speed) {
    if (speed > 0.0f) g_local.move_speed = speed;
}
float local_player_move_speed(void) { return g_local.move_speed; }

void local_player_set_action_cooldown(float seconds) {
    if (seconds > 0.0f) g_local.action_cooldown_s = seconds;
}
float local_player_action_cooldown(void) { return g_local.action_cooldown_s; }

void local_player_set_portal_hold(bool on_portal, float progress) {
    g_local.on_portal = on_portal;
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    g_local.portal_hold_progress = progress;
}
bool  local_player_on_portal(void)             { return g_local.on_portal; }
float local_player_portal_hold_progress(void)  { return g_local.portal_hold_progress; }

bool local_player_fct_push(const LocalFctEvent* ev) {
    if (!ev || g_local.fct_count >= LOCAL_FCT_PENDING_MAX) return false;
    g_local.fct[g_local.fct_count++] = *ev;
    return true;
}

int local_player_fct_count(void) { return g_local.fct_count; }

const LocalFctEvent* local_player_fct_at(int idx) {
    if (idx < 0 || idx >= g_local.fct_count) return NULL;
    return &g_local.fct[idx];
}

void local_player_fct_clear(void) { g_local.fct_count = 0; }
