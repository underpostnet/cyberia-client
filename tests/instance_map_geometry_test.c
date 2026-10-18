/* The geometry of the instance maps: each map card has the proportions of its
 * map at the one scale the engine sends, centred on its cell, at every zoom; a
 * picture keeps its aspect ratio inside its card; the minimap fits the active
 * map only, and its exits point to the maps they lead to. Maps with world
 * origins sit at their places and join only along whole shared edges. */

#include "data/instance_map_data.h"
#include "data/engine_client.h"
#include "data/quest_progress_store.h"
#include "sim/game_state.h"
#include "ui/ui_rect.h"
#include "utils.h"

#include <assert.h>
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
const QuestProgressEntry* quest_progress_store_find(const char* code) { return NULL; }
bool quest_progress_store_is_completed(const char* code) { return false; }
int quest_progress_store_revision(void) { return 0; }

static const char* STATIC =
    "{\"status\":\"success\",\"data\":{\"instanceCode\":\"forest\",\"name\":\"Forest\",\"nodes\":["
    "{\"mapCode\":\"wide\",\"x\":0,\"y\":0,\"w\":0.88,\"h\":0.44,\"gridX\":64,\"gridY\":32},"
    "{\"mapCode\":\"tall\",\"x\":1,\"y\":0,\"w\":0.44,\"h\":0.88,\"gridX\":32,\"gridY\":64},"
    "{\"mapCode\":\"old\",\"x\":2,\"y\":0,\"gridX\":16,\"gridY\":16}],"
    "\"edges\":["
    "{\"sourceMapCode\":\"wide\",\"sourceCellX\":60,\"sourceCellY\":10,\"targetMapCode\":\"tall\",\"targetCellX\":1,\"targetCellY\":1},"
    "{\"sourceMapCode\":\"tall\",\"sourceCellX\":3,\"sourceCellY\":3,\"targetMapCode\":\"tall\"},"
    "{\"sourceMapCode\":\"old\",\"sourceCellX\":0,\"sourceCellY\":0,\"targetMapCode\":\"wide\",\"targetCellX\":2,\"targetCellY\":2}],"
    "\"presencePois\":[]}}";

static const char* SPATIAL =
    "{\"status\":\"success\",\"data\":{\"instanceCode\":\"forest\",\"nodes\":["
    "{\"mapCode\":\"a\",\"name\":\"Gate\",\"w\":0.88,\"h\":0.88,\"gridX\":64,\"gridY\":64,\"world\":{\"x\":0.44,\"y\":0.44}},"
    "{\"mapCode\":\"b\",\"name\":\"Gate\",\"w\":0.88,\"h\":0.88,\"gridX\":64,\"gridY\":64,\"world\":{\"x\":1.32,\"y\":0.44}},"
    "{\"mapCode\":\"c\",\"name\":\"Cliff\",\"w\":0.44,\"h\":0.88,\"gridX\":32,\"gridY\":64,\"world\":{\"x\":1.98,\"y\":0.44}},"
    "{\"mapCode\":\"d\",\"w\":0.88,\"h\":0.88,\"gridX\":64,\"gridY\":64,\"world\":{\"x\":0.44,\"y\":1.32}},"
    "{\"mapCode\":\"e\",\"w\":0.88,\"h\":0.88,\"gridX\":64,\"gridY\":64,\"world\":{\"x\":1.815,\"y\":1.815}},"
    "{\"mapCode\":\"f\",\"x\":0,\"y\":0,\"w\":0.88,\"h\":0.88,\"gridX\":64,\"gridY\":64,\"world\":null}],"
    "\"edges\":["
    "{\"sourceMapCode\":\"a\",\"sourceCellX\":63,\"sourceCellY\":10,\"targetMapCode\":\"b\",\"targetCellX\":0,\"targetCellY\":10},"
    "{\"sourceMapCode\":\"b\",\"sourceCellX\":5,\"sourceCellY\":5,\"targetMapCode\":\"elsewhere\"},"
    "{\"sourceMapCode\":\"c\",\"sourceCellX\":5,\"sourceCellY\":5,\"targetMapCode\":\"f\"}],"
    "\"presencePois\":[]}}";

static bool near(float a, float b) { return fabsf(a - b) < 0.01f; }

