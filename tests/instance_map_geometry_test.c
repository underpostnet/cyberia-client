/* The geometry of the instance maps: each map keeps the proportions of its
 * grid at the one scale the engine sends; maps join where they share an edge
 * of positive length, whatever their sizes; corners, gaps and overlaps never
 * join; edges within the tolerance take one value; the minimap fits the
 * active map only, and its exits point to the maps they lead to. */

#include "data/instance_map_data.h"
#include "data/engine_client.h"
#include "data/quest_progress_store.h"
#include "sim/game_state.h"
#include "ui/ui_rect.h"
#include "utils.h"
#include "world/world_types.h"

#include <assert.h>
#include <cJSON.h>
#include <math.h>
#include <raylib.h>
#define RAYMATH_IMPLEMENTATION
#include <raymath.h>
#include <stdio.h>
#include <string.h>

GameState g_game_state;

static FetchCompletedCb s_callback;
static char s_asset_id[64];

void fetch_request_start(const char* id, const char* url, FetchCompletedCb callback) {
    copy_str(s_asset_id, sizeof s_asset_id, id);
    s_callback = callback;
}
void fetch_data_release(void* data) {}
void fetch_forget(const char* url) {}
/* The player's quests: one active, one completed; the revision moves when they change. */
static QuestProgressEntry s_active = { .status = QUEST_ACTIVE };
static char s_completed[64];
static int  s_revision;
const QuestProgressEntry* quest_progress_store_find(const char* code) {
    return 0 == strcmp(code, s_active.code) ? &s_active : NULL;
}
bool quest_progress_store_is_completed(const char* code) { return 0 == strcmp(code, s_completed); }
int quest_progress_store_revision(void) { return s_revision; }

/* Two maps with every kind of portal, a place with two quests and a vendor, the
 * bots and resources map A places, and an entry spawn on A. */
static const char* SERVICES_WORLD =
    "{\"status\":\"success\",\"data\":{\"instanceCode\":\"test\","
    "\"entrySpawn\":{\"mode\":\"fixed\",\"mapCode\":\"a\",\"mapName\":\"A\",\"cellX\":4,\"cellY\":5},\"nodes\":["
    "{\"mapCode\":\"a\",\"name\":\"A\",\"w\":0.5,\"h\":0.5,\"gridX\":16,\"gridY\":16,\"position\":{\"x\":-1,\"y\":0},"
    "\"placed\":{\"hostile\":1,\"passive\":2,\"resource\":3}},"
    "{\"mapCode\":\"b\",\"name\":\"B\",\"w\":0.5,\"h\":0.5,\"gridX\":16,\"gridY\":16,\"position\":{\"x\":1,\"y\":0}}],"
    "\"edges\":["
    "{\"sourceMapCode\":\"a\",\"sourceCellX\":1,\"sourceCellY\":2,\"targetMapCode\":\"b\",\"targetCellX\":3,\"targetCellY\":4},"
    "{\"sourceMapCode\":\"a\",\"sourceCellX\":5,\"sourceCellY\":6,\"targetMapCode\":\"b\",\"targetCellX\":-1,\"targetCellY\":-1},"
    "{\"sourceMapCode\":\"b\",\"sourceCellX\":7,\"sourceCellY\":8,\"targetMapCode\":\"b\",\"targetCellX\":5,\"targetCellY\":6},"
    "{\"sourceMapCode\":\"b\",\"sourceCellX\":9,\"sourceCellY\":9,\"targetMapCode\":\"b\"}],"
    "\"presencePois\":["
    "{\"mapCode\":\"a\",\"cellX\":1,\"cellY\":1,\"presenceStatus\":\"passive\",\"capabilities\":[\"action\",\"quest\"],"
    "\"quests\":[{\"code\":\"q1\",\"title\":\"First\",\"prerequisiteCodes\":[]},"
    "{\"code\":\"q2\",\"title\":\"Second\",\"prerequisiteCodes\":[\"q1\"]}],"
    "\"actions\":[{\"code\":\"vendor\",\"label\":\"Vendor\",\"standalone\":true,\"questCodes\":[\"q2\"],"
    "\"services\":[\"shop\",\"market\",\"talk\",\"unknown\"]}]},"
    "{\"mapCode\":\"b\",\"cellX\":2,\"cellY\":2,\"presenceStatus\":\"passive\",\"capabilities\":[\"action\"],"
    "\"actions\":[{\"code\":\"idle\",\"services\":[]}]}]}}";

