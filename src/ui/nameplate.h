#ifndef NAMEPLATE_H
#define NAMEPLATE_H

#include "world/object_layer.h"
#include "world/object_layers_management.h"
#include "world/world_types.h"

#include <stdbool.h>

/* One display name for each entity, so the overhead HUD, the interaction
 * bubble, and the JS overlay header always agree.
 *
 *   Players — the name the server gives the player.
 *   Bots    — active skin item_id, first letter capital.
 * The dev UI adds "-" and the entity id. A bot falls back to the first active
 * layer, then to the raw id. */

/* `layers` may be NULL. `out_size` counts the terminator. */
void nameplate_resolve(EntityID entity_id,
                       bool is_player,
                       const ObjectLayerState *layers,
                       int layer_count,
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
