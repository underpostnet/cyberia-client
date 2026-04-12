/**
 * @file interact_bridge.c
 * @brief C-side exports for the JS interact overlay.
 *
 * EMSCRIPTEN_KEEPALIVE functions are exported and callable from JS as
 * Module._c_xxx().  They bridge the JS overlay back into the C game
 * systems (WebSocket send, dialogue modal open).
 */

#include "interact_bridge.h"
#include "client.h"
#include "modal_dialogue.h"
#include "dialogue_data.h"
#include <emscripten.h>
#include <stdio.h>
#include <string.h>

/* ── Entity context (set when overlay opens) ──────────────────────────── */

static char s_entity_id[64]  = {0};
static char s_dlg_item_id[128] = {0};

/* ── Callback: restore JS overlay after dialogue modal closes ─────────── */

static void on_dialogue_close(void) {
    printf("[INTERACT_BRIDGE] Dialogue modal closed → restore overlay\n");
    js_interact_overlay_restore();
}

/* ── Exported C functions (called from JS via Module._xxx) ────────────── */

EMSCRIPTEN_KEEPALIVE
void c_send_ws_message(const char* json) {
    if (!json || json[0] == '\0') return;
    client_send(json);
}

EMSCRIPTEN_KEEPALIVE
void c_open_dialogue_from_js(const char* entity_id, const char* dlg_item_id) {
    if (!entity_id || !dlg_item_id || dlg_item_id[0] == '\0') {
        printf("[INTERACT_BRIDGE] c_open_dialogue_from_js: invalid args\n");
        return;
    }

    strncpy(s_entity_id, entity_id, sizeof(s_entity_id) - 1);
    s_entity_id[sizeof(s_entity_id) - 1] = '\0';
    strncpy(s_dlg_item_id, dlg_item_id, sizeof(s_dlg_item_id) - 1);
    s_dlg_item_id[sizeof(s_dlg_item_id) - 1] = '\0';

    const DialogueDataSet* d = dialogue_data_get(dlg_item_id);
    if (!d || d->state != DLG_DATA_READY || d->line_count <= 0) {
        printf("[INTERACT_BRIDGE] No dialogue data ready for item=%s\n", dlg_item_id);
        js_interact_overlay_restore();
        return;
    }

    printf("[INTERACT_BRIDGE] Opening native dialogue: entity=%s item=%s lines=%d\n",
           entity_id, dlg_item_id, d->line_count);

    modal_dialogue_set_on_close(on_dialogue_close);
    modal_dialogue_open(entity_id, dlg_item_id, d->lines, d->line_count);
}

EMSCRIPTEN_KEEPALIVE
void c_interact_overlay_did_close(void) {
    printf("[INTERACT_BRIDGE] Panel closed\n");
}
