#ifndef NAMEPLATE_H
#define NAMEPLATE_H

#include "object_layer.h"
#include "object_layers_management.h"

#include <stdbool.h>

/* One display name for each entity, so the overhead HUD, the interaction
 * bubble, and the JS overlay header always agree.
 *
 *   Players — "Anon-" plus the first 8 characters of the websocket ID.
 *   Bots    — active skin item_id, first letter capital, plus "-" and the
 *             first 8 characters of the entity ID. Falls back to the first
 *             active layer, then to the raw ID. */

/* `entity_id` is the websocket session ID for a player, the server UUID for
 * a bot. `layers` may be NULL. A NULL `mgr` skips the bot skin lookup.
 * `out_size` counts the terminator. */
void nameplate_resolve(const char *entity_id,
                       bool is_player,
                       const ObjectLayerState *layers,
                       int layer_count,
                       ObjectLayersManager *mgr,
                       char *out,
                       int out_size);

/* Resolve a portal nameplate: "<target_map_code> <x>,<y>" for a fixed target, or
 * just "<target_map_code>" for a random target (negative cell). Empty when
 * target_map_code is empty. */
void nameplate_resolve_portal(const char *target_map_code,
                              int target_cell_x,
                              int target_cell_y,
                              char *out,
                              int out_size);

#endif /* NAMEPLATE_H */
