/**
 * @file interact_bridge.h
 * @brief C ↔ JS bridge for the interact overlay.
 *
 * Declares:
 *   - `extern` JS functions callable from C (implemented in interact_overlay.js)
 *   - `EMSCRIPTEN_KEEPALIVE` C functions callable from JS (Module._xxx)
 *
 * Data flow:
 *   C interaction_bubble click  →  js_interact_overlay_open()         → JS builds DOM
 *   JS chat send                →  c_send_chat_binary()               → C network_send_binary()
 *   C incoming chat WS msg      →  js_interact_overlay_receive_chat() → JS DOM
 */

#ifndef INTERACT_BRIDGE_H
#define INTERACT_BRIDGE_H

#include <stdint.h>

/* ── JS functions (implemented in interact_overlay.js, called from C) ── */

extern void js_interact_overlay_open(const char* entity_id,
                                     const char* display_name,
                                     const char* dlg_item_id,
                                     uint32_t interact_flags,
                                     int is_player,
                                     int is_self,
                                     int border_r,
                                     int border_g,
                                     int border_b,
                                     int border_a,
                                     int initial_tab);

extern void js_interact_overlay_close(void);

extern int  js_interact_overlay_is_open(void);

extern void js_interact_overlay_set_ol_stack(const char* json);

extern void js_interact_overlay_receive_chat(const char* from_id,
                                             const char* from_name,
                                             const char* text);

/* Set the engine API base URL on the JS side (FetchState.api_base_url),
 * consulted when building DOM <img> asset previews. For REST/blob fetches
 * prefer the native engine fetch API (network/engine_client.h:
 * fetch_request_start) over adding a JS bridge here. */
extern void js_init_engine_api(const char* api_base_url);

/* ── C functions (EMSCRIPTEN_KEEPALIVE, called from JS as Module._xxx) ── */

void c_send_chat_binary(const char* to_id, const char* text);

/* Called from JS when the interact overlay closes — reopens modal_interact. */
void c_interact_overlay_closed(void);

#endif /* INTERACT_BRIDGE_H */
