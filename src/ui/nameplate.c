/**
 * @file nameplate.c
 * @brief Centralized display-name resolution implementation.
 */

#include "nameplate.h"

#include "domain/presentation_runtime.h"

#include <assert.h>
#include <ctype.h>
#include <stdio.h>
#include <string.h>

/**
 * @brief Copy src into dst, uppercasing the first character.
 */
static void copy_ucfirst(char *dst, int dst_size, const char *src) {
    if (!dst || dst_size <= 0 || !src || src[0] == '\0') return;
    snprintf(dst, (size_t)dst_size, "%s", src);
    dst[0] = (char)toupper((unsigned char)dst[0]);
}

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

    /* The "-<id hash>" suffix is a dev aid; off-mode shows the clean label. */
    bool dev = presentation_runtime_dev_ui();

    /* ── Players: "Anon" (+ "-<first 8 chars of websocket ID>" in dev). ── */
    if (is_player) {
        if (dev) snprintf(out, (size_t)out_size, "Anon-%.8s", entity_id);
        else     snprintf(out, (size_t)out_size, "Anon");
        return;
    }

    /* ── Bots: "<Skin_item_id>" (+ "-<first 8 chars of entity ID>" in dev),
     *    first character uppercased. ───────────────────────────────────── */
    if (layers && layer_count > 0 && mgr) {
        const char *skin_id = NULL;
        const char *first_active_id = NULL;

        for (int i = 0; i < layer_count; i++) {
            if (!layers[i].active || layers[i].item_id[0] == '\0') continue;

            if (!first_active_id)
                first_active_id = layers[i].item_id;

            ObjectLayer *ol = lookup_cached_layer(layers[i].item_id);
            if (ol && (strcmp(ol->data.item.type, "skin") == 0 ||
                       strcmp(ol->data.item.type, "body") == 0)) {
                skin_id = layers[i].item_id;
                break;
            }
        }

        const char *label = skin_id ? skin_id : first_active_id;
        if (label) {
            char ucbuf[64];
            copy_ucfirst(ucbuf, (int)sizeof(ucbuf), label);
            if (dev) snprintf(out, (size_t)out_size, "%s-%.8s", ucbuf, entity_id);
            else     snprintf(out, (size_t)out_size, "%s", ucbuf);
            return;
        }
    }

    /* No resolvable label — only the raw id remains, shown in dev mode only. */
    if (dev) snprintf(out, (size_t)out_size, "%.8s", entity_id);
}
