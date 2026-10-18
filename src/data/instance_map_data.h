#ifndef CYBERIA_UI_INSTANCE_MAP_DATA_H
#define CYBERIA_UI_INSTANCE_MAP_DATA_H

#include <raylib.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* instance_map_data — data layer for the expanded Instance Map modal.
 *
 * Independent from the gameplay AOI stream: talks only to the engine-cyberia
 * Instance Map REST endpoint through network/engine_client.
 *
 *   open  → GET /api/v1/cyberia-instance/instance-map/:code/static   (once)
 *   close → a late response is discarded.
 *
 * Each node sits on the cell of the authored graph layout the engine resolves:
 * x grows right, y grows down. The layout is view data; portals keep their own
 * cells on each map. A map with a world origin has a place in its world
 * instead: maps whose places share a whole edge join there.
 *
 * Static POIs carry authored presence, the quests and actions each one
 * provides, and the prerequisites of each quest. A POI is active from the own
 * quest state: a quest while it runs, or once its prerequisites are complete;
 * an action while it carries a shop, a craft or a vault, or fronts such a
 * quest. Live player position and stats remain client-side.
 */

#define IMAP_MAX_NODES       48
#define IMAP_MAX_EDGES       128
#define IMAP_MAX_PRESENCE_POIS 512
#define IMAP_CODE_MAX        64
#define IMAP_NAME_MAX        64
#define IMAP_URL_MAX         160
#define IMAP_MAX_PROVIDERS   512
#define IMAP_MAX_LINKS       8

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
    IMAP_DATA_READY,        /* static graph parsed                    */
    IMAP_DATA_ERROR,        /* static fetch/parse failed              */
} ImapDataState;

typedef struct {
    char    map_code[IMAP_CODE_MAX];
    char    name[IMAP_NAME_MAX];
    char    preview_url[IMAP_URL_MAX];      /* API path of the map's Object
                                             * Layer capture ("" = none)  */
    bool    placed;                 /* the map has a world origin      */
    Vector2 world;                  /* its centre there, in layout
                                     * cells at the scale of extent  */
    int     grid_x, grid_y;
    float   extent_w, extent_h;     /* size of the map in its layout
                                     * cell, in cells: one scale for
                                     * every map of the world        */
    int     quest_provider_count;   /* static totals bound to this map */
    int     action_provider_count;
    int     portal_count;           /* edges touching this node        */
    int     layout_x, layout_y;     /* cell of the graph layout        */
} ImapNode;

typedef struct {
    int  source_node;
    int  target_node;
    bool intra;                     /* same-map edge (intra-* mode)    */
    /* Endpoint cells on each node's map; -1 = random destination (the
     * rendered link anchors to the node centre with a vibration). */
    int  source_cell_x, source_cell_y;
    int  target_cell_x, target_cell_y;
} ImapEdge;

typedef struct {
    int  node;
    int  cell_x, cell_y;
    ImapPresenceStatus presence_status;
    uint8_t capabilities;
    bool action_active;
    bool quest_active;
} ImapPresencePoi;

/* One quest or action a POI provides. A quest links its prerequisites; an
 * action links the quests it fronts. */
typedef struct {
    int  poi;
    char code[IMAP_CODE_MAX];
    char links[IMAP_MAX_LINKS][IMAP_CODE_MAX];
    int  link_count;
    bool standalone;    /* action: a shop, a craft or a vault */
} ImapProvider;

typedef struct {
    char instance_code[IMAP_CODE_MAX];
    char name[IMAP_NAME_MAX];

    ImapNode nodes[IMAP_MAX_NODES];
    int      node_count;

    ImapEdge edges[IMAP_MAX_EDGES];
    int      edge_count;

    ImapPresencePoi presence_pois[IMAP_MAX_PRESENCE_POIS];
    int             presence_poi_count;

    ImapProvider quests[IMAP_MAX_PROVIDERS];
    int          quest_count;
    ImapProvider actions[IMAP_MAX_PROVIDERS];
    int          action_count;
} ImapGraph;

/* Begin the static fetch. Requires the metadata message to have delivered
 * g_game_state.instance_code; otherwise the state goes straight to
 * IMAP_DATA_ERROR. */
void instance_map_data_open(void);

/* A response that arrives after the close is discarded. */
void instance_map_data_close(void);

/* Mark the active POIs again when the own quest state changed. Call once per
 * frame. */
void instance_map_data_update(float dt);

ImapDataState    instance_map_data_state(void);
const ImapGraph* instance_map_data_graph(void);

/* Node index for a gameplay map code, -1 when absent. */
int instance_map_data_find_node(const char* map_code);

/* Bumped every time a static graph is (re)parsed — layout invalidation key. */
int instance_map_data_generation(void);

/* Count of active quest and action providers on one node. */
int instance_map_data_node_active_quests(int node);
int instance_map_data_node_active_actions(int node);

/* Marker colour for a presence status. One table for both maps — the instance
 * map and the minimap, POI markers and live markers alike. */
Color instance_map_presence_color(ImapPresenceStatus status);

/* Cell centre on a node card: `cell_x`/`cell_y` in the node's own grid space,
 * mapped into `card`. A node with no grid gives the card centre. */
Vector2 instance_map_cell_to_card(Rectangle card, const ImapNode* node,
                                  float cell_x, float cell_y);

/* The card of a node: its map at its extent, centred on `center`, where one
 * layout cell spans `cell_side` pixels. */
Rectangle instance_map_node_rect(const ImapNode* node, Vector2 center, float cell_side);
/* The active map in `bounds`: zoom 1 fits it whole; a closer zoom follows the
 * player and stops at the map edges. */
Rectangle instance_map_local_rect(const ImapNode* node, Rectangle bounds, Vector2 player, float zoom);

/* The edges of a card a neighbour covers whole. */
enum { IMAP_EDGE_LEFT = 1u << 0, IMAP_EDGE_TOP = 1u << 1, IMAP_EDGE_RIGHT = 1u << 2, IMAP_EDGE_BOTTOM = 1u << 3 };

/* The edges of `a` that the place of `b` covers whole: both maps placed, the
 * same length along the edge, and their places side by side. */
unsigned instance_map_shared_edges(const ImapNode* a, const ImapNode* b);
/* The edges of a node any other node covers. */
unsigned instance_map_joined_edges(const ImapGraph* graph, int index);

/* Each centre of the view, about the origin, and the span, in layout cells. A
 * placed map sits at its place; the other maps pack on their layout cells, a
 * gap right of the placed ones. */
Vector2 instance_map_layout(const ImapGraph* graph, float gap, Vector2* centers);

/* The name of a node, with its map code when another node has the same name. */
void instance_map_node_label(const ImapGraph* graph, int index, char* out, size_t size);

/* Unit direction, in layout cells, from the map of a portal to the map it
 * leads to, between their places when both have one; zero for a link inside
 * one map. */
Vector2 instance_map_exit_direction(const ImapGraph* graph, const ImapEdge* edge);

#endif /* CYBERIA_UI_INSTANCE_MAP_DATA_H */
