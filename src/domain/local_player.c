#include "local_player.h"

#include <raylib.h>
#include <string.h>

#include "network/game_client.h"
#include "serial.h"
#include "util/log.h"

#define LOCAL_PLAYER_DEFAULT_MOVE_SPEED 3.0f
#define LOCAL_FREEZE_TIMEOUT_S          30.0
#define LOCAL_FREEZE_REASON_MAX         32

static struct {
    bool          frozen;
    uint8_t       status_icon;
    float         move_speed;
    LocalFctEvent fct[LOCAL_FCT_PENDING_MAX];
    int           fct_count;

    bool          freeze_pending;
    double        freeze_deadline;
    char          freeze_reason[LOCAL_FREEZE_REASON_MAX];
} g_local = {
    .move_speed = LOCAL_PLAYER_DEFAULT_MOVE_SPEED,
};

static void send_freeze_frame(bool start, const char* reason) {
    BinWriter w;
    if (start) { uplink_freeze_start(&w, reason); }
    else       { uplink_freeze_end(&w, reason); }
    bool rc = network_send_binary(w.buf, w.pos);
    LOG_INFO("[LOCAL_PLAYER] WS -> freeze_%s (reason=%s) rc=%d",
             start ? "start" : "end", reason ? reason : "", rc);
}

void local_player_reset(void) {
    g_local.frozen          = false;
    g_local.status_icon     = 0;
    g_local.move_speed      = LOCAL_PLAYER_DEFAULT_MOVE_SPEED;
    g_local.fct_count       = 0;
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

static void arm_freeze_watchdog(const char* reason) {
    strncpy(g_local.freeze_reason, reason ? reason : "", LOCAL_FREEZE_REASON_MAX - 1);
    g_local.freeze_reason[LOCAL_FREEZE_REASON_MAX - 1] = '\0';
    g_local.freeze_pending  = true;
    g_local.freeze_deadline = GetTime() + LOCAL_FREEZE_TIMEOUT_S;
}

void local_player_request_dialogue_start(const char* entity_id, const char* item_id) {
    BinWriter w;
    uplink_dlg_start(&w, entity_id, item_id);
    network_send_binary(w.buf, w.pos);
    arm_freeze_watchdog("dialogue");
}

void local_player_request_dialogue_complete(const char* entity_id, const char* item_id,
                                            const char* dialog_code) {
    BinWriter w;
    uplink_dlg_complete(&w, entity_id, item_id, dialog_code);
    network_send_binary(w.buf, w.pos);
    g_local.freeze_pending = false;
}

void local_player_request_dialogue_cancel(const char* entity_id, const char* item_id) {
    BinWriter w;
    uplink_dlg_cancel(&w, entity_id, item_id);
    network_send_binary(w.buf, w.pos);
    g_local.freeze_pending = false;
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