int main(void) {
    copy_str(g_game_state.instance_code, sizeof g_game_state.instance_code, "forest");
    instance_map_data_open();
    FetchResponse response = { .asset_id = s_asset_id, .success = true, .data = (void*)STATIC, .size = strlen(STATIC) };
    s_callback(&response);
    const ImapGraph* graph = instance_map_data_graph();
    assert(IMAP_DATA_READY == instance_map_data_state() && 3 == graph->node_count);

    /* A landscape map and a portrait map keep their proportions at every zoom. */
    for (float side = 40.0f; side < 1000.0f; side *= 3.0f) {
        Rectangle wide = instance_map_node_rect(&graph->nodes[0], (Vector2){ 100, 100 }, side);
        Rectangle tall = instance_map_node_rect(&graph->nodes[1], (Vector2){ 100, 100 }, side);
        assert(near(wide.width / wide.height, 2.0f) && near(tall.height / tall.width, 2.0f));
        assert(near(wide.width, tall.height) && near(wide.width, 0.88f * side));
        assert(near(wide.x + wide.width * 0.5f, 100.0f) && near(tall.y + tall.height * 0.5f, 100.0f));
    }
    /* A payload without extents fills the cell, as before. */
    Rectangle old = instance_map_node_rect(&graph->nodes[2], (Vector2){ 0, 0 }, 50.0f);
    assert(near(old.width, 50.0f) && near(old.height, 50.0f));

    /* A cell of the map lands where it is on the map, whatever the card shape. */
    Rectangle tall = instance_map_node_rect(&graph->nodes[1], (Vector2){ 0, 0 }, 100.0f);
    Vector2 corner = instance_map_cell_to_card(tall, &graph->nodes[1], 32.0f, 64.0f);
    assert(near(corner.x, tall.x + tall.width) && near(corner.y, tall.y + tall.height));

    Rectangle bounds = { 0, 0, 200, 160 };
    Vector2 player = { 16, 8 };
    Rectangle current = instance_map_local_rect(&graph->nodes[0], bounds, player, 1.0f);
    Vector2 marker = instance_map_cell_to_card(current, &graph->nodes[0], player.x, player.y);
    assert(near(current.width, 200.0f) && near(current.height, 100.0f));
    assert(near(marker.x, 50.0f) && near(marker.y, 55.0f));
    current = instance_map_local_rect(&graph->nodes[0], bounds, (Vector2){ 32, 16 }, 2.0f);
    marker = instance_map_cell_to_card(current, &graph->nodes[0], 32, 16);
    assert(near(marker.x, 100.0f) && near(marker.y, 80.0f));
    current = instance_map_local_rect(&graph->nodes[0], bounds, (Vector2){ 0, 0 }, 3.0f);
    assert(near(current.x, bounds.x) && near(current.y, bounds.y));
    current = instance_map_local_rect(&graph->nodes[1], bounds, (Vector2){ 32, 64 }, 3.0f);
    assert(near(current.x + current.width, bounds.width) && near(current.y + current.height, bounds.height));
    assert(near(current.height / current.width, 2.0f));

    /* An exit points from its map to the map it leads to; a link inside one map has no direction. */
    assert(3 == graph->edge_count && !graph->edges[0].intra && graph->edges[1].intra);
    Vector2 east = instance_map_exit_direction(graph, &graph->edges[0]);
    Vector2 inside = instance_map_exit_direction(graph, &graph->edges[1]);
    Vector2 west = instance_map_exit_direction(graph, &graph->edges[2]);
    assert(near(east.x, 1.0f) && near(east.y, 0.0f));
    assert(near(inside.x, 0.0f) && near(inside.y, 0.0f));
    assert(near(west.x, -1.0f) && near(west.y, 0.0f));

    Rectangle fit = ui_rect_contain((Rectangle){ 0, 0, 100, 50 }, 64, 64);
    assert(near(fit.width, 50.0f) && near(fit.height, 50.0f) && near(fit.x, 25.0f) && near(fit.y, 0.0f));
    fit = ui_rect_contain((Rectangle){ 10, 10, 40, 80 }, 300, 100);
    assert(near(fit.width / fit.height, 3.0f) && near(fit.width, 40.0f) && near(fit.y + fit.height * 0.5f, 50.0f));

    /* Maps with world origins: a, b, c in a row, d under a, e apart, f on a layout cell. */
    instance_map_data_close();
    instance_map_data_open();
    response = (FetchResponse){ .asset_id = s_asset_id, .success = true, .data = (void*)SPATIAL, .size = strlen(SPATIAL) };
    s_callback(&response);
    graph = instance_map_data_graph();
    assert(IMAP_DATA_READY == instance_map_data_state() && 6 == graph->node_count && 2 == graph->edge_count);
    const ImapNode *a = &graph->nodes[0], *b = &graph->nodes[1], *c = &graph->nodes[2];
    const ImapNode *d = &graph->nodes[3], *e = &graph->nodes[4], *f = &graph->nodes[5];
    assert(a->placed && near(a->world.x, 0.44f) && near(a->world.y, 0.44f) && !f->placed);
    assert(IMAP_EDGE_RIGHT == instance_map_shared_edges(a, b) && IMAP_EDGE_LEFT == instance_map_shared_edges(b, a));
    assert(IMAP_EDGE_BOTTOM == instance_map_shared_edges(a, d) && IMAP_EDGE_RIGHT == instance_map_shared_edges(b, c));
    assert(0 == instance_map_shared_edges(a, e) && 0 == instance_map_shared_edges(a, f));
    assert(IMAP_EDGE_LEFT == instance_map_shared_edges(c, b));
    /* A map half as wide under another shares part of an edge only: no join. */
    ImapNode upper = { .placed = true, .world = { 0.44f, 0.44f }, .extent_w = 0.88f, .extent_h = 0.88f };
    ImapNode lower = { .placed = true, .world = { 0.22f, 1.32f }, .extent_w = 0.44f, .extent_h = 0.88f };
    assert(0 == instance_map_shared_edges(&upper, &lower) && 0 == instance_map_shared_edges(&lower, &upper));
    assert((IMAP_EDGE_RIGHT | IMAP_EDGE_BOTTOM) == instance_map_joined_edges(graph, 0));

    Vector2 centers[IMAP_MAX_NODES], again[IMAP_MAX_NODES];
    Vector2 span = instance_map_layout(graph, 0.025f, centers);
    instance_map_layout(graph, 0.025f, again);
    for (int i = 0; graph->node_count > i; i++) assert(near(centers[i].x, again[i].x) && near(centers[i].y, again[i].y));
    /* Joined maps meet with no gap; the map apart keeps its distance; the unplaced map sits beside. */
    assert(near(centers[1].x - centers[0].x, 0.88f) && near(centers[3].y - centers[0].y, 0.88f));
    assert(near(centers[2].x - centers[1].x, 0.66f));
    for (int i = 0; graph->node_count > i; i++)
        for (int j = i + 1; graph->node_count > j; j++) {
            const ImapNode *p = &graph->nodes[i], *q = &graph->nodes[j];
            assert(fabsf(centers[i].x - centers[j].x) >= (p->extent_w + q->extent_w) * 0.5f - 0.001f ||
                   fabsf(centers[i].y - centers[j].y) >= (p->extent_h + q->extent_h) * 0.5f - 0.001f);
        }
    assert(centers[5].x - f->extent_w * 0.5f >= centers[4].x + e->extent_w * 0.5f + 0.025f - 0.001f);
    assert(near(span.y, (100.0f + 64.0f) * 0.88f / 64.0f));
    /* The rectangles of joined cards share their edge exactly. */
    Rectangle ra = instance_map_node_rect(a, Vector2Scale(centers[0], 200.0f), 200.0f);
    Rectangle rb = instance_map_node_rect(b, Vector2Scale(centers[1], 200.0f), 200.0f);
    assert(near(ra.x + ra.width, rb.x) && near(ra.y, rb.y) && near(ra.height, rb.height));
    /* An exit between placed maps points along their places. */
    Vector2 placed_east = instance_map_exit_direction(graph, &graph->edges[0]);
    assert(near(placed_east.x, 1.0f) && near(placed_east.y, 0.0f));

    /* Two maps of one name tell themselves apart by their codes. */
    char label[IMAP_NAME_MAX];
    instance_map_node_label(graph, 0, label, sizeof label);
    assert(0 == strcmp(label, "Gate (a)"));
    instance_map_node_label(graph, 2, label, sizeof label);
    assert(0 == strcmp(label, "Cliff"));
    puts("instance_map_geometry_test OK");
    return 0;
}
