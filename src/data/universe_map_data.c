#include "universe_map_data.h"

#include "engine_client.h"
#include "log.h"
#include "sim/game_state.h"
#include "utils.h"

#include <cJSON.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static UmapGraph     s_graph;
static ImapDataState s_state      = IMAP_DATA_IDLE;
static bool          s_open       = false;
static bool          s_in_flight  = false;
static int           s_generation = 0;
static float         s_since_fetch = 0.0f;
static float         s_age;
static bool          s_stale = true;
static char          s_instance[IMAP_CODE_MAX];

/* Bumped on every open and close, so a response of an older session is dropped. */
static int s_session = 0;

#define UMAP_ASSET_PREFIX "umap-"

static int find_node(const UmapGraph* graph, const char* code) {
    for (int i = 0; NULL != code && i < graph->node_count; ++i)
        if (0 == strcmp(graph->nodes[i].instance_code, code)) return i;
    return -1;
}

static UmapStatus status_from_string(const char* value) {
    if (NULL != value && 0 == strcmp(value, "available")) return UMAP_AVAILABLE;
    if (NULL != value && 0 == strcmp(value, "draining")) return UMAP_DRAINING;
    return UMAP_OFFLINE;
}

static bool parse_doc(const cJSON* doc, UmapGraph* graph) {
    if (!cJSON_IsArray(cJSON_GetObjectItemCaseSensitive(doc, "instances"))) return false;
    memset(graph, 0, sizeof(*graph));
    copy_str(graph->code, IMAP_CODE_MAX, json_str(doc, "code"));
    copy_str(graph->name, IMAP_NAME_MAX, json_str(doc, "name"));
    graph->current = -1;

    const cJSON* member = NULL;
    cJSON_ArrayForEach(member, cJSON_GetObjectItemCaseSensitive(doc, "instances")) {
        if (UMAP_MAX_NODES <= graph->node_count) break;
        UmapNode* n = &graph->nodes[graph->node_count];
        copy_str(n->instance_code, IMAP_CODE_MAX, json_str(member, "instanceCode"));
        if ('\0' == n->instance_code[0]) continue;
        const char* name = json_str(member, "name");
        copy_str(n->name, IMAP_NAME_MAX, name ? name : n->instance_code);
        copy_str(n->description, sizeof n->description, json_str(member, "description"));
        n->map_count = cJSON_GetArraySize(cJSON_GetObjectItemCaseSensitive(member, "mapCodes"));
        copy_str(n->preview_url, IMAP_URL_MAX, json_str(member, "previewUrl"));
        copy_str(n->thumbnail_url, IMAP_URL_MAX, json_str(member, "thumbnailUrl"));
        copy_str(n->url, IMAP_URL_MAX, json_str(member, "url"));
        copy_str(n->path, IMAP_CODE_MAX, json_str(member, "path"));
        copy_str(n->build, IMAP_CODE_MAX, json_str(member, "build"));
        n->layout_x = json_int(member, "x", 0);
        n->layout_y = json_int(member, "y", 0);
        n->extent_w = json_float(member, "w", 1.0f);
        n->extent_h = json_float(member, "h", 1.0f);
        n->served   = cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(member, "served"));
        n->status   = status_from_string(json_str(member, "status"));
        if (0 == strcmp(n->instance_code, g_game_state.instance_code)) graph->current = graph->node_count;
        graph->node_count++;
    }

    const cJSON* link = NULL;
    cJSON_ArrayForEach(link, cJSON_GetObjectItemCaseSensitive(doc, "links")) {
        if (UMAP_MAX_LINKS <= graph->link_count) break;
        int source = find_node(graph, json_str(link, "sourceInstanceCode"));
        int target = find_node(graph, json_str(link, "targetInstanceCode"));
        if (0 > source || 0 > target || source == target) continue;
        graph->links[graph->link_count++] = (UmapLink){
            .source = source,
            .target = target,
            .bidirectional = !cJSON_IsFalse(cJSON_GetObjectItemCaseSensitive(link, "bidirectional")),
        };
    }
    return true;
}

