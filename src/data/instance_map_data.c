#include "instance_map_data.h"

#include "sim/game_state.h"
#include "engine_client.h"
#include "quest_progress_store.h"
#include "log.h"
#include "utils.h"
#include "ui/graph_layout.h"

#include <cJSON.h>
#include <float.h>
#include <math.h>
#include <raymath.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static ImapGraph     s_graph;
static ImapDataState s_state       = IMAP_DATA_IDLE;
static bool          s_open        = false;
static int           s_generation  = 0;
/* The quest store revision and the graph the active marks were made for. */
static int           s_marked_revision   = -1;
static int           s_marked_generation = -1;

/* Session guard: bumped on every open/close so responses that complete after
 * a close (or across a reopen) are recognised as stale and dropped. */
static int s_session = 0;

int instance_map_data_find_node(const char* map_code) {
    if (!map_code) return -1;
    for (int i = 0; i < s_graph.node_count; ++i) {
        if (0 == strcmp(s_graph.nodes[i].map_code, map_code)) return i;
    }
    return -1;
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

/* The providers one POI lists under `key`, with the codes each one links. */
static void parse_providers(const cJSON* poi_doc, const char* key, const char* links_key, int poi,
                            ImapProvider* out, int* count) {
    const cJSON* row = NULL;
    cJSON_ArrayForEach(row, cJSON_GetObjectItemCaseSensitive(poi_doc, key)) {
        if (IMAP_MAX_PROVIDERS <= *count) return;
        ImapProvider* p = &out[(*count)++];
        *p = (ImapProvider){ .poi = poi, .standalone = cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(row, "standalone")) };
        copy_str(p->code, IMAP_CODE_MAX, json_str(row, "code"));
        const cJSON* link = NULL;
        cJSON_ArrayForEach(link, cJSON_GetObjectItemCaseSensitive(row, links_key)) {
            if (IMAP_MAX_LINKS <= p->link_count || !cJSON_IsString(link)) break;
            copy_str(p->links[p->link_count++], IMAP_CODE_MAX, link->valuestring);
        }
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
        poi->capabilities |= parse_capabilities(poi_doc);
        parse_providers(poi_doc, "quests", "prerequisiteCodes", (int)(poi - s_graph.presence_pois),
                        s_graph.quests, &s_graph.quest_count);
        parse_providers(poi_doc, "actions", "questCodes", (int)(poi - s_graph.presence_pois),
                        s_graph.actions, &s_graph.action_count);
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
        copy_str(node->preview_url, IMAP_URL_MAX, json_str(nd, "previewUrl"));
        const cJSON* world = cJSON_GetObjectItemCaseSensitive(nd, "world");
        node->placed = cJSON_IsObject(world);
        if (node->placed) node->world = (Vector2){ json_float(world, "x", 0.0f), json_float(world, "y", 0.0f) };
        node->grid_x = json_int(nd, "gridX", 16);
        node->grid_y = json_int(nd, "gridY", 16);
        node->extent_w = json_float(nd, "w", 1.0f);
        node->extent_h = json_float(nd, "h", 1.0f);
        node->layout_x = json_int(nd, "x", 0);
        node->layout_y = json_int(nd, "y", 0);
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
            e->source_cell_x = json_int(ed, "sourceCellX", -1);
            e->source_cell_y = json_int(ed, "sourceCellY", -1);
            e->target_cell_x = json_int(ed, "targetCellX", -1);
            e->target_cell_y = json_int(ed, "targetCellY", -1);
            s_graph.nodes[src].portal_count++;
            if (tgt != src) s_graph.nodes[tgt].portal_count++;
        }
    }

    return s_graph.node_count > 0;
}

static void on_static_fetched(const FetchResponse* r) {
    /* asset_id carries the session stamp — drop stale/closed sessions. */
    if (!s_open || atoi(r->asset_id + strlen("imap-static-")) != s_session) {
        fetch_data_release(r->data);
        return;
    }
    if (!r->success) {
        s_state = IMAP_DATA_ERROR;
        LOG_WARN("instance map static fetch failed");
        fetch_data_release(r->data);
        return;
    }
    cJSON* root = cJSON_ParseWithLength((const char*)r->data, r->size);
    const cJSON* doc = envelope_success_doc(root);
    if (doc && parse_static_doc(doc)) {
        s_state = IMAP_DATA_READY;
        s_generation++;
    } else {
        s_state = IMAP_DATA_ERROR;
        LOG_WARN("instance map static parse failed");
    }
    cJSON_Delete(root);
    fetch_data_release(r->data);
}

/* ── Active marks ───────────────────────────────────────────────────────── */

static bool quest_running(const char* code) {
    const QuestProgressEntry* e = quest_progress_store_find(code);
    return NULL != e && QUEST_ACTIVE == e->status;
}