/* The engine payload of the `test` world: two maps side by side, one as wide as both under them. */
static const char* TEST_WORLD =
    "{\"status\":\"success\",\"data\":{\"instanceCode\":\"test\",\"name\":\"Test\",\"nodes\":["
    "{\"mapCode\":\"test-a\",\"x\":0,\"y\":0,\"w\":0.5,\"h\":0.5,\"gridX\":64,\"gridY\":64,\"position\":{\"x\":-0.25,\"y\":-0.1953125}},"
    "{\"mapCode\":\"test-b\",\"x\":1,\"y\":0,\"w\":0.5,\"h\":0.5,\"gridX\":64,\"gridY\":64,\"position\":{\"x\":0.25,\"y\":-0.1953125}},"
    "{\"mapCode\":\"map-test-b\",\"x\":0,\"y\":1,\"w\":1,\"h\":0.390625,\"gridX\":128,\"gridY\":50,\"position\":{\"x\":0,\"y\":0.25}}],"
    "\"edges\":["
    "{\"sourceMapCode\":\"test-a\",\"sourceCellX\":60,\"sourceCellY\":10,\"targetMapCode\":\"test-b\",\"targetCellX\":1,\"targetCellY\":1},"
    "{\"sourceMapCode\":\"test-b\",\"sourceCellX\":3,\"sourceCellY\":3,\"targetMapCode\":\"test-b\"},"
    "{\"sourceMapCode\":\"map-test-b\",\"sourceCellX\":0,\"sourceCellY\":0,\"targetMapCode\":\"test-a\",\"targetCellX\":2,\"targetCellY\":2}],"
    "\"presencePois\":[]}}";

/* The engine payload of the `amethyst-strata-expansion` world: a column of
 * two maps, an empty column, two maps in a row, and a wide map under the
 * column that meets the row at one corner. */
static const char* AMETHYST_WORLD =
    "{\"status\":\"success\",\"data\":{\"instanceCode\":\"amethyst\",\"nodes\":["
    "{\"mapCode\":\"foliage\",\"name\":\"Gate\",\"w\":0.5,\"h\":0.5,\"gridX\":64,\"gridY\":64,\"position\":{\"x\":-0.75,\"y\":0.0546875}},"
    "{\"mapCode\":\"athel\",\"name\":\"Gate\",\"w\":0.5,\"h\":0.5,\"gridX\":64,\"gridY\":64,\"position\":{\"x\":0.25,\"y\":0.0546875}},"
    "{\"mapCode\":\"sap\",\"name\":\"Sap\",\"w\":0.5,\"h\":0.5,\"gridX\":64,\"gridY\":64,\"position\":{\"x\":-0.75,\"y\":-0.4453125}},"
    "{\"mapCode\":\"verdant\",\"w\":0.5,\"h\":0.5,\"gridX\":64,\"gridY\":64,\"position\":{\"x\":0.75,\"y\":0.0546875}},"
    "{\"mapCode\":\"map-test\",\"w\":1,\"h\":0.390625,\"gridX\":128,\"gridY\":50,\"position\":{\"x\":-0.5,\"y\":0.5}}],"
    "\"edges\":["
    "{\"sourceMapCode\":\"foliage\",\"targetMapCode\":\"athel\"},"
    "{\"sourceMapCode\":\"sap\",\"targetMapCode\":\"foliage\"},"
    "{\"sourceMapCode\":\"map-test\",\"targetMapCode\":\"athel\"},"
    "{\"sourceMapCode\":\"athel\",\"targetMapCode\":\"verdant\"},"
    "{\"sourceMapCode\":\"map-test\",\"targetMapCode\":\"foliage\"}],"
    "\"presencePois\":[]}}";

