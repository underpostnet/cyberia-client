#include "presentation_runtime.h"
#include "network/engine_client.h"
#include "game_state.h"
#include <cJSON.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_PALETTE_ENTRIES     64
#define MAX_STATUS_ENTRIES      32
#define MAX_ENTITY_COLOR_KEYS   16
#define ENTITY_TYPE_MAX         32
#define COLOR_KEY_MAX           32
#define ICON_ID_MAX             64

typedef struct {
    char  key[COLOR_KEY_MAX];
    Color color;
} PaletteEntry;

typedef struct {
    char entity_type[ENTITY_TYPE_MAX];
    char color_key[COLOR_KEY_MAX];
} EntityColorKey;

typedef struct {
    uint8_t id;
    char    icon_id[ICON_ID_MAX];
    bool    icon_id_set;
    Color   border;
    bool    border_set;
} StatusIconEntry;

/* ── Bootstrap fallback ─────────────────────────────────────────────
 *
 * The smallest possible palette so the splash screen can render before
 * the engine fetch settles. Only one shade of grey + a neutral icon
 * border. The real palette comes in from the hints JSON moments later.
 *
 * This is NOT a duplicate source of truth — it is deliberately tiny.
 * Any value the renderer would actually need lives in the fetched
 * response. */
static const Color kBootstrapNeutral = { 100, 100, 100, 200 };

static struct {
    bool             started;
    bool             ready;

    PaletteEntry     palette[MAX_PALETTE_ENTRIES];
    int              palette_count;

    EntityColorKey   entity_keys[MAX_ENTITY_COLOR_KEYS];
    int              entity_key_count;

    StatusIconEntry  status[MAX_STATUS_ENTRIES];
    int              status_count;

    float            cell_size;
    float            camera_zoom;
    float            camera_smoothing;
    int              interpolation_ms;
    float            default_obj_width;
    float            default_obj_height;
    bool             dev_ui;
} g_rt = {
    .cell_size          = 45.0f,
    .camera_zoom        = 1.0f,
    .camera_smoothing   = 0.1f,
    .interpolation_ms   = 100,
    .default_obj_width  = 1.0f,
    .default_obj_height = 1.0f,
    .dev_ui             = false,
};

/* ── JSON parsing helpers ──────────────────────────────────────────── */

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
    if (!root) return;

    cJSON* data = cJSON_GetObjectItem(root, "data");
    if (!data || !cJSON_IsObject(data)) data = root;

    /* Palette */
    cJSON* palette = cJSON_GetObjectItem(data, "palette");
    if (palette && cJSON_IsArray(palette)) {
        g_rt.palette_count = 0;
        cJSON* item = NULL;
        cJSON_ArrayForEach(item, palette) {
            if (g_rt.palette_count >= MAX_PALETTE_ENTRIES) break;
            cJSON* k = cJSON_GetObjectItem(item, "key");
            if (!k || !cJSON_IsString(k)) continue;
            PaletteEntry* p = &g_rt.palette[g_rt.palette_count++];
            strncpy(p->key, k->valuestring, sizeof(p->key) - 1);
            p->key[sizeof(p->key) - 1] = '\0';
            parse_color_object(item, &p->color);
        }
    }

    /* Per-entity-type colour keys */
    cJSON* ekeys = cJSON_GetObjectItem(data, "entityColorKeys");
    if (ekeys && cJSON_IsArray(ekeys)) {
        g_rt.entity_key_count = 0;
        cJSON* item = NULL;
        cJSON_ArrayForEach(item, ekeys) {
            if (g_rt.entity_key_count >= MAX_ENTITY_COLOR_KEYS) break;
            cJSON* t = cJSON_GetObjectItem(item, "entityType");
            cJSON* c = cJSON_GetObjectItem(item, "colorKey");
            if (!t || !c || !cJSON_IsString(t) || !cJSON_IsString(c)) continue;
            EntityColorKey* e = &g_rt.entity_keys[g_rt.entity_key_count++];
            strncpy(e->entity_type, t->valuestring, sizeof(e->entity_type) - 1);
            e->entity_type[sizeof(e->entity_type) - 1] = '\0';
            strncpy(e->color_key, c->valuestring, sizeof(e->color_key) - 1);
            e->color_key[sizeof(e->color_key) - 1] = '\0';
        }
    }

    /* Status icons */
    cJSON* status = cJSON_GetObjectItem(data, "statusIcons");
    if (status && cJSON_IsArray(status)) {
        g_rt.status_count = 0;
        cJSON* item = NULL;
        cJSON_ArrayForEach(item, status) {
            if (g_rt.status_count >= MAX_STATUS_ENTRIES) break;
            cJSON* id = cJSON_GetObjectItem(item, "id");
            if (!id || !cJSON_IsNumber(id)) continue;
            StatusIconEntry* s = &g_rt.status[g_rt.status_count++];
            s->id = (uint8_t)id->valueint;
            cJSON* icon = cJSON_GetObjectItem(item, "iconId");
            if (icon && cJSON_IsString(icon) && icon->valuestring[0] != '\0') {
                strncpy(s->icon_id, icon->valuestring, sizeof(s->icon_id) - 1);
                s->icon_id[sizeof(s->icon_id) - 1] = '\0';
                s->icon_id_set = true;
            }
            cJSON* bc = cJSON_GetObjectItem(item, "borderColor");
            if (parse_color_object(bc, &s->border)) s->border_set = true;
        }
    }

    /* Camera / cell tunings */
    cJSON* n;
    if ((n = cJSON_GetObjectItem(data, "cellSize")) && cJSON_IsNumber(n))           g_rt.cell_size = (float)n->valuedouble;
    if ((n = cJSON_GetObjectItem(data, "cameraZoom")) && cJSON_IsNumber(n))         g_rt.camera_zoom = (float)n->valuedouble;
    if ((n = cJSON_GetObjectItem(data, "cameraSmoothing")) && cJSON_IsNumber(n))    g_rt.camera_smoothing = (float)n->valuedouble;
    if ((n = cJSON_GetObjectItem(data, "interpolationMs")) && cJSON_IsNumber(n))    g_rt.interpolation_ms = n->valueint;
    if ((n = cJSON_GetObjectItem(data, "defaultObjWidth")) && cJSON_IsNumber(n))    g_rt.default_obj_width = (float)n->valuedouble;
    if ((n = cJSON_GetObjectItem(data, "defaultObjHeight")) && cJSON_IsNumber(n))   g_rt.default_obj_height = (float)n->valuedouble;
    if ((n = cJSON_GetObjectItem(data, "devUi")) && cJSON_IsBool(n))                g_rt.dev_ui = cJSON_IsTrue(n);

    cJSON_Delete(root);
    printf("[presentation_runtime] hydrated %d palette / %d entity-keys / %d status-icons; cellSize=%.1f interp=%dms\n",
           g_rt.palette_count, g_rt.entity_key_count, g_rt.status_count,
           g_rt.cell_size, g_rt.interpolation_ms);
}

