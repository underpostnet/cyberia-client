#ifndef CYBERIA_UI_INSTANCE_MAP_DATA_H
#define CYBERIA_UI_INSTANCE_MAP_DATA_H

#include <raylib.h>
#include <stdbool.h>

/* instance_map_data — data layer for the expanded Instance Map modal.
 *
 * Independent from the gameplay AOI stream: talks only to the engine-cyberia
 * Instance Map REST endpoints through network/engine_client.
 *
 *   open  → GET /api/cyberia-instance/instance-map/:code/static   (once)
 *   open  → GET /api/cyberia-instance/instance-map/:code/dynamic  (~1/s)
 *   close → polling stops immediately; late responses are discarded.
 *
 * Live player position never travels through this API — engine-cyberia holds
 * no simulation state. The modal overlays g_game_state's predicted position.
 */

#define IMAP_MAX_NODES       48
#define IMAP_MAX_EDGES       128
#define IMAP_MAX_PROVIDERS   96
#define IMAP_MAX_PORTAL_POIS 192
#define IMAP_CODE_MAX        64
#define IMAP_NAME_MAX        64

typedef enum {
    IMAP_DATA_IDLE = 0,     /* modal closed, nothing fetched          */
    IMAP_DATA_LOADING,      /* static fetch in flight                 */
    IMAP_DATA_READY,        /* static graph parsed; polling dynamic   */
    IMAP_DATA_ERROR,        /* static fetch/parse failed              */
} ImapDataState;

typedef struct {
    char    map_code[IMAP_CODE_MAX];
    char    name[IMAP_NAME_MAX];
    char    preview_file_id[IMAP_CODE_MAX]; /* File id of the map's Object
                                             * Layer capture ("" = none)  */
    int     grid_x, grid_y;
    int     quest_provider_count;   /* static totals bound to this map */
    int     action_provider_count;
    int     portal_count;           /* edges touching this node        */
    Vector2 pos;                    /* layout position, graph units    */
} ImapNode;

typedef struct {
    int  source_node;
    int  target_node;
    bool intra;                     /* same-map edge (intra-* mode)    */
    char portal_mode[16];
    /* Endpoint cells on each node's map; -1 = random destination (the
     * rendered link anchors to the node centre with a vibration). */
    int  source_cell_x, source_cell_y;
    int  target_cell_x, target_cell_y;
} ImapEdge;

typedef struct {
    char code[IMAP_CODE_MAX];
    char label[IMAP_NAME_MAX];
    int  node;
    int  cell_x, cell_y;
    /* dynamic (poll-refreshed) */
    bool active;
    bool acceptable;                /* quest providers: never started  */
} ImapProvider;

/* Portal landmark on a node's map: an edge endpoint with a known cell
 * (init spawn position). Random-cell endpoints carry no POI. */
typedef struct {
    int  node;
    int  cell_x, cell_y;
    bool intra;                     /* same-map portal                 */
} ImapPortalPoi;

typedef struct {
    char instance_code[IMAP_CODE_MAX];
    char name[IMAP_NAME_MAX];

    ImapNode nodes[IMAP_MAX_NODES];
    int      node_count;

    ImapEdge edges[IMAP_MAX_EDGES];
    int      edge_count;

    ImapProvider quest_providers[IMAP_MAX_PROVIDERS];
    int          quest_provider_count;

    ImapProvider action_providers[IMAP_MAX_PROVIDERS];
    int          action_provider_count;

    ImapPortalPoi portal_pois[IMAP_MAX_PORTAL_POIS];
    int           portal_poi_count;
} ImapGraph;

/* Begin the static fetch and enable dynamic polling. Requires the metadata
 * message to have delivered g_game_state.instance_code; otherwise the state
 * goes straight to IMAP_DATA_ERROR. */
void instance_map_data_open(void);

/* Stop polling immediately. In-flight responses are discarded on arrival. */
void instance_map_data_close(void);

/* Drive the ~1/s dynamic poll while open. Call once per frame. */
void instance_map_data_update(float dt);

ImapDataState    instance_map_data_state(void);
const ImapGraph* instance_map_data_graph(void);

/* Node index for a gameplay map code, -1 when absent. */
int instance_map_data_find_node(const char* map_code);

/* Bumped every time a static graph is (re)parsed — layout invalidation key. */
int instance_map_data_generation(void);

/* Count of dynamically-active quest providers on one node. */
int instance_map_data_node_active_quests(int node);
int instance_map_data_node_active_actions(int node);

#endif /* CYBERIA_UI_INSTANCE_MAP_DATA_H */
