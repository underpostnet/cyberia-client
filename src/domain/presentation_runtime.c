#include "presentation_runtime.h"
#include "presentation_defaults.h"
#include "../js/services.h"

#include <cJSON.h>
#include <raylib.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Reserve a request_id range for this module so it never collides with
 * other Channel-3 fetches (atlas, object-layer, dialogue, ui-icon). */
#define PRESENTATION_REQUEST_ID 0x40000001

/* Maximum overrides we keep in memory. Generous — even a hand-authored
 * per-instance hint document is unlikely to override more than a couple
 * of dozen entries. */
#define MAX_PALETTE_OVERRIDES 64
#define MAX_STATUS_OVERRIDES  32

typedef struct {
    char  key[32];
    Color color;
} PaletteOverride;

typedef struct {
    unsigned char id;
    char          icon_id[64];
    Color         border;
    bool          icon_id_set;
    bool          border_set;
} StatusIconOverride;

static struct {
    bool                started;
    bool                ready;
    bool                just_became_ready;
    PaletteOverride     palette[MAX_PALETTE_OVERRIDES];
    int                 palette_count;
    StatusIconOverride  status[MAX_STATUS_OVERRIDES];
    int                 status_count;
} g_rt = {0};

/* Parse a {r,g,b,a} object into a raylib Color. Missing fields default
 * to opaque. Numeric-coerce per cJSON's IsNumber semantics. */
static bool parse_color_object(const cJSON* obj, Color* out) {
    if (!obj || !cJSON_IsObject(obj) || !out) return false;
    cJSON* r = cJSON_GetObjectItem(obj, "r");
    cJSON* g = cJSON_GetObjectItem(obj, "g");
    cJSON* b = cJSON_GetObjectItem(obj, "b");
    cJSON* a = cJSON_GetObjectItem(obj, "a");
    out->r = (unsigned char)(r && cJSON_IsNumber(r) ? r->valueint : 0);
    out->g = (unsigned char)(g && cJSON_IsNumber(g) ? g->valueint : 0);
    out->b = (unsigned char)(b && cJSON_IsNumber(b) ? b->valueint : 0);
    out->a = (unsigned char)(a && cJSON_IsNumber(a) ? a->valueint : 255);
    return true;
}

static void parse_response(const char* body, int len) {
    cJSON* root = cJSON_ParseWithLength(body, (size_t)len);
    if (!root) {
        /* Parse failure is non-fatal — the client keeps the built-in
         * compile-time defaults. We deliberately do not write to stderr
         * because the browser surfaces stderr as error logs. */
        return;
    }
    /* Wire shape: { "status": "success", "data": { ... } }. The hints
     * object may also appear directly at the root. */
    cJSON* data = cJSON_GetObjectItem(root, "data");
    if (!data || !cJSON_IsObject(data)) {
        data = root;
    }

    /* Palette overrides. */
    cJSON* palette = cJSON_GetObjectItem(data, "palette");
    if (palette && cJSON_IsArray(palette)) {
        cJSON* item = NULL;
        cJSON_ArrayForEach(item, palette) {
            if (g_rt.palette_count >= MAX_PALETTE_OVERRIDES) break;
            cJSON* k = cJSON_GetObjectItem(item, "key");
            if (!k || !cJSON_IsString(k)) continue;
            PaletteOverride* p = &g_rt.palette[g_rt.palette_count++];
            strncpy(p->key, k->valuestring, sizeof(p->key) - 1);
            p->key[sizeof(p->key) - 1] = '\0';
            cJSON* r = cJSON_GetObjectItem(item, "r");
            cJSON* g = cJSON_GetObjectItem(item, "g");
            cJSON* b = cJSON_GetObjectItem(item, "b");
            cJSON* a = cJSON_GetObjectItem(item, "a");
            p->color.r = (unsigned char)(r && cJSON_IsNumber(r) ? r->valueint : 0);
            p->color.g = (unsigned char)(g && cJSON_IsNumber(g) ? g->valueint : 0);
            p->color.b = (unsigned char)(b && cJSON_IsNumber(b) ? b->valueint : 0);
            p->color.a = (unsigned char)(a && cJSON_IsNumber(a) ? a->valueint : 255);
        }
    }

    /* Status-icon overrides. */
    cJSON* status = cJSON_GetObjectItem(data, "statusIcons");
    if (status && cJSON_IsArray(status)) {
        cJSON* item = NULL;
        cJSON_ArrayForEach(item, status) {
            if (g_rt.status_count >= MAX_STATUS_OVERRIDES) break;
            cJSON* id = cJSON_GetObjectItem(item, "id");
            if (!id || !cJSON_IsNumber(id)) continue;
            StatusIconOverride* s = &g_rt.status[g_rt.status_count++];
            s->id = (unsigned char)id->valueint;
            cJSON* icon = cJSON_GetObjectItem(item, "iconId");
            if (icon && cJSON_IsString(icon)) {
                strncpy(s->icon_id, icon->valuestring, sizeof(s->icon_id) - 1);
                s->icon_id[sizeof(s->icon_id) - 1] = '\0';
                s->icon_id_set = true;
            }
            cJSON* bc = cJSON_GetObjectItem(item, "borderColor");
            if (parse_color_object(bc, &s->border)) {
                s->border_set = true;
            }
        }
    }

    cJSON_Delete(root);
    printf("[presentation_runtime] applied %d palette + %d status overrides\n",
           g_rt.palette_count, g_rt.status_count);
}

