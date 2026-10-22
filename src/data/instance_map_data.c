#include "instance_map_data.h"

#include "sim/game_state.h"
#include "engine_client.h"
#include "quest_progress_store.h"
#include "log.h"
#include "utils.h"
#include "world/world_types.h"

#include <assert.h>
#include <cJSON.h>
#include <float.h>
#include <math.h>
#include <raymath.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static ImapGraph     s_graph;
static ImapDataState s_state       = IMAP_DATA_IDLE;
static int           s_users       = 0;     /* the views that hold the graph open */
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

ImapPresenceStatus instance_map_presence_of_status(uint8_t status_icon) {
    switch (status_icon) {
        case STATUS_ICON_PASSIVE:       return IMAP_PRESENCE_PASSIVE;
        case STATUS_ICON_HOSTILE:       return IMAP_PRESENCE_HOSTILE;
        case STATUS_ICON_RESOURCE:      return IMAP_PRESENCE_RESOURCE;
        case STATUS_ICON_PORTAL:        return IMAP_PRESENCE_PORTAL;
        case STATUS_ICON_PORTAL_RANDOM: return IMAP_PRESENCE_PORTAL_RANDOM;
        default:                        return IMAP_PRESENCE_NONE;
    }
}

ImapEntrySpawn instance_map_parse_entry_spawn(const cJSON* doc) {
    const cJSON* spawn = cJSON_GetObjectItemCaseSensitive(doc, "entrySpawn");
    const char* mode = json_str(spawn, "mode");
    ImapEntrySpawn out = { .mode = NULL == mode                         ? IMAP_SPAWN_NONE
                                 : 0 == strcmp(mode, "fixed")           ? IMAP_SPAWN_FIXED
                                 : 0 == strcmp(mode, "random")          ? IMAP_SPAWN_RANDOM
                                                                        : IMAP_SPAWN_UNRESOLVED,
                           .cell_x = json_int(spawn, "cellX", -1), .cell_y = json_int(spawn, "cellY", -1) };
    copy_str(out.map_code, sizeof out.map_code, json_str(spawn, "mapCode"));
    copy_str(out.map_name, sizeof out.map_name, json_str(spawn, "mapName"));
    if (IMAP_SPAWN_FIXED == out.mode && (0 > out.cell_x || 0 > out.cell_y || '\0' == out.map_code[0]))
        out.mode = IMAP_SPAWN_UNRESOLVED;
    return out;
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

static const char* const SERVICE_NAMES[IMAP_SERVICE_COUNT] = {
    [IMAP_SERVICE_SHOP] = "shop",       [IMAP_SERVICE_CRAFT] = "craft",     [IMAP_SERVICE_STORAGE] = "storage",
    [IMAP_SERVICE_MARKET] = "market",   [IMAP_SERVICE_BUYBACK] = "buyback", [IMAP_SERVICE_BURN] = "burn",
    [IMAP_SERVICE_TALK] = "talk",
};

static uint8_t parse_services(const cJSON* row) {
    uint8_t services = 0;
    const cJSON* value = NULL;
    cJSON_ArrayForEach(value, cJSON_GetObjectItemCaseSensitive(row, "services")) {
        for (int s = 0; cJSON_IsString(value) && IMAP_SERVICE_COUNT > s; ++s)
            if (0 == strcmp(value->valuestring, SERVICE_NAMES[s])) services |= (uint8_t)(1u << s);
    }
    return services;
}

/* The providers one POI lists under `key`, with the label and the codes each one links. */
static void parse_providers(const cJSON* poi_doc, const char* key, const char* label_key, const char* links_key,
                            int poi, ImapProvider* out, int* count) {
    const cJSON* row = NULL;
    cJSON_ArrayForEach(row, cJSON_GetObjectItemCaseSensitive(poi_doc, key)) {
        if (IMAP_MAX_PROVIDERS <= *count) return;
        ImapProvider* p = &out[(*count)++];
        *p = (ImapProvider){ .poi = poi, .services = parse_services(row),
                             .standalone = cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(row, "standalone")) };
        copy_str(p->code, IMAP_CODE_MAX, json_str(row, "code"));
        copy_str(p->label, IMAP_NAME_MAX, json_str(row, label_key));
        const cJSON* link = NULL;
        cJSON_ArrayForEach(link, cJSON_GetObjectItemCaseSensitive(row, links_key)) {
            if (IMAP_MAX_LINKS <= p->link_count || !cJSON_IsString(link)) break;
            copy_str(p->links[p->link_count++], IMAP_CODE_MAX, link->valuestring);
        }
    }
}

