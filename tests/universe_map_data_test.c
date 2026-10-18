/* The data layer of the Universe view: it parses the universe of this world,
 * marks this world, offers TRAVEL only along a route to a world that takes
 * sessions, reads the engine again while open, and drops a late response. */

#include "data/universe_map_data.h"
#include "data/engine_client.h"
#include "sim/game_state.h"
#include "utils.h"

#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

GameState g_game_state;

static FetchCompletedCb s_callback;
static char s_asset_id[64];
static char s_url[256];
static int  s_requests;
static int  s_forgotten;

void fetch_request_start(const char* id, const char* url, FetchCompletedCb callback) {
    copy_str(s_asset_id, sizeof s_asset_id, id);
    copy_str(s_url, sizeof s_url, url);
    s_callback = callback;
    s_requests++;
}
void fetch_forget(const char* url) { s_forgotten++; }
void fetch_data_release(void* data) {}

static const char* UNIVERSE =
    "{\"status\":\"success\",\"data\":{\"code\":\"cyberia\",\"name\":\"Cyberia\",\"instances\":["
    "{\"instanceCode\":\"root\",\"name\":\"Violet Strata\",\"previewUrl\":\"/world-preview.png\",\"thumbnailUrl\":\"/world-thumbnail.png\",\"x\":0,\"y\":0,\"served\":true,\"status\":\"available\",\"url\":\"https://server.example.com\"},"
    "{\"instanceCode\":\"test\",\"name\":\"Test\",\"x\":2,\"y\":-1,\"w\":1,\"h\":0.5,\"served\":true,\"status\":\"available\",\"url\":\"https://server.example.com/test\","
    "\"path\":\"/test\",\"build\":\"0123456789abcdef\"},"
    "{\"instanceCode\":\"closing\",\"x\":0,\"y\":3,\"served\":true,\"status\":\"draining\",\"url\":\"https://server.example.com/closing\","
    "\"path\":\"/closing\",\"build\":\"fedcba9876543210\"},"
    "{\"instanceCode\":\"dark\",\"x\":-2,\"y\":1,\"served\":true,\"status\":\"offline\",\"url\":\"\"},"
    "{\"instanceCode\":\"unreleased\",\"x\":5,\"y\":5,\"served\":false,\"status\":\"available\",\"url\":\"https://server.example.com/u\"},"
    "{\"instanceCode\":\"island\",\"x\":9,\"y\":9,\"served\":true,\"status\":\"available\",\"url\":\"https://server.example.com/i\"}],"
    "\"links\":["
    "{\"sourceInstanceCode\":\"root\",\"targetInstanceCode\":\"test\",\"bidirectional\":true},"
    "{\"sourceInstanceCode\":\"closing\",\"targetInstanceCode\":\"root\",\"bidirectional\":true},"
    "{\"sourceInstanceCode\":\"root\",\"targetInstanceCode\":\"dark\",\"bidirectional\":false},"
    "{\"sourceInstanceCode\":\"root\",\"targetInstanceCode\":\"unreleased\",\"bidirectional\":false},"
    "{\"sourceInstanceCode\":\"island\",\"targetInstanceCode\":\"root\",\"bidirectional\":false},"
    "{\"sourceInstanceCode\":\"root\",\"targetInstanceCode\":\"ghost\",\"bidirectional\":true}]}}";

static void respond(const char* body, bool success) {
    FetchResponse response = { .asset_id = s_asset_id, .success = success,
                               .data = (void*)body, .size = strlen(body) };
    s_callback(&response);
}

static int node_of(const char* code) {
    const UmapGraph* g = universe_map_data_graph();
    for (int i = 0; i < g->node_count; ++i)
        if (0 == strcmp(g->nodes[i].instance_code, code)) return i;
    return -1;
}

