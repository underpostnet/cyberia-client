#ifndef CYBERIA_DATA_MAP_ROUTE_H
#define CYBERIA_DATA_MAP_ROUTE_H

/* map_route — the shortest known portal routes of the loaded graphs.
 *
 * A route is a chain of directed links, each from the node the last one leads
 * to. A portal always names the map it leads to; a random portal names no
 * arrival cell, so a route through it reaches the map and never a cell. Maps
 * change only through portals, and a link is never travelled back unless the
 * graph says so. Routes run breadth first in link order: the fewest links win,
 * and among equal routes the one whose links come first in the graph. */

/* Shortest routes from `origin` over `count` directed links `from[i]` → `to[i]`
 * between `nodes` nodes: `hops[n]` links to node n, -1 without a route, and
 * `entry[n]` the link a route enters n by, -1 at the origin or without a route.
 * An `origin` under 0 reaches nothing. */
void map_route_tree(int nodes, const int* from, const int* to, int count, int origin, int* hops, int* entry);

/* Builds the tree of the world graph from the player's map again when that map
 * or the graph changed. Call once per frame from each view that shows routes. */
void map_route_update(void);

/* Portals from the player's map to `node`; -1 without a known route. */
int map_route_hops(int node);

/* The portal a route from the player's map enters `node` by; -1 at that map or without a route. */
int map_route_entry(int node);

/* The first portal of the route from the player's map to `node`; -1 at that map or without a route. */
int map_route_first(int node);

/* Bumped every time the tree is built: the key of anything drawn from it. */
int map_route_revision(void);

#endif /* CYBERIA_DATA_MAP_ROUTE_H */
