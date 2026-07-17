#ifndef CYBERIA_UI_INSTANCE_MAP_DATA_H
#define CYBERIA_UI_INSTANCE_MAP_DATA_H

#include <stdbool.h>
#include <stdint.h>

/* instance_map_data — data layer for the expanded Instance Map modal.
 *
 * Independent from the gameplay AOI stream: talks only to the engine-cyberia
 * Instance Map REST endpoints through network/engine_client.
 *
 *   open  → GET /api/cyberia-instance/instance-map/:code/static   (once)
 *   open  → GET /api/cyberia-instance/instance-map/:code/dynamic  (~1/s)
 *   close → polling stops immediately; late responses are discarded.
 *
 * Static POIs carry authored presence, baseline ObjectLayer stats, and
 * capability membership. Live player position and stats remain client-side.
 */

#define IMAP_MAX_NODES       48
#define IMAP_MAX_EDGES       128
#define IMAP_MAX_PRESENCE_POIS 512
#define IMAP_CODE_MAX        64
#define IMAP_NAME_MAX        64

enum {
    IMAP_CAPABILITY_ACTION = 1u << 0,
    IMAP_CAPABILITY_QUEST  = 1u << 1,
};

typedef enum {
    IMAP_PRESENCE_NONE = 0,
    IMAP_PRESENCE_PASSIVE,
    IMAP_PRESENCE_HOSTILE,
    IMAP_PRESENCE_RESOURCE,
    IMAP_PRESENCE_PORTAL,
    IMAP_PRESENCE_PORTAL_RANDOM,
} ImapPresenceStatus;

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
    int     grid_col, grid_row;     /* packed instance-map tile        */
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
    int  node;
    int  cell_x, cell_y;
    int  stats_sum;                 /* living presence only; 0 for portals */
    bool show_stats_value;
    ImapPresenceStatus presence_status;
    uint8_t capabilities;
    bool action_active;
    bool quest_active;
    bool quest_acceptable;
} ImapPresencePoi;

typedef struct {
    char instance_code[IMAP_CODE_MAX];
    char name[IMAP_NAME_MAX];

    ImapNode nodes[IMAP_MAX_NODES];
    int      node_count;
    int      grid_cols, grid_rows;

    ImapEdge edges[IMAP_MAX_EDGES];
    int      edge_count;

    ImapPresencePoi presence_pois[IMAP_MAX_PRESENCE_POIS];
    int             presence_poi_count;
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