int main(void) {
    copy_str(g_game_state.instance_code, sizeof g_game_state.instance_code, "root");

    universe_map_data_open();
    assert(IMAP_DATA_LOADING == universe_map_data_state());
    assert(0 == strcmp("/api/v1/cyberia-universe/instance/root", s_url));
    respond(UNIVERSE, true);
    assert(IMAP_DATA_READY == universe_map_data_state());

    const UmapGraph* g = universe_map_data_graph();
    assert(6 == g->node_count && 5 == g->link_count); /* a link to a non-member is dropped */
    assert(0 == strcmp("Cyberia", g->name) && node_of("root") == g->current);
    assert(0 == strcmp("Violet Strata", g->nodes[node_of("root")].name));
    assert(0 == strcmp("/world-preview.png", g->nodes[node_of("root")].preview_url));
    assert(0 == strcmp("/world-thumbnail.png", g->nodes[node_of("root")].thumbnail_url));
    assert('\0' == g->nodes[node_of("test")].thumbnail_url[0]);
    /* A card takes the proportions of its preview; a payload without them is square. */
    assert(1.0f == g->nodes[node_of("test")].extent_w && 0.5f == g->nodes[node_of("test")].extent_h);
    assert(1.0f == g->nodes[node_of("root")].extent_w && 1.0f == g->nodes[node_of("root")].extent_h);
    assert(0 == strcmp("closing", g->nodes[node_of("closing")].name)); /* no name: the code */

    assert(UMAP_TRAVEL_HERE == universe_map_data_travel(node_of("root")));
    assert(UMAP_TRAVEL_OPEN == universe_map_data_travel(node_of("test")));
    assert(UMAP_TRAVEL_DRAINING == universe_map_data_travel(node_of("closing"))); /* a route back counts both ways */
    assert(UMAP_TRAVEL_OFFLINE == universe_map_data_travel(node_of("dark")));
    assert(UMAP_TRAVEL_NOT_SERVED == universe_map_data_travel(node_of("unreleased")));
    assert(UMAP_TRAVEL_NO_ROUTE == universe_map_data_travel(node_of("island"))); /* a one-way route into here */

    /* A world plays in this page only on the build of the server this client plays on. */
    assert(0 == strcmp("/test", g->nodes[node_of("test")].path));
    assert(!universe_map_data_in_place(node_of("test")));
    copy_str(g_game_state.server_build, sizeof g_game_state.server_build, "0123456789abcdef");
    assert(universe_map_data_in_place(node_of("test")));
    assert(!universe_map_data_in_place(node_of("closing")));
    assert(!universe_map_data_in_place(node_of("dark"))); /* no build: the launch bridge */

    /* Every read, the first one too, goes past the response cache: the view reads the engine again while open. */
    assert(1 == s_requests && 1 == s_forgotten);
    universe_map_data_update(UMAP_REFRESH_SECONDS * 0.5f);
    assert(1 == s_requests);
    universe_map_data_update(UMAP_REFRESH_SECONDS);
    assert(2 == s_requests && 2 == s_forgotten);
    respond("not json", false);
    assert(IMAP_DATA_READY == universe_map_data_state() && 6 == universe_map_data_graph()->node_count);
    assert(universe_map_data_stale());
    assert(UMAP_TRAVEL_STALE == universe_map_data_travel(node_of("test")));
    assert(!universe_map_data_in_place(node_of("test")));

    /* A response after the close changes nothing. */
    universe_map_data_update(UMAP_REFRESH_SECONDS);
    universe_map_data_close();
    int generation = universe_map_data_generation();
    respond(UNIVERSE, true);
    assert(generation == universe_map_data_generation());
    universe_map_data_update(UMAP_REFRESH_SECONDS * 2.0f);
    assert(3 == s_requests);

    universe_map_data_open();
    respond(UNIVERSE, true);
    assert(!universe_map_data_stale());
    universe_map_data_update(UMAP_REFRESH_SECONDS * 2.0f);
    assert(universe_map_data_stale());
    respond(UNIVERSE, true);
    assert(!universe_map_data_stale());
    universe_map_data_close();
    copy_str(g_game_state.instance_code, sizeof g_game_state.instance_code, "island");
    universe_map_data_open();
    assert(IMAP_DATA_LOADING == universe_map_data_state() && 0 == universe_map_data_graph()->node_count);
    respond("{\"status\":\"success\",\"data\":{\"code\":\"empty\",\"instances\":[],\"links\":[]}}", true);
    assert(IMAP_DATA_READY == universe_map_data_state() && 0 == universe_map_data_graph()->node_count);
    assert(UMAP_TRAVEL_NO_ROUTE == universe_map_data_travel(-1));
    universe_map_data_close();

    printf("universe_map_data_test OK\n");
    return 0;
}
