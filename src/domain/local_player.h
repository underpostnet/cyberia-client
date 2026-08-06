#ifndef CYBERIA_DOMAIN_LOCAL_PLAYER_H
#define CYBERIA_DOMAIN_LOCAL_PLAYER_H

#include <stdbool.h>
#include <stdint.h>

#include "ui/floating_combat_text.h"   /* FCTType */

/* Local-player state.
 *
 * Holds the client-side view of the local player that the simulation
 * server pushes through the AOI self-player block:
 *
 *   - frozen flag (FrozenInteractionState)
 *   - status icon ID (overhead UI hint)
 *   - authoritative move speed (cells/second) for the prediction integrator
 *   - per-frame FCT event queue drained by the floating combat text module
 *
 * These are render-only flags / per-tick view models, not world state, so
 * they live outside the simulation-shaped GameState.
 */

#define LOCAL_FCT_PENDING_MAX 64

typedef struct {
    float    world_x;
    float    world_y;
    uint32_t value;
    FCTType  type;
} LocalFctEvent;

/* Reset all local-player flags to their post-disconnect defaults. */
void  local_player_reset(void);

/* Frozen flag — authoritative server signal. */
void  local_player_set_frozen(bool frozen);
bool  local_player_is_frozen(void);

/* Request the server to freeze/unfreeze the local player for an interaction
 * (dialogue, inventory, ...). Owns the freeze_start/freeze_end dispatch so UI
 * modules never drive the wire directly. A freeze_start arms a client-side
 * watchdog that auto-sends freeze_end if the matching end never arrives
 * (e.g. the UI closed via a path that skipped it, or a crash interrupted it). */
void  local_player_request_freeze(bool start, const char* reason);

/* Renew the freeze watchdog without touching the wire. A modal that owns the
 * freeze calls this every frame it stays open, so a player who lingers past
 * LOCAL_FREEZE_TIMEOUT_S is not auto-thawed — and killed — while still inside
 * it. The server freeze has no timeout of its own; only the matching
 * freeze_end (or this watchdog) releases it. */
void  local_player_keep_freeze(void);

/* Dialogue interaction frames. dlg_start freezes the player server-side
 * (modal protection) and arms the same watchdog as a freeze with reason
 * "dialogue"; dlg_complete / dlg_cancel release it. The server resolves the
 * bound action and advances quest progress on dlg_complete — the client
 * only reports the entity and the dialogue group it finished reading. */
void  local_player_request_dialogue_start(const char* entity_id, const char* item_id);
void  local_player_request_dialogue_complete(const char* entity_id, const char* item_id,
                                             const char* dialog_code);
void  local_player_request_dialogue_cancel(const char* entity_id, const char* item_id);

/* Abandon an active quest by code — server moves it to the failed section. */
void  local_player_request_quest_abandon(const char* quest_code);

/* Accept the quest the entity offers — the only path to start a mission. */
void  local_player_request_quest_accept(const char* entity_id, const char* quest_code);

/* Buy `quantity` units of a vendor entity's catalog item. The server validates
 * the total price against the player's balance and replies with a shop_ack. */
void  local_player_request_shop_buy(const char* entity_id, const char* item_id,
                                    int quantity);

/* Advance the freeze watchdog; call once per render frame. */
void  local_player_on_tick(void);

/* Status icon ID (mirrors entity status indicator for self). */
void     local_player_set_status_icon(uint8_t id);
uint8_t  local_player_status_icon(void);

/* Authoritative move speed in grid units per second, pushed by the server
 * in every AOI self-player block. Returns a non-zero positive value at
 * steady state; before the first snapshot it returns the bootstrap default. */
void   local_player_set_move_speed(float speed);
float  local_player_move_speed(void);

/* Portal hold — authoritative teleport charge for the local player. on_portal
 * mirrors the server OnPortal flag; hold_progress is the 0..1 fraction of the
 * hold time elapsed. Both come from the AOI self-player block; the HUD renders
 * the progress bar only while on_portal is set. */
void   local_player_set_portal_hold(bool on_portal, float progress);
bool   local_player_on_portal(void);
float  local_player_portal_hold_progress(void);

/* FCT event queue — single-producer (message.c) /
 * single-consumer (floating_combat_text). */
bool                   local_player_fct_push(const LocalFctEvent* ev);
int                    local_player_fct_count(void);
const LocalFctEvent*   local_player_fct_at(int idx);
void                   local_player_fct_clear(void);

#endif /* CYBERIA_DOMAIN_LOCAL_PLAYER_H */
