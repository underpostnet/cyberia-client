#ifndef CYBERIA_DOMAIN_LOCAL_PLAYER_H
#define CYBERIA_DOMAIN_LOCAL_PLAYER_H

#include <stdbool.h>
#include <stdint.h>

#include "../object_layer.h"

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
