#ifndef CYBERIA_DATA_UNIVERSE_MAP_DATA_H
#define CYBERIA_DATA_UNIVERSE_MAP_DATA_H

#include "instance_map_data.h"

#include <stdbool.h>

/* universe_map_data — data layer of the Universe view of the expanded map.
 *
 * One engine-cyberia REST read, never the AOI stream:
 *
 *   open  → GET /api/v1/cyberia-universe/instance/:code, again every
 *           UMAP_REFRESH_SECONDS while open, so the availability stays fresh.
 *   close → a late response is discarded.
 *
 * The response holds the topology of the universe of this world, the content
 * name of each member and the availability of its servers. It holds no member
 * content: a preview loads only when its card is drawn.
 */

#define UMAP_MAX_NODES       32
#define UMAP_MAX_LINKS       64
#define UMAP_REFRESH_SECONDS 20.0f

typedef enum {
    UMAP_OFFLINE = 0,   /* no live server                  */
    UMAP_DRAINING,      /* live servers take no new session */
    UMAP_AVAILABLE,     /* a live server takes sessions     */
} UmapStatus;

typedef struct {
    char       instance_code[IMAP_CODE_MAX];
    char       name[IMAP_NAME_MAX];
    char       description[256];
    int        map_count;
    char       preview_url[IMAP_URL_MAX];
    char       thumbnail_url[IMAP_URL_MAX];
    char       url[IMAP_URL_MAX];   /* public endpoint, "" when offline */
    char       path[IMAP_CODE_MAX]; /* path of the world on the client, "" at the root */
    char       build[IMAP_CODE_MAX];/* binary its servers share, "" when unknown or mixed */
    int        layout_x, layout_y;  /* cell of the universe layout      */
    float      extent_w, extent_h;  /* size of its card in a cell, the
                                     * proportions of its preview      */
    bool       served;              /* the served content holds it      */
    UmapStatus status;
    ImapEntrySpawn entry;           /* where a new player of it starts  */
} UmapNode;

typedef struct {
    int  source, target;
    bool bidirectional;
} UmapLink;

typedef struct {
    char     code[IMAP_CODE_MAX];
    char     name[IMAP_NAME_MAX];
    UmapNode nodes[UMAP_MAX_NODES];
    int      node_count;
    UmapLink links[UMAP_MAX_LINKS];
    int      link_count;
    int      current;               /* node of this world, -1 if absent */
} UmapGraph;

/* Why a player cannot travel to a node. */
typedef enum {
    UMAP_TRAVEL_OPEN = 0,
    UMAP_TRAVEL_HERE,
    UMAP_TRAVEL_NO_ROUTE,
    UMAP_TRAVEL_NOT_SERVED,
    UMAP_TRAVEL_OFFLINE,
    UMAP_TRAVEL_DRAINING,
    UMAP_TRAVEL_STALE,
} UmapTravel;

void universe_map_data_open(void);
void universe_map_data_close(void);
/* After a failed fetch: fetch again now, ahead of the refresh period. */
void universe_map_data_retry(void);

/* Refreshes the availability while open. Call once per frame. */
void universe_map_data_update(float dt);

ImapDataState    universe_map_data_state(void);
const UmapGraph* universe_map_data_graph(void);

/* Bumped every time a response is parsed. */
int universe_map_data_generation(void);
bool universe_map_data_stale(void);

/* Whether a player of this world can travel to a node now, and why not. */
UmapTravel universe_map_data_travel(int node);

/* Whether this client can play a node without leaving the page: the servers
 * of that world run the build of the server it plays now. */
bool universe_map_data_in_place(int node);

#endif /* CYBERIA_DATA_UNIVERSE_MAP_DATA_H */