void presentation_runtime_start_fetch(const char* api_base_url, const char* instance_code) {
    if (g_rt.started) return;
    if (!api_base_url || !instance_code) return;
    char url[512];
    int n = snprintf(url, sizeof(url), "%s/api/cyberia-client-hints/%s", api_base_url, instance_code);
    if (n <= 0 || n >= (int)sizeof(url)) return;
    g_rt.started = true;
    js_start_fetch_binary(url, PRESENTATION_REQUEST_ID);
    printf("[presentation_runtime] fetching %s\n", url);
}

bool presentation_runtime_poll(void) {
    if (!g_rt.started || g_rt.ready) return false;
    int size = 0;
    unsigned char* buf = js_get_fetch_result(PRESENTATION_REQUEST_ID, &size);
    if (buf && size > 0) {
        parse_response((const char*)buf, size);
        free(buf);
        g_rt.ready = true;
        g_rt.just_became_ready = true;
        return true;
    }
    if (!buf && size < 0) {
        /* Error or 404 — fall through to the compile-time defaults. The
         * fetch is optional, so this is a normal outcome on fresh
         * environments. Logged at info-level via stdout. */
        g_rt.ready = true;
        g_rt.just_became_ready = true;
        printf("[presentation_runtime] fetch unavailable; using built-in defaults\n");
        return true;
    }
    return false;
}

bool presentation_runtime_is_ready(void) {
    return g_rt.ready;
}

Color presentation_runtime_palette(const char* key) {
    if (g_rt.ready && key) {
        for (int i = 0; i < g_rt.palette_count; i++) {
            if (strcmp(g_rt.palette[i].key, key) == 0) {
                return g_rt.palette[i].color;
            }
        }
    }
    return presentation_palette_lookup(key);
}

const char* presentation_runtime_status_icon(unsigned char status_id) {
    if (g_rt.ready) {
        for (int i = 0; i < g_rt.status_count; i++) {
            if (g_rt.status[i].id == status_id && g_rt.status[i].icon_id_set) {
                return g_rt.status[i].icon_id;
            }
        }
    }
    return presentation_status_icon_id(status_id);
}

Color presentation_runtime_status_border(unsigned char status_id) {
    if (g_rt.ready) {
        for (int i = 0; i < g_rt.status_count; i++) {
            if (g_rt.status[i].id == status_id && g_rt.status[i].border_set) {
                return g_rt.status[i].border;
            }
        }
    }
    return presentation_status_icon_border(status_id);
}
