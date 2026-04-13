/**
 * @file interact_bridge.h
 * @brief C ↔ JS bridge for the interact overlay.
 *
 * Declares:
 *   - `extern` JS functions callable from C (implemented in interact_overlay.js)
 *   - `EMSCRIPTEN_KEEPALIVE` C functions callable from JS (Module._xxx)
 *
 * Data flow:
 *   C interaction_bubble click  →  js_interact_overlay_open()    → JS builds DOM
 *   JS "Dialog" button          →  c_open_dialogue_from_js()     → C modal_dialogue
 *   JS overlay close            →  c_interact_overlay_did_close()→ C cleanup
 *   JS chat send                →  c_send_ws_message()           → C client_send()
 *   C incoming chat WS msg      →  js_interact_overlay_receive_chat() → JS DOM
 *   C dialogue modal close cb   →  js_interact_overlay_restore() → JS restores DOM
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
                                     int border_a);

extern void js_interact_overlay_close(void);

extern int  js_interact_overlay_is_open(void);

extern void js_interact_overlay_set_ol_stack(const char* json);

extern void js_interact_overlay_receive_chat(const char* from_id,
                                             const char* from_name,
                                             const char* text);

extern void js_interact_overlay_restore(void);

#endif /* INTERACT_BRIDGE_H */
