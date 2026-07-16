#include "instance_map_data.h"

#include "game_state.h"
#include "network/engine_client.h"
#include "util/log.h"

#include <cJSON.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define IMAP_POLL_INTERVAL_S 1.0f

static ImapGraph     s_graph;
static ImapDataState s_state       = IMAP_DATA_IDLE;
static bool          s_open        = false;
static int           s_generation  = 0;
static float         s_poll_timer  = 0.0f;
static bool          s_poll_inflight = false;

/* Session guard: bumped on every open/close so responses that complete after
 * a close (or across a reopen) are recognised as stale and dropped. */
static int s_session = 0;

static void copy_str(char* dst, size_t cap, const char* src) {
    if (!src) { dst[0] = '\0'; return; }
    strncpy(dst, src, cap - 1);
    dst[cap - 1] = '\0';
}

static const char* json_str(const cJSON* obj, const char* key) {
    const cJSON* v = cJSON_GetObjectItemCaseSensitive(obj, key);
    return cJSON_IsString(v) ? v->valuestring : NULL;
}

static int json_int(const cJSON* obj, const char* key, int fallback) {
    const cJSON* v = cJSON_GetObjectItemCaseSensitive(obj, key);
    return cJSON_IsNumber(v) ? v->valueint : fallback;
}

/* Returns the envelope's `data` object on success, else NULL. */
static const cJSON* envelope_success_doc(const cJSON* root) {
    if (!root) return NULL;
    const cJSON* status = cJSON_GetObjectItemCaseSensitive(root, "status");
    const cJSON* doc    = cJSON_GetObjectItemCaseSensitive(root, "data");
    if (!cJSON_IsString(status) || 0 != strcmp(status->valuestring, "success") ||
        !cJSON_IsObject(doc)) {
        return NULL;
    }
    return doc;
}

int instance_map_data_find_node(const char* map_code) {
    if (!map_code) return -1;
    for (int i = 0; i < s_graph.node_count; ++i) {
        if (0 == strcmp(s_graph.nodes[i].map_code, map_code)) return i;
    }
    return -1;
}

/* ── Deterministic force-directed layout ─────────────────────────────────
 * Small graphs (a handful of maps): seed nodes on a circle in declaration
 * order, then relax with pairwise repulsion + edge springs. No randomness,
 * so the same instance always lays out the same way. Output spans roughly
 * [-1, 1] on both axes (normalised at the end). */
static void layout_graph(void) {
    int n = s_graph.node_count;
    if (0 == n) return;

    for (int i = 0; i < n; ++i) {
        float a = (float)i / (float)n * 2.0f * PI - PI * 0.5f;
        s_graph.nodes[i].pos = (Vector2){ cosf(a), sinf(a) };
    }
    if (1 == n) { s_graph.nodes[0].pos = (Vector2){ 0, 0 }; return; }

    const float k = 1.6f / sqrtf((float)n);   /* ideal edge length */
    for (int iter = 0; iter < 220; ++iter) {
        Vector2 disp[IMAP_MAX_NODES] = {0};
        for (int i = 0; i < n; ++i) {
            for (int j = i + 1; j < n; ++j) {
                float dx = s_graph.nodes[i].pos.x - s_graph.nodes[j].pos.x;
                float dy = s_graph.nodes[i].pos.y - s_graph.nodes[j].pos.y;
                float d2 = dx * dx + dy * dy + 0.0001f;
                float f  = (k * k) / d2;      /* repulsion */
                disp[i].x += dx * f; disp[i].y += dy * f;
                disp[j].x -= dx * f; disp[j].y -= dy * f;
            }
        }
        for (int e = 0; e < s_graph.edge_count; ++e) {
            int a = s_graph.edges[e].source_node;
            int b = s_graph.edges[e].target_node;
            if (a == b) continue;
            float dx = s_graph.nodes[a].pos.x - s_graph.nodes[b].pos.x;
            float dy = s_graph.nodes[a].pos.y - s_graph.nodes[b].pos.y;
            float d  = sqrtf(dx * dx + dy * dy) + 0.0001f;
            float f  = (d * d) / k * 0.5f;    /* attraction */
            disp[a].x -= dx / d * f * 0.02f; disp[a].y -= dy / d * f * 0.02f;
            disp[b].x += dx / d * f * 0.02f; disp[b].y += dy / d * f * 0.02f;
        }
        float step = 0.08f * (1.0f - (float)iter / 220.0f);
        for (int i = 0; i < n; ++i) {
            float d = sqrtf(disp[i].x * disp[i].x + disp[i].y * disp[i].y);
            if (d > 0.0001f) {
                float clamp = d < step ? d : step;
                s_graph.nodes[i].pos.x += disp[i].x / d * clamp;
                s_graph.nodes[i].pos.y += disp[i].y / d * clamp;
            }
        }
    }

    /* Normalise to [-1, 1] around the centroid. */
    float cx = 0, cy = 0;
    for (int i = 0; i < n; ++i) { cx += s_graph.nodes[i].pos.x; cy += s_graph.nodes[i].pos.y; }
    cx /= (float)n; cy /= (float)n;
    float max_r = 0.0001f;
    for (int i = 0; i < n; ++i) {
        s_graph.nodes[i].pos.x -= cx;
        s_graph.nodes[i].pos.y -= cy;
        float r = fmaxf(fabsf(s_graph.nodes[i].pos.x), fabsf(s_graph.nodes[i].pos.y));
        if (r > max_r) max_r = r;
    }
    for (int i = 0; i < n; ++i) {
        s_graph.nodes[i].pos.x /= max_r;
        s_graph.nodes[i].pos.y /= max_r;
    }
}

