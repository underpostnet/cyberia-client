#ifndef JS_SERVICES_H
#define JS_SERVICES_H

#include <stdint.h>

/* ── services.js — HTTP fetch bridge ──────────────────────────────────── */

void js_init_engine_api(const char* api_base_url);
void js_start_fetch_binary(const char* url, int request_id);
unsigned char* js_get_fetch_result(int request_id, int* size);

/* ── social_overlay.js — social overlay bridge ────────────────────────── */

void js_social_overlay_open(const char* entity_id, const char* display_name, const char* dlg_item_id, uint32_t interact_flags);
void js_social_overlay_close(void);
int js_social_overlay_is_open(void);
void js_social_overlay_receive_chat(const char* from_id, const char* from_name, const char* text);
void js_social_overlay_restore(void);

#endif /* JS_SERVICES_H */