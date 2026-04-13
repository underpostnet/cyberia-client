/**
 * @file nameplate.c
 * @brief Centralized display-name resolution implementation.
 */

#include "nameplate.h"
#include <stdio.h>
#include <string.h>

void nameplate_resolve(const char *entity_id,
                       bool is_player,
                       const ObjectLayerState *layers,
                       int layer_count,
                       ObjectLayersManager *mgr,
                       char *out,
                       int out_size) {
    if (!out || out_size <= 0) return;
    out[0] = '\0';

    if (!entity_id || entity_id[0] == '\0') return;

    /* ── Players: "AnonPlayer" + first 8 chars of websocket ID ────── */
    if (is_player) {
        snprintf(out, (size_t)out_size, "AnonPlayer%.8s", entity_id);
        return;
    }

    /* ── Bots: prefer the item_id of the "skin"/"body" layer ──────── */
    if (layers && layer_count > 0 && mgr) {
        const char *first_active_id = NULL;

        for (int i = 0; i < layer_count; i++) {
            if (!layers[i].active || layers[i].item_id[0] == '\0') continue;

            if (!first_active_id)
                first_active_id = layers[i].item_id;

            ObjectLayer *ol = get_or_fetch_object_layer(mgr, layers[i].item_id);
            if (ol && (strcmp(ol->data.item.type, "skin") == 0 ||
                       strcmp(ol->data.item.type, "body") == 0)) {
                snprintf(out, (size_t)out_size, "%s", layers[i].item_id);
                return;
            }
        }

        /* No skin/body found — use first active layer's item_id. */
        if (first_active_id) {
            snprintf(out, (size_t)out_size, "%s", first_active_id);
            return;
        }
    }

    /* Fallback — raw entity ID, truncated to fit. */
    snprintf(out, (size_t)out_size, "%s", entity_id);
}