/* Centres a float step apart: the edges of neighbours differ by far less than a map cell. */
static const char* JITTER_WORLD =
    "{\"status\":\"success\",\"data\":{\"instanceCode\":\"jitter\",\"nodes\":["
    "{\"mapCode\":\"a\",\"w\":0.3333333,\"h\":0.3333333,\"gridX\":48,\"gridY\":48,\"position\":{\"x\":-0.1666667,\"y\":0}},"
    "{\"mapCode\":\"b\",\"w\":0.3333333,\"h\":0.3333333,\"gridX\":48,\"gridY\":48,\"position\":{\"x\":0.16666671,\"y\":0.0000001}},"
    "{\"mapCode\":\"c\",\"w\":1,\"h\":0.5,\"gridX\":144,\"gridY\":72,\"position\":{\"x\":0.3333334,\"y\":0.4166667}}],"
    "\"edges\":[],\"presencePois\":[]}}";

static bool near(float a, float b) { return fabsf(a - b) < 0.01f; }

static const ImapGraph* load(const char* payload) {
    instance_map_data_refresh();
    FetchResponse response = { .asset_id = s_asset_id, .success = true, .data = (void*)payload, .size = strlen(payload) };
    s_callback(&response);
    assert(IMAP_DATA_READY == instance_map_data_state());
    return instance_map_data_graph();
}

static ImapBounds box(float left, float top, float right, float bottom) {
    return (ImapBounds){ left, top, right, bottom };
}

static bool touch(ImapBounds a, ImapBounds b) {
    const float tolerance = IMAP_JOIN_TOLERANCE / 64.0f;
    bool joined = instance_map_bounds_touch(a, b, tolerance);
    assert(joined == instance_map_bounds_touch(b, a, tolerance));
    return joined;
}

static Rectangle card_of(const ImapNode* node, float side) {
    ImapBounds b = node->bounds;
    return (Rectangle){ b.left * side, b.top * side, (b.right - b.left) * side, (b.bottom - b.top) * side };
}