/* A quest the player runs, or may accept: not completed, and every
 * prerequisite completed. */
static bool quest_actionable(const char* code) {
    if (quest_running(code)) return true;
    if (quest_progress_store_is_completed(code)) return false;
    for (int q = 0; q < s_graph.quest_count; ++q) {
        const ImapProvider* p = &s_graph.quests[q];
        if (0 != strcmp(p->code, code)) continue;
        for (int i = 0; i < p->link_count; ++i)
            if (!quest_progress_store_is_completed(p->links[i])) return false;
        return true;
    }
    return false;
}

static void mark_active_pois(void) {
    for (int i = 0; i < s_graph.presence_poi_count; ++i) {
        s_graph.presence_pois[i].quest_active = false;
        s_graph.presence_pois[i].action_active = false;
    }
    for (int q = 0; q < s_graph.quest_count; ++q) {
        if (quest_actionable(s_graph.quests[q].code))
            s_graph.presence_pois[s_graph.quests[q].poi].quest_active = true;
    }
    for (int a = 0; a < s_graph.action_count; ++a) {
        const ImapProvider* p = &s_graph.actions[a];
        bool active = p->standalone;
        for (int i = 0; !active && i < p->link_count; ++i) active = quest_actionable(p->links[i]);
        if (active) s_graph.presence_pois[p->poi].action_active = true;
    }
}

/* ── Lifecycle ──────────────────────────────────────────────────────────── */

void instance_map_data_open(void) {
    s_session++;
    s_open = true;

    if ('\0' == g_game_state.instance_code[0]) {
        s_state = IMAP_DATA_ERROR;
        LOG_WARN("instance map open without instance code (metadata not received)");
        return;
    }
    s_state = IMAP_DATA_LOADING;

    char asset_id[48];
    snprintf(asset_id, sizeof(asset_id), "imap-static-%d", s_session);
    char url[256];
    snprintf(url, sizeof(url), ENGINE_API_BASE "/cyberia-instance/instance-map/%s/static",
             g_game_state.instance_code);
    fetch_request_start(asset_id, url, on_static_fetched);
}

void instance_map_data_close(void) {
    s_session++;          /* invalidates every in-flight response */
    s_open  = false;
    s_state = IMAP_DATA_IDLE;
}

