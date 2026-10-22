/* Portal routes: the fewest portals from the player's map, among equal routes
 * the one whose portals come first; a portal is travelled only the way it
 * leads; a random portal reaches its map; a map nothing leads to has no route;
 * the player's own map needs none. The tree is built again only when the
 * player's map or the world graph changes. */

#include "data/map_route.h"
#include "data/engine_client.h"
#include "data/instance_map_data.h"
#include "data/quest_cache.h"
#include "sim/game_state.h"
#include "sim/local_player.h"
#include "utils.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

GameState g_game_state;
LocalPlayer g_local_player;

static FetchCompletedCb s_callback;
static char s_asset_id[64];
void fetch_request_start(const char* id, const char* url, FetchCompletedCb callback) {
    copy_str(s_asset_id, sizeof s_asset_id, id);
    s_callback = callback;
}
void fetch_forget(const char* url) {}
void fetch_data_release(void* data) {}
void audio_event(const char* logic_event_id) {}
const QuestMetadataEntry* quest_cache_get(const char* code) { return NULL; }

#define NODE(code, x)                                                                                    \
    "{\"mapCode\":\"" code "\",\"name\":\"" code "\",\"w\":0.5,\"h\":0.5,\"gridX\":16,\"gridY\":16,"   \
    "\"position\":{\"x\":" #x ",\"y\":0}}"
#define EDGE(from, to, cell)                                                                             \
    "{\"sourceMapCode\":\"" from "\",\"sourceCellX\":1,\"sourceCellY\":1,\"targetMapCode\":\"" to   \
    "\",\"targetCellX\":" #cell ",\"targetCellY\":" #cell "}"

/* a → b → c and a → d → c, both two portals; d takes a random cell; c leads back to a only;
 * e leads to a and nothing leads to e; b also leads inside itself. */
static const char* WORLD =
    "{\"status\":\"success\",\"data\":{\"instanceCode\":\"test\",\"nodes\":[" NODE("a", -2) "," NODE("b", -1) ","
    NODE("c", 0) "," NODE("d", 1) "," NODE("e", 2) "],\"edges\":[" EDGE("a", "b", 2) "," EDGE("b", "c", 2) ","
    EDGE("a", "d", -1) "," EDGE("d", "c", 2) "," EDGE("c", "a", 2) "," EDGE("e", "a", 2) "," EDGE("b", "b", 3)
    "],\"presencePois\":[]}}";

static void deliver(void) {
    FetchResponse response = { .asset_id = s_asset_id, .success = true, .data = (void*)WORLD, .size = strlen(WORLD) };
    s_callback(&response);
    assert(IMAP_DATA_READY == instance_map_data_state());
}

static int node(const char* code) { return instance_map_data_find_node(code); }

static void stand_on(const char* code) {
    copy_str(g_local_player.map_code, sizeof g_local_player.map_code, code);
    map_route_update();
}

int main(void) {
    copy_str(g_game_state.instance_code, sizeof g_game_state.instance_code, "test");
    instance_map_data_open();
    /* Before the world map loads, no map has a route. */
    stand_on("a");
    assert(-1 == map_route_hops(0));
    deliver();
    stand_on("a");

    /* The player's map needs no route. */
    assert(0 == map_route_hops(node("a")) && -1 == map_route_entry(node("a")) && -1 == map_route_first(node("a")));
    /* A direct route: one portal. */
    assert(1 == map_route_hops(node("b")) && 0 == map_route_entry(node("b")) && 0 == map_route_first(node("b")));
    /* Two routes of two portals: the one whose portals come first. */
    assert(2 == map_route_hops(node("c")) && 1 == map_route_entry(node("c")) && 0 == map_route_first(node("c")));
    /* A random portal reaches its map. */
    assert(1 == map_route_hops(node("d")) && 2 == map_route_entry(node("d")));
    /* A map nothing leads to has no route, though it leads here. */
    assert(-1 == map_route_hops(node("e")) && -1 == map_route_entry(node("e")) && -1 == map_route_first(node("e")));

    /* A portal leads one way: from b, a lies two portals away, through c. */
    int revision = map_route_revision();
    map_route_update();
    assert(revision == map_route_revision());
    stand_on("b");
    assert(revision != map_route_revision());
    assert(2 == map_route_hops(node("a")) && 4 == map_route_entry(node("a")) && 1 == map_route_first(node("a")));
    assert(3 == map_route_hops(node("d")) && 1 == map_route_first(node("d")));

    /* A world rebuild builds the tree again. */
    revision = map_route_revision();
    instance_map_data_refresh();
    deliver();
    map_route_update();
    assert(revision != map_route_revision() && 2 == map_route_hops(node("a")));

    /* The same search over any directed links: a ring 0 → 1 → 2 → 0 with a shortcut 0 → 2. */
    const int from[] = { 0, 1, 2, 0 }, to[] = { 1, 2, 0, 2 };
    int hops[3], entry[3];
    map_route_tree(3, from, to, 4, 1, hops, entry);
    assert(0 == hops[1] && 1 == hops[2] && 2 == hops[0] && 2 == entry[0] && 1 == entry[2] && -1 == entry[1]);
    map_route_tree(3, from, to, 4, -1, hops, entry);
    assert(-1 == hops[0] && -1 == hops[1] && -1 == hops[2]);

    puts("map_route_test OK");
    return 0;
}