/* ── Static payload ─────────────────────────────────────────────────────── */

static void parse_providers(const cJSON* node_doc, const char* key, int node_idx,
                            ImapProvider* out, int* count) {
    const cJSON* arr = cJSON_GetObjectItemCaseSensitive(node_doc, key);
    if (!cJSON_IsArray(arr)) return;
    const cJSON* p = NULL;
    cJSON_ArrayForEach(p, arr) {
        if (*count >= IMAP_MAX_PROVIDERS) return;
        ImapProvider* pr = &out[(*count)++];
        memset(pr, 0, sizeof(*pr));
        const char* code  = json_str(p, "questCode");
        if (!code) code   = json_str(p, "actionCode");
        const char* label = json_str(p, "title");
        if (!label) label = json_str(p, "label");
        copy_str(pr->code, IMAP_CODE_MAX, code);
        copy_str(pr->label, IMAP_NAME_MAX, label ? label : code);
        pr->node   = node_idx;
        pr->cell_x = json_int(p, "cellX", 0);
        pr->cell_y = json_int(p, "cellY", 0);
    }
}

/* Register an edge endpoint as a portal landmark; negative cells (random
 * destinations) carry no fixed position and are skipped. Duplicate cells on
 * the same node collapse into one POI. */
static void add_portal_poi(int node, int cell_x, int cell_y, bool intra) {
    if (node < 0 || cell_x < 0 || cell_y < 0) return;
    if (s_graph.portal_poi_count >= IMAP_MAX_PORTAL_POIS) return;
    for (int i = 0; i < s_graph.portal_poi_count; ++i) {
        const ImapPortalPoi* p = &s_graph.portal_pois[i];
        if (p->node == node && p->cell_x == cell_x && p->cell_y == cell_y) return;
    }
    s_graph.portal_pois[s_graph.portal_poi_count++] =
        (ImapPortalPoi){ .node = node, .cell_x = cell_x, .cell_y = cell_y, .intra = intra };
}