static void on_fetched(const FetchResponse* r) {
    if (!s_open || atoi(r->asset_id + strlen(UMAP_ASSET_PREFIX)) != s_session) {
        fetch_data_release(r->data);
        return;
    }
    s_in_flight = false;
    cJSON* root = r->success ? cJSON_ParseWithLength((const char*)r->data, r->size) : NULL;
    const cJSON* doc = envelope_success_doc(root);
    static UmapGraph parsed;
    if (doc && parse_doc(doc, &parsed)) {
        s_graph = parsed;
        s_state = IMAP_DATA_READY;
        s_generation++;
        s_stale = false;
        s_age = 0.0f;
    } else {
        s_stale = true;
        if (IMAP_DATA_READY != s_state) s_state = IMAP_DATA_ERROR;
        LOG_WARN("universe map fetch failed");
    }
    cJSON_Delete(root);
    fetch_data_release(r->data);
}

static void request(void) {
    char asset_id[48];
    snprintf(asset_id, sizeof(asset_id), UMAP_ASSET_PREFIX "%d", s_session);
    char url[256];
    snprintf(url, sizeof(url), ENGINE_API_BASE "/cyberia-universe/instance/%s", g_game_state.instance_code);
    s_in_flight = true;
    s_since_fetch = 0.0f;
    fetch_forget(url);
    fetch_request_start(asset_id, url, on_fetched);
}

void universe_map_data_open(void) {
    s_session++;
    s_open = true;
    s_in_flight = false;
    s_stale = true;
    if (0 != strcmp(s_instance, g_game_state.instance_code)) {
        memset(&s_graph, 0, sizeof s_graph);
        s_graph.current = -1;
        s_state = IMAP_DATA_IDLE;
        copy_str(s_instance, sizeof s_instance, g_game_state.instance_code);
    }
    if ('\0' == g_game_state.instance_code[0]) {
        s_state = IMAP_DATA_ERROR;
        LOG_WARN("universe map open without instance code (metadata not received)");
        return;
    }
    if (IMAP_DATA_READY != s_state) s_state = IMAP_DATA_LOADING;
    request();
}

void universe_map_data_close(void) {
    s_session++;
    s_open = false;
    s_in_flight = false;
}

void universe_map_data_update(float dt) {
    if (!s_open || '\0' == g_game_state.instance_code[0]) return;
    s_age += dt;
    if (2.0f * UMAP_REFRESH_SECONDS <= s_age) s_stale = true;
    if (s_in_flight) return;
    s_since_fetch += dt;
    if (UMAP_REFRESH_SECONDS <= s_since_fetch) request();
}

ImapDataState    universe_map_data_state(void) { return s_state; }
const UmapGraph* universe_map_data_graph(void) { return &s_graph; }
int              universe_map_data_generation(void) { return s_generation; }
bool             universe_map_data_stale(void) { return s_stale; }

bool universe_map_data_linked(int node) {
    int here = s_graph.current;
    for (int i = 0; 0 <= here && i < s_graph.link_count; ++i) {
        const UmapLink* l = &s_graph.links[i];
        if (l->source == here && l->target == node) return true;
        if (l->bidirectional && l->target == here && l->source == node) return true;
    }
    return false;
}

UmapTravel universe_map_data_travel(int node) {
    if (0 > node || s_graph.node_count <= node) return UMAP_TRAVEL_NO_ROUTE;
    const UmapNode* n = &s_graph.nodes[node];
    if (node == s_graph.current) return UMAP_TRAVEL_HERE;
    if (s_stale) return UMAP_TRAVEL_STALE;
    if (!universe_map_data_linked(node)) return UMAP_TRAVEL_NO_ROUTE;
    if (!n->served) return UMAP_TRAVEL_NOT_SERVED;
    if (UMAP_OFFLINE == n->status) return UMAP_TRAVEL_OFFLINE;
    if (UMAP_DRAINING == n->status) return UMAP_TRAVEL_DRAINING;
    return UMAP_TRAVEL_OPEN;
}

bool universe_map_data_in_place(int node) {
    if (0 > node || s_graph.node_count <= node || s_stale) return false;
    const UmapNode* n = &s_graph.nodes[node];
    return '\0' != n->build[0] && 0 == strcmp(n->build, g_game_state.server_build);
}