void instance_map_data_update(float dt) {
    (void)dt;
    if (!s_open || IMAP_DATA_READY != s_state) return;
    if (s_marked_revision == quest_progress_store_revision() && s_marked_generation == s_generation) return;
    s_marked_revision = quest_progress_store_revision();
    s_marked_generation = s_generation;
    mark_active_pois();
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

/* One presence palette for both maps. The instance map values are the source;
 * the minimap used to hold its own slightly different copies. */
Color instance_map_presence_color(ImapPresenceStatus status) {
    switch (status) {
        case IMAP_PRESENCE_HOSTILE:       return (Color){ 220, 76, 70, 255 };
        case IMAP_PRESENCE_RESOURCE:      return (Color){ 105, 200, 105, 255 };
        case IMAP_PRESENCE_PORTAL:        return (Color){ 60, 200, 255, 255 };
        case IMAP_PRESENCE_PORTAL_RANDOM: return (Color){ 170, 110, 255, 255 };
        case IMAP_PRESENCE_PASSIVE:       return (Color){ 205, 225, 245, 235 };
        case IMAP_PRESENCE_NONE:
        default:                          return (Color){ 130, 150, 180, 190 };
    }
}

Rectangle instance_map_node_rect(const ImapNode* node, Vector2 center, float cell_side) {
    float w = node->extent_w * cell_side;
    float h = node->extent_h * cell_side;
    return (Rectangle){ center.x - w * 0.5f, center.y - h * 0.5f, w, h };
}

Rectangle instance_map_local_rect(const ImapNode* node, Rectangle bounds, Vector2 player, float zoom) {
    float gx = 0 < node->grid_x ? (float)node->grid_x : 1.0f;
    float gy = 0 < node->grid_y ? (float)node->grid_y : 1.0f;
    float scale = fminf(bounds.width / gx, bounds.height / gy) * fmaxf(1.0f, zoom);
    float w = gx * scale, h = gy * scale;
    float x = bounds.x + bounds.width * 0.5f - player.x * scale;
    float y = bounds.y + bounds.height * 0.5f - player.y * scale;
    x = w > bounds.width ? Clamp(x, bounds.x + bounds.width - w, bounds.x)
                         : bounds.x + (bounds.width - w) * 0.5f;
    y = h > bounds.height ? Clamp(y, bounds.y + bounds.height - h, bounds.y)
                          : bounds.y + (bounds.height - h) * 0.5f;
    return (Rectangle){ x, y, w, h };
}

Vector2 instance_map_exit_direction(const ImapGraph* graph, const ImapEdge* edge) {
    const ImapNode* from = &graph->nodes[edge->source_node];
    const ImapNode* to = &graph->nodes[edge->target_node];
    if (from->placed && to->placed) return Vector2Normalize(Vector2Subtract(to->world, from->world));
    return Vector2Normalize((Vector2){ (float)(to->layout_x - from->layout_x), (float)(to->layout_y - from->layout_y) });
}

Vector2 instance_map_cell_to_card(Rectangle card, const ImapNode* node,
                                  float cell_x, float cell_y) {
    float fx = 0 < node->grid_x ? cell_x / (float)node->grid_x : 0.5f;
    float fy = 0 < node->grid_y ? cell_y / (float)node->grid_y : 0.5f;
    return (Vector2){ card.x + Clamp(fx, 0.0f, 1.0f) * card.width,
                      card.y + Clamp(fy, 0.0f, 1.0f) * card.height };
}

/* Two lengths of one world, equal up to float error. */
static bool same_length(float a, float b) { return fabsf(a - b) <= 1e-4f * fmaxf(1.0f, fmaxf(fabsf(a), fabsf(b))); }

unsigned instance_map_shared_edges(const ImapNode* a, const ImapNode* b) {
    if (!a->placed || !b->placed) return 0;
    float dx = b->world.x - a->world.x, dy = b->world.y - a->world.y;
    unsigned edges = 0;
    if (same_length(dy, 0.0f) && same_length(a->extent_h, b->extent_h)) {
        float across = (a->extent_w + b->extent_w) * 0.5f;
        if (same_length(dx, -across)) edges |= IMAP_EDGE_LEFT;
        if (same_length(dx, across)) edges |= IMAP_EDGE_RIGHT;
    }
    if (same_length(dx, 0.0f) && same_length(a->extent_w, b->extent_w)) {
        float across = (a->extent_h + b->extent_h) * 0.5f;
        if (same_length(dy, -across)) edges |= IMAP_EDGE_TOP;
        if (same_length(dy, across)) edges |= IMAP_EDGE_BOTTOM;
    }
    return edges;
}

unsigned instance_map_joined_edges(const ImapGraph* graph, int index) {
    unsigned edges = 0;
    for (int i = 0; graph->node_count > i; i++)
        if (i != index) edges |= instance_map_shared_edges(&graph->nodes[index], &graph->nodes[i]);
    return edges;
}

Vector2 instance_map_layout(const ImapGraph* graph, float gap, Vector2* centers) {
    Vector2 cells[IMAP_MAX_NODES], sizes[IMAP_MAX_NODES], packed[IMAP_MAX_NODES];
    int indices[IMAP_MAX_NODES], count = 0;
    float left = FLT_MAX, top = FLT_MAX, right = -FLT_MAX, bottom = -FLT_MAX;
    for (int i = 0; graph->node_count > i; i++) {
        const ImapNode* n = &graph->nodes[i];
        if (n->placed) {
            centers[i] = n->world;
            left = fminf(left, n->world.x - n->extent_w * 0.5f);
            right = fmaxf(right, n->world.x + n->extent_w * 0.5f);
            top = fminf(top, n->world.y - n->extent_h * 0.5f);
            bottom = fmaxf(bottom, n->world.y + n->extent_h * 0.5f);
            continue;
        }
        cells[count] = (Vector2){ (float)n->layout_x, (float)n->layout_y };
        sizes[count] = (Vector2){ n->extent_w, n->extent_h };
        indices[count++] = i;
    }
    Vector2 span = graph_layout_pack(cells, sizes, count, gap, packed);
    if (FLT_MAX == left) {
        for (int i = 0; count > i; i++) centers[indices[i]] = packed[i];
        return span;
    }
    /* The other maps sit right of the placed ones, level with their top. */
    if (0 < count) {
        for (int i = 0; count > i; i++)
            centers[indices[i]] = (Vector2){ packed[i].x + right + gap + span.x * 0.5f, packed[i].y + top + span.y * 0.5f };
        right += gap + span.x;
        bottom = fmaxf(bottom, top + span.y);
    }
    for (int i = 0; graph->node_count > i; i++) {
        centers[i].x -= (left + right) * 0.5f;
        centers[i].y -= (top + bottom) * 0.5f;
    }
    return (Vector2){ right - left, bottom - top };
}

void instance_map_node_label(const ImapGraph* graph, int index, char* out, size_t size) {
    const ImapNode* node = &graph->nodes[index];
    for (int i = 0; graph->node_count > i; i++) {
        if (i == index || 0 != strcmp(graph->nodes[i].name, node->name)) continue;
        snprintf(out, size, "%s (%s)", node->name, node->map_code);
        return;
    }
    copy_str(out, size, node->name);
}