static bool parse_static_doc(const cJSON* doc) {
    memset(&s_graph, 0, sizeof(s_graph));
    copy_str(s_graph.instance_code, IMAP_CODE_MAX, json_str(doc, "instanceCode"));
    copy_str(s_graph.name, IMAP_NAME_MAX, json_str(doc, "name"));

    const cJSON* nodes = cJSON_GetObjectItemCaseSensitive(doc, "nodes");
    if (!cJSON_IsArray(nodes)) return false;
    const cJSON* nd = NULL;
    cJSON_ArrayForEach(nd, nodes) {
        if (s_graph.node_count >= IMAP_MAX_NODES) break;
        int idx = s_graph.node_count;
        ImapNode* node = &s_graph.nodes[s_graph.node_count++];
        memset(node, 0, sizeof(*node));
        copy_str(node->map_code, IMAP_CODE_MAX, json_str(nd, "mapCode"));
        const char* name = json_str(nd, "name");
        copy_str(node->name, IMAP_NAME_MAX, name ? name : node->map_code);
        copy_str(node->preview_file_id, IMAP_CODE_MAX, json_str(nd, "preview"));
        node->grid_x = json_int(nd, "gridX", 16);
        node->grid_y = json_int(nd, "gridY", 16);

        parse_providers(nd, "questProviders", idx,
                        s_graph.quest_providers, &s_graph.quest_provider_count);
        parse_providers(nd, "actionProviders", idx,
                        s_graph.action_providers, &s_graph.action_provider_count);
    }
    for (int i = 0; i < s_graph.quest_provider_count; ++i)
        s_graph.nodes[s_graph.quest_providers[i].node].quest_provider_count++;
    for (int i = 0; i < s_graph.action_provider_count; ++i)
        s_graph.nodes[s_graph.action_providers[i].node].action_provider_count++;

    const cJSON* edges = cJSON_GetObjectItemCaseSensitive(doc, "edges");
    if (cJSON_IsArray(edges)) {
        const cJSON* ed = NULL;
        cJSON_ArrayForEach(ed, edges) {
            if (s_graph.edge_count >= IMAP_MAX_EDGES) break;
            int src = instance_map_data_find_node(json_str(ed, "sourceMapCode"));
            int tgt = instance_map_data_find_node(json_str(ed, "targetMapCode"));
            if (src < 0 || tgt < 0) continue;
            ImapEdge* e = &s_graph.edges[s_graph.edge_count++];
            memset(e, 0, sizeof(*e));
            e->source_node   = src;
            e->target_node   = tgt;
            e->intra         = (src == tgt);
            copy_str(e->portal_mode, sizeof(e->portal_mode), json_str(ed, "portalMode"));
            e->source_cell_x = json_int(ed, "sourceCellX", -1);
            e->source_cell_y = json_int(ed, "sourceCellY", -1);
            e->target_cell_x = json_int(ed, "targetCellX", -1);
            e->target_cell_y = json_int(ed, "targetCellY", -1);
            s_graph.nodes[src].portal_count++;
            if (tgt != src) s_graph.nodes[tgt].portal_count++;

            /* Portal landmarks: each edge endpoint with a known init-spawn
             * cell becomes a POI on its node's map. */
            add_portal_poi(src, e->source_cell_x, e->source_cell_y, e->intra);
            add_portal_poi(tgt, e->target_cell_x, e->target_cell_y, e->intra);
        }
    }

    layout_graph();
    return s_graph.node_count > 0;
}

static void on_static_fetched(const FetchResponse* r) {
    /* asset_id carries the session stamp — drop stale/closed sessions. */
    if (!s_open || atoi(r->asset_id + strlen("imap-static-")) != s_session) {
        free(r->data);
        return;
    }
    if (!r->success) {
        s_state = IMAP_DATA_ERROR;
        LOG_WARN("instance map static fetch failed");
        free(r->data);
        return;
    }
    cJSON* root = cJSON_ParseWithLength((const char*)r->data, r->size);
    const cJSON* doc = envelope_success_doc(root);
    if (doc && parse_static_doc(doc)) {
        s_state = IMAP_DATA_READY;
        s_generation++;
        s_poll_timer = IMAP_POLL_INTERVAL_S; /* first dynamic poll immediately */
    } else {
        s_state = IMAP_DATA_ERROR;
        LOG_WARN("instance map static parse failed");
    }
    cJSON_Delete(root);
    free(r->data);
}

/* ── Dynamic payload ────────────────────────────────────────────────────── */