int main(void) {
    copy_str(g_game_state.instance_code, sizeof g_game_state.instance_code, "test");
    instance_map_data_open();

    /* Equal maps on a whole edge, different maps on a whole edge, a small map on part of a large edge. */
    assert(touch(box(0, 0, 1, 1), box(1, 0, 2, 1)));
    assert(touch(box(0, 0, 2, 1), box(2, 0, 3, 1)));
    assert(touch(box(0, 0, 2, 2), box(2, 0.5f, 3, 1.5f)));
    assert(touch(box(0, 0, 2, 2), box(0.5f, 2, 1, 3)));
    /* A gap, a corner, an overlap and an island never join. */
    assert(!touch(box(0, 0, 1, 1), box(1.5f, 0, 2.5f, 1)));
    assert(!touch(box(0, 0, 1, 1), box(1, 1, 2, 2)));
    assert(!touch(box(0, 0, 2, 1), box(1, 0, 3, 1)));
    assert(!touch(box(0, 0, 1, 1), box(4, 4, 5, 5)));
    /* Within the tolerance an edge is one edge; past it, a gap or an overlap. */
    const float cell = 1.0f / 64.0f;
    assert(touch(box(0, 0, 1, 1), box(1 + 0.005f * cell, 0, 2, 1)));
    assert(touch(box(0, 0, 1, 1), box(1 - 0.005f * cell, 0, 2, 1)));
    assert(!touch(box(0, 0, 1, 1), box(1 + 0.05f * cell, 0, 2, 1)));
    assert(!touch(box(0, 0, 1, 1), box(1 - 0.05f * cell, 0, 2, 1)));
    /* A shared stretch shorter than the tolerance is a corner. */
    assert(!touch(box(0, 0, 1, 1), box(1, 1 - 0.005f * cell, 2, 2)));

    /* The `test` world: one surface of three maps, two sizes. */
    const ImapGraph* graph = load(TEST_WORLD);
    assert(3 == graph->node_count && near(graph->span.x, 1.0f) && near(graph->span.y, 114.0f / 128.0f));
    assert(near(graph->cell, 1.0f / 128.0f));
    const ImapNode *a = &graph->nodes[0], *b = &graph->nodes[1], *wide = &graph->nodes[2];
    assert(a->bounds.right == b->bounds.left && a->bounds.bottom == wide->bounds.top);
    assert(b->bounds.bottom == wide->bounds.top);
    assert(a->bounds.left == wide->bounds.left && b->bounds.right == wide->bounds.right);
    assert(graph->edges[0].joined && !graph->edges[1].joined && graph->edges[2].joined);
    assert(!graph->edges[1].joined && graph->edges[1].intra);

    /* A map keeps the proportions of its grid. */
    Rectangle card = card_of(wide, 300.0f);
    assert(near(card.width / card.height, 128.0f / 50.0f));
    card = card_of(a, 300.0f);
    assert(near(card.width, card.height));

    /* A cell of the map lands where it is on the map, whatever the card shape. */
    card = card_of(wide, 100.0f);
    Vector2 corner = instance_map_cell_to_card(card, wide, 128.0f, 50.0f);
    assert(near(corner.x, card.x + card.width) && near(corner.y, card.y + card.height));

    /* An exit points from its map to the map it leads to; a link inside one map has no direction. */
    Vector2 east = instance_map_exit_direction(graph, &graph->edges[0]);
    Vector2 inside = instance_map_exit_direction(graph, &graph->edges[1]);
    assert(near(east.x, 1.0f) && near(east.y, 0.0f));
    assert(near(inside.x, 0.0f) && near(inside.y, 0.0f));

    /* An action carries its label and the services it configures; a quest its title. A place sums them up. */
    graph = load(SERVICES_WORLD);
    assert(2 == graph->action_count && 2 == graph->quest_count && 2 == graph->presence_poi_count);
    assert(0 == strcmp("Vendor", graph->actions[0].label) && 0 == strcmp("Second", graph->quests[1].label));
    uint8_t vendor = (1u << IMAP_SERVICE_SHOP) | (1u << IMAP_SERVICE_MARKET) | (1u << IMAP_SERVICE_TALK);
    assert(vendor == graph->actions[0].services && 0 == graph->actions[1].services);
    const ImapPresencePoi* place = &graph->presence_pois[0];
    assert(2 == place->quest_count && 1 == place->action_count && vendor == place->services);
    assert(0 == graph->presence_pois[1].quest_count && 1 == graph->presence_pois[1].action_count);

    /* Each map counts the bots and resources it places; the world names its entry spawn. */
    const ImapPopulation* population = &graph->nodes[0].population;
    assert(1 == population->hostile && 2 == population->passive && 3 == population->resource);
    assert(0 == graph->nodes[1].population.passive);
    const ImapEntrySpawn* spawn = &graph->entry_spawn;
    assert(IMAP_SPAWN_FIXED == spawn->mode && 0 == strcmp("a", spawn->map_code) && 0 == strcmp("A", spawn->map_name));
    assert(4 == spawn->cell_x && 5 == spawn->cell_y);
    /* A fixed spawn without a map or a cell, or an unknown mode, is unresolved; no entry spawn is none. */
    const char* unresolved[] = { "{\"entrySpawn\":{\"mode\":\"fixed\",\"mapCode\":\"a\"}}",
                                 "{\"entrySpawn\":{\"mode\":\"fixed\",\"cellX\":1,\"cellY\":1}}",
                                 "{\"entrySpawn\":{\"mode\":\"elsewhere\"}}" };
    for (int i = 0; 3 > i; i++) {
        cJSON* doc = cJSON_Parse(unresolved[i]);
        assert(IMAP_SPAWN_UNRESOLVED == instance_map_parse_entry_spawn(doc).mode);
        cJSON_Delete(doc);
    }
    cJSON* empty = cJSON_Parse("{\"entrySpawn\":null}");
    assert(IMAP_SPAWN_NONE == instance_map_parse_entry_spawn(empty).mode);
    cJSON_Delete(empty);

    /* A map draws the presence class of a status. Capabilities, players, the dead and unknown ids have none. */
    assert(IMAP_PRESENCE_PASSIVE == instance_map_presence_of_status(STATUS_ICON_PASSIVE));
    assert(IMAP_PRESENCE_HOSTILE == instance_map_presence_of_status(STATUS_ICON_HOSTILE));
    assert(IMAP_PRESENCE_RESOURCE == instance_map_presence_of_status(STATUS_ICON_RESOURCE));
    assert(IMAP_PRESENCE_PORTAL == instance_map_presence_of_status(STATUS_ICON_PORTAL));
    assert(IMAP_PRESENCE_PORTAL_RANDOM == instance_map_presence_of_status(STATUS_ICON_PORTAL_RANDOM));
    const uint8_t none[] = { STATUS_ICON_NONE, STATUS_ICON_FROZEN, STATUS_ICON_PLAYER, STATUS_ICON_DEAD,
                             STATUS_ICON_RESOURCE_EXTRACTED, STATUS_ICON_ACTION_PROVIDER, STATUS_ICON_QUEST_PROVIDER,
                             12, 255 };
    for (int i = 0; (int)sizeof none > i; i++) assert(IMAP_PRESENCE_NONE == instance_map_presence_of_status(none[i]));

    /* The quest states follow the server: active and completed from the store, else the prerequisites decide. */
    instance_map_data_update(0.0f);
    assert(IMAP_QUEST_AVAILABLE == instance_map_quest_state("q1") && IMAP_QUEST_LOCKED == instance_map_quest_state("q2"));
    assert(IMAP_QUEST_AVAILABLE == place->quest_state && IMAP_QUEST_LOCKED == instance_map_quest_state("other"));
    copy_str(s_completed, sizeof s_completed, "q1");
    assert(IMAP_QUEST_COMPLETED == instance_map_quest_state("q1") && IMAP_QUEST_AVAILABLE == instance_map_quest_state("q2"));
    copy_str(s_active.code, sizeof s_active.code, "q2");
    s_revision++;
    instance_map_data_update(0.0f);
    assert(IMAP_QUEST_ACTIVE == instance_map_quest_state("q2") && IMAP_QUEST_ACTIVE == place->quest_state);
    s_active.code[0] = s_completed[0] = '\0';
    s_revision++;

    /* A destination names the map and the cell, a random cell, or the same map. */
    char to[IMAP_NAME_MAX * 3];
    const char* expected[] = { "B (3, 4)", "B - random cell", "This map (5, 6)", "Random cell of this map" };
    for (int e = 0; 4 > e; e++) {
        instance_map_edge_destination(graph, &graph->edges[e], to, sizeof to);
        assert(0 == strcmp(expected[e], to));
    }
    graph = load(TEST_WORLD);

    /* A map counts the portals on it, the maps linked to it either way, and where its portals lead. */
    ImapNodeLinks links = instance_map_node_links(graph, 0);
    assert(1 == links.portals && 2 == links.neighbors && 1 == links.destination_count && 1 == links.destinations[0]);
    links = instance_map_node_links(graph, 1);
    assert(1 == links.portals && 1 == links.neighbors && 0 == links.destination_count);
    links = instance_map_node_links(graph, 2);
    assert(1 == links.portals && 1 == links.neighbors && 1 == links.destination_count && 0 == links.destinations[0]);

    /* The minimap fits the active map, and follows the player at a closer zoom. */
    Rectangle bounds = { 0, 0, 200, 160 };
    Rectangle current = instance_map_local_rect(wide, bounds, (Vector2){ 64, 25 }, 1.0f);
    assert(near(current.width, 200.0f) && near(current.height, 200.0f * 50.0f / 128.0f));
    current = instance_map_local_rect(wide, bounds, (Vector2){ 64, 25 }, 2.0f);
    Vector2 marker = instance_map_cell_to_card(current, wide, 64, 25);
    assert(near(marker.x, 100.0f) && near(marker.y, 80.0f));
    current = instance_map_local_rect(wide, bounds, (Vector2){ 0, 0 }, 3.0f);
    assert(near(current.x, bounds.x) && near(current.y, bounds.y));

    Rectangle fit = ui_rect_contain((Rectangle){ 0, 0, 100, 50 }, 64, 64);
    assert(near(fit.width, 50.0f) && near(fit.height, 50.0f) && near(fit.x, 25.0f) && near(fit.y, 0.0f));

    /* The `amethyst-strata-expansion` world: the empty column stays, the corner never joins. */
    graph = load(AMETHYST_WORLD);
    const ImapNode *foliage = &graph->nodes[0], *athel = &graph->nodes[1], *sap = &graph->nodes[2];
    const ImapNode *verdant = &graph->nodes[3], *map_test = &graph->nodes[4];
    assert(near(graph->span.x, 2.0f) && near(graph->span.y, 178.0f / 128.0f));
    assert(foliage->bounds.top == athel->bounds.top && athel->bounds.top == verdant->bounds.top);
    assert(near(athel->bounds.left - foliage->bounds.right, 0.5f));
    assert(athel->bounds.right == verdant->bounds.left && sap->bounds.bottom == foliage->bounds.top);
    assert(map_test->bounds.right == athel->bounds.left && map_test->bounds.top == athel->bounds.bottom);
    assert(!graph->edges[0].joined && graph->edges[1].joined && !graph->edges[2].joined);
    assert(graph->edges[3].joined && graph->edges[4].joined);
    /* Two maps of one name tell themselves apart by their codes. */
    char label[IMAP_NAME_MAX];
    instance_map_node_label(graph, 0, label, sizeof label);
    assert(0 == strcmp(label, "Gate (foliage)"));
    instance_map_node_label(graph, 2, label, sizeof label);
    assert(0 == strcmp(label, "Sap"));

    /* Edges a float step apart take one value; a map under both meets them. */
    graph = load(JITTER_WORLD);
    assert(graph->nodes[0].bounds.right == graph->nodes[1].bounds.left);
    assert(graph->nodes[0].bounds.bottom == graph->nodes[2].bounds.top);
    assert(graph->nodes[1].bounds.bottom == graph->nodes[2].bounds.top);
    assert(graph->nodes[0].bounds.top == graph->nodes[1].bounds.top);

    const char* invalid[] = {
        "{\"nodes\":[{\"mapCode\":\"a\"}]}",
        "{\"nodes\":[{\"mapCode\":\"a\",\"position\":{\"x\":\"0\",\"y\":0}}]}",
        "{\"nodes\":[{\"mapCode\":\"a\",\"position\":{\"x\":1e99,\"y\":0}}]}",
        "{\"nodes\":[{\"mapCode\":\"a\",\"w\":0,\"position\":{\"x\":0,\"y\":0}}]}",
    };
    for (int i = 0; 4 > i; i++) {
        instance_map_data_refresh();
        char payload[512];
        snprintf(payload, sizeof payload, "{\"status\":\"success\",\"data\":%s}", invalid[i]);
        FetchResponse response = { .asset_id = s_asset_id, .success = true, .data = payload, .size = strlen(payload) };
        s_callback(&response);
        assert(IMAP_DATA_ERROR == instance_map_data_state());
    }
    puts("instance_map_geometry_test OK");
    return 0;
}
