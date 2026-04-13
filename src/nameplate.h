/**
 * @file nameplate.h
 * @brief Centralized display-name resolution for all entities.
 *
 * Provides a single function that computes the nameplate string for
 * any entity — player or bot — so every UI surface (overhead HUD,
 * interaction bubble, JS overlay header) shows a consistent name.
 *
 * Rules:
 *   - **Players** → "AnonPlayer" + first 8 characters of the websocket ID.
 *   - **Bots**    → The item_id of the active "skin" (or "body") layer.
 *                    Falls back to the first active layer's item_id,
 *                    then to the raw entity ID truncated to fit.
 */

#ifndef NAMEPLATE_H
#define NAMEPLATE_H

#include <stdbool.h>
#include "object_layer.h"
#include "object_layers_management.h"

/**
 * @brief Resolve a display name into @p out.
 *
 * @param entity_id   Entity's unique ID (websocket session ID for players,
 *                     server-assigned UUID for bots).
 * @param is_player   true for main player and other players, false for bots.
 * @param layers      Entity's active ObjectLayerState array (may be NULL).
 * @param layer_count Number of entries in @p layers.
 * @param mgr         ObjectLayersManager for type look-ups (may be NULL;
 *                     bot skin detection is skipped when NULL).
 * @param out         Destination buffer for the resolved name.
 * @param out_size    Size of @p out in bytes (including NUL terminator).
 */
void nameplate_resolve(const char *entity_id,
                       bool is_player,
                       const ObjectLayerState *layers,
                       int layer_count,
                       ObjectLayersManager *mgr,
                       char *out,
                       int out_size);

#endif /* NAMEPLATE_H */