/* One-shot hydration of g_game_state with presentation values that the
 * renderer and game-state helpers consult directly. Called the frame the
 * fetch settles. */
static void hydrate_game_state(void) {
    g_game_state.cell_size       = g_rt.cell_size;
    g_game_state.interpolation_ms = g_rt.interpolation_ms;
    g_game_state.dev_ui          = g_rt.dev_ui;
    /* Camera zoom: initialise only if untouched (0 by default). User zoom
     * input via mouse wheel mustn't be clobbered if they zoomed early. */
    if (g_game_state.camera.zoom == 0.0f) {
        g_game_state.camera.zoom = g_rt.camera_zoom;
    }
}

static void on_hints_fetched(const FetchResponse* r) {
    if (FETCH_STATE_READY == r->state && r->data && r->size > 0) {
        parse_response((const char*)r->data, (int)r->size);
    } else {
        fprintf(stderr, "[presentation_runtime] fetch unavailable — using bootstrap fallback\n");
    }
    free(r->data);
    g_rt.ready = true;
    hydrate_game_state();
}

/* ── Public lifecycle ──────────────────────────────────────────────── */

void presentation_runtime_start_fetch(const char* api_base_url, const char* client_hints_code) {
    if (g_rt.started) return;
    if (!api_base_url || !client_hints_code) return;
    char url[512];
    int n = snprintf(url, sizeof(url), "%s/api/cyberia-client-hints/%s", api_base_url, client_hints_code);
    if (n <= 0 || n >= (int)sizeof(url)) return;
    g_rt.started = true;
    fetch_request_start("cyberia-client-hints", url, on_hints_fetched);
    printf("[presentation_runtime] fetching %s\n", url);
}

bool presentation_runtime_is_ready(void) {
    return g_rt.ready;
}

/* ── Accessors ─────────────────────────────────────────────────────── */

Color presentation_runtime_palette(const char* key) {
    if (key && key[0] != '\0') {
        for (int i = 0; i < g_rt.palette_count; i++) {
            if (strcmp(g_rt.palette[i].key, key) == 0) return g_rt.palette[i].color;
        }
    }
    return kBootstrapNeutral;
}

Color presentation_runtime_entity_fallback_color(const char* entity_type) {
    if (entity_type) {
        for (int i = 0; i < g_rt.entity_key_count; i++) {
            if (strcmp(g_rt.entity_keys[i].entity_type, entity_type) == 0) {
                return presentation_runtime_palette(g_rt.entity_keys[i].color_key);
            }
        }
    }
    return kBootstrapNeutral;
}

const char* presentation_runtime_status_icon(uint8_t status_id) {
    for (int i = 0; i < g_rt.status_count; i++) {
        if (g_rt.status[i].id == status_id && g_rt.status[i].icon_id_set) {
            return g_rt.status[i].icon_id;
        }
    }
    return NULL;
}

Color presentation_runtime_status_border(uint8_t status_id) {
    for (int i = 0; i < g_rt.status_count; i++) {
        if (g_rt.status[i].id == status_id && g_rt.status[i].border_set) {
            return g_rt.status[i].border;
        }
    }
    return kBootstrapNeutral;
}

float presentation_runtime_cell_size(void)         { return g_rt.cell_size; }
float presentation_runtime_camera_zoom(void)       { return g_rt.camera_zoom; }
float presentation_runtime_camera_smoothing(void)  { return g_rt.camera_smoothing; }
int   presentation_runtime_interpolation_ms(void)  { return g_rt.interpolation_ms; }
float presentation_runtime_default_obj_width(void) { return g_rt.default_obj_width; }
float presentation_runtime_default_obj_height(void){ return g_rt.default_obj_height; }
bool  presentation_runtime_dev_ui(void)            { return g_rt.dev_ui; }