static const char* const YIELD_VIAS[IMAP_YIELD_VIA_COUNT] = {
    [IMAP_YIELD_DROP] = "drop", [IMAP_YIELD_SHOP] = "shop", [IMAP_YIELD_CRAFT] = "craft",
};

/* One row of `key` placed on a POI, under the item of `id_key`. */
static void parse_placed(const cJSON* poi_doc, const char* key, const char* id_key, int poi) {
    const cJSON* row = NULL;
    cJSON_ArrayForEach(row, cJSON_GetObjectItemCaseSensitive(poi_doc, key)) {
        if (IMAP_MAX_PLACED <= s_graph.placed_count) return;
        if (0 > s_graph.presence_pois[poi].first_placed) s_graph.presence_pois[poi].first_placed = s_graph.placed_count;
        ImapPlaced* placed = &s_graph.placed[s_graph.placed_count++];
        *placed = (ImapPlaced){ .poi = poi, .bot = 0 == strcmp("bots", key),
                                .mortal = cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(row, "mortal")),
                                .stats = json_int(row, "stats", 0) };
        copy_str(placed->item_id, sizeof placed->item_id, json_str(row, id_key));
    }
}

/* The bots and resources one POI places and the items it gives. */
static void parse_targets(const cJSON* poi_doc, int poi) {
    parse_placed(poi_doc, "bots", "skin", poi);
    parse_placed(poi_doc, "resources", "itemId", poi);
    const cJSON* row = NULL;
    cJSON_ArrayForEach(row, cJSON_GetObjectItemCaseSensitive(poi_doc, "yields")) {
        const char* via = json_str(row, "via");
        for (int v = 0; NULL != via && IMAP_YIELD_VIA_COUNT > v && IMAP_MAX_YIELDS > s_graph.yield_count; v++) {
            if (0 != strcmp(via, YIELD_VIAS[v])) continue;
            ImapYield* yield = &s_graph.yields[s_graph.yield_count++];
            *yield = (ImapYield){ .poi = poi, .via = (ImapYieldVia)v };
            copy_str(yield->item_id, sizeof yield->item_id, json_str(row, "itemId"));
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
            *poi = (ImapPresencePoi){ .node = node, .cell_x = cell_x, .cell_y = cell_y, .first_placed = -1 };
        }
        poi->presence_status = presence;
        poi->capabilities |= parse_capabilities(poi_doc);
        parse_providers(poi_doc, "quests", "title", "prerequisiteCodes", (int)(poi - s_graph.presence_pois),
                        s_graph.quests, &s_graph.quest_count);
        parse_providers(poi_doc, "actions", "label", "questCodes", (int)(poi - s_graph.presence_pois),
                        s_graph.actions, &s_graph.action_count);
        parse_targets(poi_doc, (int)(poi - s_graph.presence_pois));
    }
    for (int q = 0; s_graph.quest_count > q; ++q) s_graph.presence_pois[s_graph.quests[q].poi].quest_count++;
    for (int a = 0; s_graph.action_count > a; ++a) {
        ImapPresencePoi* poi = &s_graph.presence_pois[s_graph.actions[a].poi];
        poi->action_count++;
        poi->services |= s_graph.actions[a].services;
    }
    refresh_node_capability_counts();
}

/* Values within the tolerance take the first of them. */
static void snap(float* values, int count, float tolerance) {
    for (int i = 1; count > i; i++)
        for (int j = 0; i > j; j++)
            if (tolerance >= fabsf(values[i] - values[j])) {
                values[i] = values[j];
                break;
            }
}

/* Bounds once per graph: maps that touch share one edge value, so they round to one pixel. */
static void compose_maps(void) {
    /* Every map has one scale; with no grid at all a map is one cell, as the engine sizes it. */
    s_graph.cell = 1.0f;
    for (int i = 0; s_graph.node_count > i; i++) {
        const ImapNode* n = &s_graph.nodes[i];
        if (0 >= n->grid_x) continue;
        s_graph.cell = n->extent_w / (float)n->grid_x;
        break;
    }
    float xs[IMAP_MAX_NODES * 2], ys[IMAP_MAX_NODES * 2];
    for (int i = 0; s_graph.node_count > i; i++) {
        const ImapNode* n = &s_graph.nodes[i];
        xs[2 * i] = n->position.x - n->extent_w * 0.5f;
        xs[2 * i + 1] = n->position.x + n->extent_w * 0.5f;
        ys[2 * i] = n->position.y - n->extent_h * 0.5f;
        ys[2 * i + 1] = n->position.y + n->extent_h * 0.5f;
    }
    snap(xs, 2 * s_graph.node_count, IMAP_JOIN_TOLERANCE * s_graph.cell);
    snap(ys, 2 * s_graph.node_count, IMAP_JOIN_TOLERANCE * s_graph.cell);
    ImapBounds world = { FLT_MAX, FLT_MAX, -FLT_MAX, -FLT_MAX };
    for (int i = 0; s_graph.node_count > i; i++) {
        ImapBounds b = { xs[2 * i], ys[2 * i], xs[2 * i + 1], ys[2 * i + 1] };
        s_graph.nodes[i].bounds = b;
        world = (ImapBounds){ fminf(world.left, b.left), fminf(world.top, b.top),
                              fmaxf(world.right, b.right), fmaxf(world.bottom, b.bottom) };
    }
    s_graph.span = (Vector2){ world.right - world.left, world.bottom - world.top };
}

