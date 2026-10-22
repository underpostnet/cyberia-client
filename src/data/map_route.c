#include "map_route.h"

#include "instance_map_data.h"
#include "sim/local_player.h"
#include "utils.h"

#include <assert.h>
#include <stdbool.h>
#include <string.h>

void map_route_tree(int nodes, const int* from, const int* to, int count, int origin, int* hops, int* entry) {
    assert(IMAP_MAX_NODES >= nodes);
    for (int i = 0; nodes > i; i++) hops[i] = entry[i] = -1;
    if (0 > origin) return;
    int queue[IMAP_MAX_NODES], head = 0, tail = 0;
    hops[origin] = 0;
    queue[tail++] = origin;
    while (tail > head) {
        int node = queue[head++];
        for (int l = 0; count > l; l++) {
            if (node != from[l] || 0 <= hops[to[l]]) continue;
            hops[to[l]] = hops[node] + 1;
            entry[to[l]] = l;
            queue[tail++] = to[l];
        }
    }
}

static int  s_hops[IMAP_MAX_NODES], s_entry[IMAP_MAX_NODES];
static int  s_revision;
static int  s_built_generation = -1;
static bool s_built_ready;
static char s_built_map[IMAP_CODE_MAX];

void map_route_update(void) {
    bool ready = IMAP_DATA_READY == instance_map_data_state();
    if (s_built_generation == instance_map_data_generation() && s_built_ready == ready &&
        0 == strcmp(s_built_map, g_local_player.map_code))
        return;
    s_built_generation = instance_map_data_generation();
    s_built_ready = ready;
    copy_str(s_built_map, sizeof s_built_map, g_local_player.map_code);
    s_revision++;

    for (int i = 0; IMAP_MAX_NODES > i; i++) s_hops[i] = s_entry[i] = -1;
    const ImapGraph* gr = instance_map_data_graph();
    int from[IMAP_MAX_EDGES], to[IMAP_MAX_EDGES];
    for (int e = 0; gr->edge_count > e; e++) {
        from[e] = gr->edges[e].source_node;
        to[e] = gr->edges[e].target_node;
    }
    map_route_tree(gr->node_count, from, to, gr->edge_count,
                   ready ? instance_map_data_find_node(g_local_player.map_code) : -1, s_hops, s_entry);
}

int map_route_hops(int node) { return s_hops[node]; }
int map_route_entry(int node) { return s_entry[node]; }

int map_route_first(int node) {
    const ImapGraph* gr = instance_map_data_graph();
    int first = -1;
    for (int e = s_entry[node]; 0 <= e; e = s_entry[gr->edges[e].source_node]) first = e;
    return first;
}

int map_route_revision(void) { return s_revision; }
