#ifndef CYBERIA_DOMAIN_LOCAL_PLAYER_H
#define CYBERIA_DOMAIN_LOCAL_PLAYER_H

#include <stdbool.h>
#include <stdint.h>

#include "object_layer.h"

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
    uint8_t  type;
    char     item_id[MAX_ITEM_ID_LENGTH]; /* empty → numeric FCT */
    uint32_t item_qty;
} LocalFctEvent;

/* Reset all local-player flags to their post-disconnect defaults. */
void  local_player_reset(void);

/* Frozen flag — authoritative server signal. */
void  local_player_set_frozen(bool frozen);
bool  local_player_is_frozen(void);

/* Request the server to freeze/unfreeze the local player for an interaction
 * (dialogue, inventory, ...). Owns the uplink_freeze_* network dispatch so UI
 * modules never drive the wire directly. A freeze_start arms a client-side
 * watchdog that auto-sends freeze_end if the matching end never arrives
 * (e.g. the UI closed via a path that skipped it, or a crash interrupted it). */
void  local_player_request_freeze(bool start, const char* reason);

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
void  local_player_request_quest_accept(const char* entity_id);

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

/* FCT event queue — single-producer (binary_aoi_decoder) /
 * single-consumer (floating_combat_text). */
bool                   local_player_fct_push(const LocalFctEvent* ev);
int                    local_player_fct_count(void);
const LocalFctEvent*   local_player_fct_at(int idx);
void                   local_player_fct_clear(void);

#endif /* CYBERIA_DOMAIN_LOCAL_PLAYER_H */
