/**
 * @file social_bridge.h
 * @brief C ↔ JS bridge for the social overlay.
 *
 * Declares:
 *   - `extern` JS functions callable from C (implemented in social_overlay.js)
 *   - `EMSCRIPTEN_KEEPALIVE` C functions callable from JS (Module._xxx)
 *
 * Data flow:
 *   C interaction_bubble click  →  js_social_overlay_open()    → JS builds DOM
 *   JS "Open Dialogue" button   →  c_open_dialogue_from_js()  → C modal_dialogue
 *   JS overlay close            →  c_social_overlay_did_close()→ C freeze_end
 *   JS chat send                →  c_send_ws_message()        → C client_send()
 *   C incoming chat WS msg      →  js_social_overlay_receive_chat() → JS DOM
 *   C dialogue modal close cb   →  js_social_overlay_restore() → JS restores DOM
 */

#ifndef SOCIAL_BRIDGE_H
#define SOCIAL_BRIDGE_H

#include <stdint.h>

/* ── JS functions (implemented in social_overlay.js, called from C) ──── */

/**
 * Open the JS social overlay.
 */
extern void js_social_overlay_open(const char* entity_id,
                                   const char* display_name,
                                   const char* dlg_item_id,
                                   uint32_t interact_flags);

/**
 * Close the JS social overlay.
 */
extern void js_social_overlay_close(void);

/**
 * Check if the JS overlay is currently open.
 * @return 1 if open, 0 if closed.
 */
extern int js_social_overlay_is_open(void);

/**
 * Feed an incoming chat message to the JS overlay.
 */
extern void js_social_overlay_receive_chat(const char* from_id,
                                           const char* from_name,
                                           const char* text);

/**
 * Restore the JS overlay after the C dialogue modal closes.
 */
extern void js_social_overlay_restore(void);

#endif /* SOCIAL_BRIDGE_H */