static bool parse_static_doc(const cJSON* doc) {
    memset(&s_graph, 0, sizeof(s_graph));
    copy_str(s_graph.instance_code, IMAP_CODE_MAX, json_str(doc, "instanceCode"));
    copy_str(s_graph.name, IMAP_NAME_MAX, json_str(doc, "name"));
    s_graph.entry_spawn = instance_map_parse_entry_spawn(doc);

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
        const cJSON* placed = cJSON_GetObjectItemCaseSensitive(nd, "placed");
        node->population = (ImapPopulation){ json_int(placed, "hostile", 0), json_int(placed, "passive", 0),
                                             json_int(placed, "resource", 0) };
        node->extent_w = json_float(nd, "w", 1.0f);
        node->extent_h = json_float(nd, "h", 1.0f);
        if (!isfinite(node->extent_w) || !isfinite(node->extent_h) || 0.0f >= node->extent_w || 0.0f >= node->extent_h) return false;
        const cJSON* position = cJSON_GetObjectItemCaseSensitive(nd, "position");
        const cJSON* x = cJSON_GetObjectItemCaseSensitive(position, "x");
        const cJSON* y = cJSON_GetObjectItemCaseSensitive(position, "y");
        if (!cJSON_IsNumber(x) || !cJSON_IsNumber(y) || !isfinite(x->valuedouble) || !isfinite(y->valuedouble)) return false;
        node->position = (Vector2){ (float)x->valuedouble, (float)y->valuedouble };
        if (!isfinite(node->position.x) || !isfinite(node->position.y)) return false;
    }
    compose_maps();
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
            e->joined        = !e->intra && instance_map_bounds_touch(s_graph.nodes[src].bounds, s_graph.nodes[tgt].bounds,
                                                                      IMAP_JOIN_TOLERANCE * s_graph.cell);
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
    if (0 == s_users || atoi(r->asset_id + strlen("imap-static-")) != s_session) {
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

ImapQuestState instance_map_quest_state(const char* code) {
    const QuestProgressEntry* e = quest_progress_store_find(code);
    if (NULL != e && QUEST_ACTIVE == e->status) return IMAP_QUEST_ACTIVE;
    if (quest_progress_store_is_completed(code)) return IMAP_QUEST_COMPLETED;
    for (int q = 0; q < s_graph.quest_count; ++q) {
        const ImapProvider* p = &s_graph.quests[q];
        if (0 != strcmp(p->code, code)) continue;
        for (int i = 0; i < p->link_count; ++i)
            if (!quest_progress_store_is_completed(p->links[i])) return IMAP_QUEST_LOCKED;
        return IMAP_QUEST_AVAILABLE;
    }
    return IMAP_QUEST_LOCKED;
}

/* A quest the player runs, or may accept. */
static bool quest_actionable(const char* code) {
    ImapQuestState state = instance_map_quest_state(code);
    return IMAP_QUEST_ACTIVE == state || IMAP_QUEST_AVAILABLE == state;
}

/* What a POI shows first: a quest to go on with, then one to take, then one locked, then one done. */
static int quest_state_rank(ImapQuestState state) {
    switch (state) {
        case IMAP_QUEST_ACTIVE:    return 3;
        case IMAP_QUEST_AVAILABLE: return 2;
        case IMAP_QUEST_LOCKED:    return 1;
        case IMAP_QUEST_COMPLETED:
        default:                   return 0;
    }
}

static void mark_active_pois(void) {
    for (int i = 0; i < s_graph.presence_poi_count; ++i) {
        s_graph.presence_pois[i].quest_active = false;
        s_graph.presence_pois[i].action_active = false;
        s_graph.presence_pois[i].quest_state = IMAP_QUEST_COMPLETED;
    }
    for (int q = 0; q < s_graph.quest_count; ++q) {
        ImapPresencePoi* poi = &s_graph.presence_pois[s_graph.quests[q].poi];
        ImapQuestState state = instance_map_quest_state(s_graph.quests[q].code);
        if (IMAP_QUEST_ACTIVE == state || IMAP_QUEST_AVAILABLE == state) poi->quest_active = true;
        if (quest_state_rank(state) > quest_state_rank(poi->quest_state)) poi->quest_state = state;
    }
    for (int a = 0; a < s_graph.action_count; ++a) {
        const ImapProvider* p = &s_graph.actions[a];
        bool active = p->standalone;
        for (int i = 0; !active && i < p->link_count; ++i) active = quest_actionable(p->links[i]);
        if (active) s_graph.presence_pois[p->poi].action_active = true;
    }
}

/* ── Lifecycle ──────────────────────────────────────────────────────────── */

static void static_url(char* out, size_t size) {
    snprintf(out, size, ENGINE_API_BASE "/cyberia-instance/instance-map/%s/static", g_game_state.instance_code);
}

/* A new session: a response of an older one is dropped. */
static void fetch_static(void) {
    s_session++;
    if ('\0' == g_game_state.instance_code[0]) {
        s_state = IMAP_DATA_ERROR;
        LOG_WARN("instance map open without instance code (metadata not received)");
        return;
    }
    s_state = IMAP_DATA_LOADING;

    char asset_id[48];
    snprintf(asset_id, sizeof(asset_id), "imap-static-%d", s_session);
    char url[256];
    static_url(url, sizeof url);
    fetch_request_start(asset_id, url, on_static_fetched);
}

void instance_map_data_open(void) {
    if (1 == ++s_users) fetch_static();
}

void instance_map_data_refresh(void) {
    char url[256];
    static_url(url, sizeof url);
    fetch_forget(url);
    if (0 < s_users) fetch_static();
}

void instance_map_data_retry(void) {
    if (IMAP_DATA_ERROR == s_state) instance_map_data_refresh();
}

void instance_map_data_close(void) {
    assert(0 < s_users);
    if (0 < --s_users) return;
    s_session++;          /* invalidates every in-flight response */
    s_state = IMAP_DATA_IDLE;
}

void instance_map_data_update(float dt) {
    if (0 == s_users || IMAP_DATA_READY != s_state) return;
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

void instance_map_edge_destination(const ImapGraph* graph, const ImapEdge* edge, char* out, size_t size) {
    bool random = 0 > edge->target_cell_x || 0 > edge->target_cell_y;
    if (edge->intra) {
        if (random) snprintf(out, size, "Random cell of this map");
        else snprintf(out, size, "This map (%d, %d)", edge->target_cell_x, edge->target_cell_y);
        return;
    }
    char name[IMAP_NAME_MAX + IMAP_CODE_MAX];
    instance_map_node_label(graph, edge->target_node, name, sizeof name);
    if (random) snprintf(out, size, "%s - random cell", name);
    else snprintf(out, size, "%s (%d, %d)", name, edge->target_cell_x, edge->target_cell_y);
}

ImapNodeLinks instance_map_node_links(const ImapGraph* graph, int node) {
    ImapNodeLinks links = { 0 };
    bool neighbor[IMAP_MAX_NODES] = { false };
    for (int i = 0; graph->edge_count > i; ++i) {
        const ImapEdge* e = &graph->edges[i];
        if (node == e->source_node) links.portals++;
        if (node != e->source_node && node != e->target_node) continue;
        int other = node == e->source_node ? e->target_node : e->source_node;
        if (other == node) continue;
        if (!neighbor[other]) links.neighbors++;
        neighbor[other] = true;
        if (node != e->source_node) continue;
        bool listed = false;
        for (int d = 0; links.destination_count > d; ++d) listed = listed || other == links.destinations[d];
        if (!listed) links.destinations[links.destination_count++] = other;
    }
    return links;
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
    return Vector2Normalize(Vector2Subtract(to->position, from->position));
}

Vector2 instance_map_cell_to_card(Rectangle card, const ImapNode* node,
                                  float cell_x, float cell_y) {
    float fx = 0 < node->grid_x ? cell_x / (float)node->grid_x : 0.5f;
    float fy = 0 < node->grid_y ? cell_y / (float)node->grid_y : 0.5f;
    return (Vector2){ card.x + Clamp(fx, 0.0f, 1.0f) * card.width,
                      card.y + Clamp(fy, 0.0f, 1.0f) * card.height };
}

bool instance_map_bounds_touch(ImapBounds a, ImapBounds b, float tolerance) {
    float across = fminf(a.right, b.right) - fmaxf(a.left, b.left);
    float along = fminf(a.bottom, b.bottom) - fmaxf(a.top, b.top);
    return (tolerance >= fabsf(across) && tolerance < along) || (tolerance >= fabsf(along) && tolerance < across);
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
