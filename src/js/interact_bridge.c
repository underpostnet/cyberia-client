/**
 * @file interact_bridge.c
 * @brief C-side exports for the JS interact overlay.
 *
 * EMSCRIPTEN_KEEPALIVE functions are exported and callable from JS as
 * Module._c_xxx().  They bridge the JS overlay back into the C game
 * systems (WebSocket send via binary uplink).
 */

#include "interact_bridge.h"

#include "network/game_client.h"
#include "serial.h"

#include <emscripten.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

/* ── Exported C functions (called from JS via Module._xxx) ────────────── */

EMSCRIPTEN_KEEPALIVE
void c_send_chat_binary(const char* to_id, const char* text) {
    if (!to_id || !text) return;
    network_send_chat(to_id, text);
}

EMSCRIPTEN_KEEPALIVE
void c_interact_overlay_closed(void) {
    extern void modal_interact_overlay_closed(void);
    modal_interact_overlay_closed();
}