static void apply_dynamic_providers(const cJSON* doc, const char* key,
                                    ImapProvider* list, int count, bool quest) {
    for (int i = 0; i < count; ++i) { list[i].active = false; list[i].acceptable = false; }
    const cJSON* arr = cJSON_GetObjectItemCaseSensitive(doc, key);
    if (!cJSON_IsArray(arr)) return;
    const cJSON* p = NULL;
    cJSON_ArrayForEach(p, arr) {
        const char* code = json_str(p, quest ? "questCode" : "actionCode");
        if (!code) continue;
        for (int i = 0; i < count; ++i) {
            if (0 != strcmp(list[i].code, code)) continue;
            list[i].active = true;
            if (quest) {
                const char* state = json_str(p, "state");
                list[i].acceptable = state && 0 == strcmp(state, "acceptable");
            }
            break;
        }
    }
}

static void on_dynamic_fetched(const FetchResponse* r) {
    if (!s_open || atoi(r->asset_id + strlen("imap-dyn-")) != s_session) {
        s_poll_inflight = false;
        free(r->data);
        return;
    }
    s_poll_inflight = false;
    if (!r->success) { free(r->data); return; }

    cJSON* root = cJSON_ParseWithLength((const char*)r->data, r->size);
    const cJSON* doc = envelope_success_doc(root);
    if (doc && IMAP_DATA_READY == s_state) {
        apply_dynamic_providers(doc, "questProviders",
                                s_graph.quest_providers, s_graph.quest_provider_count, true);
        apply_dynamic_providers(doc, "actionProviders",
                                s_graph.action_providers, s_graph.action_provider_count, false);
    }
    cJSON_Delete(root);
    free(r->data);
}

static void start_dynamic_poll(void) {
    char asset_id[48];
    snprintf(asset_id, sizeof(asset_id), "imap-dyn-%d", s_session);
    char url[512];
    snprintf(url, sizeof(url), "/api/cyberia-instance/instance-map/%s/dynamic?playerId=%s",
             g_game_state.instance_code, g_game_state.player_id);
    s_poll_inflight = true;
    fetch_request_start(asset_id, url, on_dynamic_fetched);
}

/* ── Lifecycle ──────────────────────────────────────────────────────────── */

void instance_map_data_open(void) {
    s_session++;
    s_open       = true;
    s_poll_timer = 0.0f;
    s_poll_inflight = false;

    if ('\0' == g_game_state.instance_code[0]) {
        s_state = IMAP_DATA_ERROR;
        LOG_WARN("instance map open without instance code (metadata not received)");
        return;
    }
    s_state = IMAP_DATA_LOADING;

    char asset_id[48];
    snprintf(asset_id, sizeof(asset_id), "imap-static-%d", s_session);
    char url[256];
    snprintf(url, sizeof(url), "/api/cyberia-instance/instance-map/%s/static",
             g_game_state.instance_code);
    fetch_request_start(asset_id, url, on_static_fetched);
}

void instance_map_data_close(void) {
    s_session++;          /* invalidates every in-flight response */
    s_open        = false;
    s_state       = IMAP_DATA_IDLE;
    s_poll_inflight = false;
}

void instance_map_data_update(float dt) {
    if (!s_open || IMAP_DATA_READY != s_state) return;
    s_poll_timer += dt;
    if (s_poll_timer >= IMAP_POLL_INTERVAL_S && !s_poll_inflight) {
        s_poll_timer = 0.0f;
        start_dynamic_poll();
    }
}

ImapDataState    instance_map_data_state(void) { return s_state; }
const ImapGraph* instance_map_data_graph(void) { return &s_graph; }
int              instance_map_data_generation(void) { return s_generation; }

int instance_map_data_node_active_quests(int node) {
    int n = 0;
    for (int i = 0; i < s_graph.quest_provider_count; ++i)
        if (s_graph.quest_providers[i].node == node && s_graph.quest_providers[i].active) n++;
    return n;
}

int instance_map_data_node_active_actions(int node) {
    int n = 0;
    for (int i = 0; i < s_graph.action_provider_count; ++i)
        if (s_graph.action_providers[i].node == node && s_graph.action_providers[i].active) n++;
    return n;
}
