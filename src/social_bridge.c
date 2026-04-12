/**
 * @file social_bridge.c
 * @brief C-side exports for the JS social overlay.
 *
 * EMSCRIPTEN_KEEPALIVE functions are exported and callable from JS as
 * Module._c_xxx().  They bridge the JS overlay back into the C game
 * systems (WebSocket send, dialogue modal open, freeze state).
 */

#include "js/services.h"
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
    printf("[SOCIAL_BRIDGE] Dialogue modal closed → restore JS overlay\n");
    js_social_overlay_restore();
}

/* ── Exported C functions (called from JS via Module._xxx) ────────────── */

/**
 * Send a raw JSON string over the WebSocket.
 * JS calls this to relay chat messages through the existing WS connection.
 */
EMSCRIPTEN_KEEPALIVE
void c_send_ws_message(const char* json) {
    if (!json || json[0] == '\0') return;
    client_send(json);
}

/**
 * Open the native C dialogue modal (Raylib rendering with OL icon + typewriter).
 * JS calls this when the user taps "Open Dialogue" in the dialogue tab.
 *
 * Sends freeze_start("dialogue") and sets up on_close callback to restore
 * the JS overlay when the dialogue finishes.
 */
EMSCRIPTEN_KEEPALIVE
void c_open_dialogue_from_js(const char* entity_id, const char* dlg_item_id) {
    if (!entity_id || !dlg_item_id || dlg_item_id[0] == '\0') {
        printf("[SOCIAL_BRIDGE] c_open_dialogue_from_js: invalid args\n");
        return;
    }

    strncpy(s_entity_id, entity_id, sizeof(s_entity_id) - 1);
    s_entity_id[sizeof(s_entity_id) - 1] = '\0';
    strncpy(s_dlg_item_id, dlg_item_id, sizeof(s_dlg_item_id) - 1);
    s_dlg_item_id[sizeof(s_dlg_item_id) - 1] = '\0';

    const DialogueDataSet* d = dialogue_data_get(dlg_item_id);
    if (!d || d->state != DLG_DATA_READY || d->line_count <= 0) {
        printf("[SOCIAL_BRIDGE] No dialogue data ready for item=%s\n", dlg_item_id);
        js_social_overlay_restore();
        return;
    }

    printf("[SOCIAL_BRIDGE] Opening native dialogue: entity=%s item=%s lines=%d\n",
           entity_id, dlg_item_id, d->line_count);

    /* Set callback so JS overlay is restored when dialogue closes */
    modal_dialogue_set_on_close(on_dialogue_close);

    modal_dialogue_open(entity_id, dlg_item_id, d->lines, d->line_count);
}

/**
 * Called from JS when the social overlay closes (user taps X or backdrop).
 * Sends freeze_end("social") to the server.
 */
EMSCRIPTEN_KEEPALIVE
void c_social_overlay_did_close(void) {
    printf("[SOCIAL_BRIDGE] Panel closed\n");
    /* No freeze_end — social panel doesn't freeze the player.
     * Only NPC dialogue (modal_dialogue.c) uses freeze state. */
}
