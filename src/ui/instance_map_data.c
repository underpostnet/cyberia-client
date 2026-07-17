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

static bool json_bool(const cJSON* obj, const char* key, bool fallback) {
    const cJSON* v = cJSON_GetObjectItemCaseSensitive(obj, key);
    return cJSON_IsBool(v) ? cJSON_IsTrue(v) : fallback;
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

/* ── Deterministic packed grid layout ───────────────────────────────────── */
static void layout_graph(void) {
    int n = s_graph.node_count;
    if (0 == n) return;

    int cols = (int)ceilf(sqrtf((float)n));
    int rows = (n + cols - 1) / cols;
    s_graph.grid_cols = cols;
    s_graph.grid_rows = rows;
    for (int i = 0; i < n; ++i) {
        s_graph.nodes[i].grid_col = i % cols;
        s_graph.nodes[i].grid_row = i / cols;
    }
}

/* ── Static payload ─────────────────────────────────────────────────────── */

static ImapPresenceStatus presence_status_from_string(const char* value) {
    if (NULL == value) return IMAP_PRESENCE_NONE;
    if (0 == strcmp(value, "passive"))       return IMAP_PRESENCE_PASSIVE;
    if (0 == strcmp(value, "hostile"))       return IMAP_PRESENCE_HOSTILE;
    if (0 == strcmp(value, "resource"))      return IMAP_PRESENCE_RESOURCE;
    if (0 == strcmp(value, "portal"))        return IMAP_PRESENCE_PORTAL;
    if (0 == strcmp(value, "portal-random")) return IMAP_PRESENCE_PORTAL_RANDOM;
    return IMAP_PRESENCE_NONE;
}

static ImapPresencePoi* find_presence_poi(int node, int cell_x, int cell_y) {
    for (int i = 0; i < s_graph.presence_poi_count; ++i) {
        ImapPresencePoi* poi = &s_graph.presence_pois[i];
        if (poi->node == node && poi->cell_x == cell_x && poi->cell_y == cell_y)
            return poi;
    }
    return NULL;
}

static uint8_t parse_capabilities(const cJSON* poi_doc) {
    uint8_t capabilities = 0;
    const cJSON* values = cJSON_GetObjectItemCaseSensitive(poi_doc, "capabilities");
    if (!cJSON_IsArray(values)) return capabilities;

    const cJSON* value = NULL;
    cJSON_ArrayForEach(value, values) {
        if (!cJSON_IsString(value)) continue;
        if (0 == strcmp(value->valuestring, "action"))
            capabilities |= IMAP_CAPABILITY_ACTION;
        else if (0 == strcmp(value->valuestring, "quest"))
            capabilities |= IMAP_CAPABILITY_QUEST;
    }
    return capabilities;
}

static void refresh_node_capability_counts(void) {
    for (int i = 0; i < s_graph.node_count; ++i) {
        s_graph.nodes[i].quest_provider_count = 0;
        s_graph.nodes[i].action_provider_count = 0;
    }
    for (int i = 0; i < s_graph.presence_poi_count; ++i) {
        const ImapPresencePoi* poi = &s_graph.presence_pois[i];
        if (poi->capabilities & IMAP_CAPABILITY_QUEST)
            s_graph.nodes[poi->node].quest_provider_count++;
        if (poi->capabilities & IMAP_CAPABILITY_ACTION)
            s_graph.nodes[poi->node].action_provider_count++;
    }
}

static void parse_presence_pois(const cJSON* doc) {
    const cJSON* pois = cJSON_GetObjectItemCaseSensitive(doc, "presencePois");
    if (!cJSON_IsArray(pois)) return;

    const cJSON* poi_doc = NULL;
    cJSON_ArrayForEach(poi_doc, pois) {
        int node = instance_map_data_find_node(json_str(poi_doc, "mapCode"));
        int cell_x = json_int(poi_doc, "cellX", -1);
        int cell_y = json_int(poi_doc, "cellY", -1);
        ImapPresenceStatus presence = presence_status_from_string(json_str(poi_doc, "presenceStatus"));
        if (node < 0 || cell_x < 0 || cell_y < 0 || IMAP_PRESENCE_NONE == presence)
            continue;

        ImapPresencePoi* poi = find_presence_poi(node, cell_x, cell_y);
        if (NULL == poi) {
            if (s_graph.presence_poi_count >= IMAP_MAX_PRESENCE_POIS) break;
            poi = &s_graph.presence_pois[s_graph.presence_poi_count++];
            *poi = (ImapPresencePoi){ .node = node, .cell_x = cell_x, .cell_y = cell_y };
        }
        poi->presence_status = presence;
        poi->stats_sum = json_int(poi_doc, "statsSum", 0);
        poi->show_stats_value = json_bool(poi_doc, "showStatsValue", false);
        poi->capabilities |= parse_capabilities(poi_doc);
    }
    refresh_node_capability_counts();
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
        ImapNode* node = &s_graph.nodes[s_graph.node_count++];
        memset(node, 0, sizeof(*node));
        copy_str(node->map_code, IMAP_CODE_MAX, json_str(nd, "mapCode"));
        const char* name = json_str(nd, "name");
        copy_str(node->name, IMAP_NAME_MAX, name ? name : node->map_code);
        copy_str(node->preview_file_id, IMAP_CODE_MAX, json_str(nd, "preview"));
        node->grid_x = json_int(nd, "gridX", 16);
        node->grid_y = json_int(nd, "gridY", 16);
    }
    parse_presence_pois(doc);

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

static void clear_dynamic_capabilities(void) {
    for (int i = 0; i < s_graph.presence_poi_count; ++i) {
        s_graph.presence_pois[i].action_active = false;
        s_graph.presence_pois[i].quest_active = false;
        s_graph.presence_pois[i].quest_acceptable = false;
    }
}

static void apply_dynamic_capabilities(const cJSON* doc, const char* key, bool quest) {
    const cJSON* arr = cJSON_GetObjectItemCaseSensitive(doc, key);
    if (!cJSON_IsArray(arr)) return;
    const cJSON* p = NULL;
    cJSON_ArrayForEach(p, arr) {
        int node = instance_map_data_find_node(json_str(p, "mapCode"));
        int cell_x = json_int(p, "cellX", -1);
        int cell_y = json_int(p, "cellY", -1);
        ImapPresencePoi* poi = find_presence_poi(node, cell_x, cell_y);
        if (NULL == poi) continue;
        if (quest && (poi->capabilities & IMAP_CAPABILITY_QUEST)) {
            poi->quest_active = true;
            const char* state = json_str(p, "state");
            poi->quest_acceptable = state && 0 == strcmp(state, "acceptable");
        } else if (!quest && (poi->capabilities & IMAP_CAPABILITY_ACTION)) {
            poi->action_active = true;
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
        clear_dynamic_capabilities();
        apply_dynamic_capabilities(doc, "questProviders", true);
        apply_dynamic_capabilities(doc, "actionProviders", false);
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
    for (int i = 0; i < s_graph.presence_poi_count; ++i)
        if (s_graph.presence_pois[i].node == node && s_graph.presence_pois[i].quest_active) n++;
    return n;
}

int instance_map_data_node_active_actions(int node) {
    int n = 0;
    for (int i = 0; i < s_graph.presence_poi_count; ++i)
        if (s_graph.presence_pois[i].node == node && s_graph.presence_pois[i].action_active) n++;
    return n;
}
