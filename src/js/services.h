#ifndef JS_SERVICES_H
#define JS_SERVICES_H

#include <stdint.h>

/* ── services.js — HTTP fetch bridge ──────────────────────────────────── */

void js_init_engine_api(const char* api_base_url);
void js_start_fetch_binary(const char* url, int request_id);
unsigned char* js_get_fetch_result(int request_id, int* size);

#endif /* JS_SERVICES_H */