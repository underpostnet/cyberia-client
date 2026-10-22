#include "modal_instance_map.h"
#include "graph_layout.h"
#include "ease.h"
#include "map_detail.h"
#include "map_marker.h"

#include "data/instance_map_data.h"
#include "data/map_route.h"
#include "data/quest_target.h"
#include "data/universe_map_data.h"
#include "hud_minimap_overlay.h"
#include "modal.h"
#include "modal_map.h"
#include "session_gate.h"
#include "text.h"
#include "toolbar.h"
#include "ui_button.h"
#include "ui_icon.h"
#include "ui_rect.h"
#include "ui_tabs.h"

#include "sim/local_player.h"
#include "content/presentation_runtime.h"
#include "sim/game_state.h"
#include "input/input.h"
#include "view/local_player_view.h"
#include "data/engine_client.h"
#include "utils.h"
#include "world/world_types.h"

#include <assert.h>
#include <ctype.h>
#include <float.h>
#include <limits.h>
#include <math.h>
#include <raylib.h>
#include <raymath.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ── Presentation constants ────────────────────────────────────────────── */
#define IMAP_NODE_SIDE      88.0f
#define IMAP_TAG_CELL_PX    9.0f    /* px of one map cell where the player's tag shows */
#define IMAP_ZOOM_MIN       0.45f
#define IMAP_ZOOM_INIT      5.0f
#define IMAP_ZOOM_MAX_FLOOR 3.2f
#define IMAP_ZOOM_STEP      1.15f
#define IMAP_GROUP_ZOOM     2.5f    /* a tap on a group of marks zooms in by it */
#define IMAP_BADGE_W        12.0f   /* room of the count badge right of a group */
#define IMAP_DRAG_SLOP_PX   9.0f
#define IMAP_CAM_LAMBDA     11.0f   /* exp smoothing rate for pan/zoom      */
#define IMAP_FRAME_CELL_PX  8.0f    /* most px of one map cell when the world view opens */
#define IMAP_LOCATE_CELL_PX 14.0f   /* px of one map cell when the view shows a quest destination */
#define IMAP_LABEL_REPEAT_PX 200.0f /* a label never repeats the text of a placed label this close */
#define UMAP_WORLD_GAP      0.2f    /* room for the routes between worlds      */

/* Chrome: the header under the toolbar, the detail cards and the hint line. */
#define IMAP_HEADER_H       48.0f
#define IMAP_HEADER_PAD     8.0f
#define IMAP_TAB_W          128.0f
#define IMAP_HINT_H         20.0f
#define IMAP_CARD_W         300.0f
#define IMAP_CARD_MARGIN    20.0f
#define IMAP_CARD_BTN_H     36.0f

static const Color IMAP_GRID_LINE = { 60, 140, 190, 26 };
static const Color IMAP_EDGE_INTRA= { 170, 110, 255, 255 };
static const Color IMAP_NODE_LINE = { 90, 210, 250, 220 };
static const Color IMAP_SELECTED  = { 255, 210, 90, 255 };
static const Color IMAP_HOVER     = { 120, 220, 255, 255 };
static const Color IMAP_RIM       = { 70, 170, 220, 210 };
static const Color IMAP_RIM_SHADOW= { 2, 4, 10, 235 };
static const Color IMAP_PLATE     = { 18, 52, 104, 238 };
static const Color IMAP_PLATE_EDGE= { 96, 176, 255, 255 };
static const Color IMAP_PLATE_GLOW= { 160, 215, 255, 150 };
static const Color IMAP_PLATE_TEXT= { 236, 246, 255, 255 };
static const Color IMAP_TEXT      = { 205, 225, 245, 235 };
static const Color IMAP_TEXT_DIM  = { 130, 150, 180, 190 };
static const Color UMAP_COLOR_ONLINE  = { 110, 255, 150, 255 };
static const Color UMAP_COLOR_CLOSING = { 255, 200, 80, 255 };
static const Color UMAP_COLOR_OFFLINE = { 150, 155, 170, 255 };

/* The two views of the container: the maps of this world, and the worlds of its universe. */
typedef enum { VIEW_WORLD = 0, VIEW_UNIVERSE, VIEW_COUNT } MapView;

static const UITab IMAP_VIEW_TABS[VIEW_COUNT] = {
    [VIEW_WORLD]    = { "World", "map", true, 0 },
    [VIEW_UNIVERSE] = { "Universe", "polyhedron", true, 0 },
};

typedef struct {
    bool  open;

    Rectangle panel;      /* last drawn panel rect          */

    /* Camera: pan is the graph-origin offset from the panel centre, px. */
    Vector2 pan, pan_target;
    float   zoom, zoom_target;

    /* Pointer gesture state (mouse or single touch). */
    bool    pointer_was_down;
    bool    pressed;      /* press began inside the panel   */
    bool    dragging;
    Vector2 press_pos;
    Vector2 last_pointer;

    /* Pinch state (two touches). */
    bool    pinching;
    float   pinch_dist;
    Vector2 pinch_mid;

    MapView view;
    bool    universe_open; /* the universe data was asked for  */
    bool    framed;        /* the open view fits its graph */
    bool    clear_pending; /* the selection still has to leave its panel */
    Vector2 centers[IMAP_MAX_NODES];     /* of the universe cards         */
    Vector2 span;                        /* of the open view, in layout units */
    int viewport_w, viewport_h;          /* the screen the view was framed for */
    int selected_node;    /* -1 = none, in the open view: the map or world being inspected */
    char selected_code[IMAP_CODE_MAX];
    MapDetail selected;   /* world view: the map, portal or place the card shows */
    bool touch;           /* the last press was a touch, so nothing shows a hover */
    bool locate_pending;  /* the followed objective waits for the world view to load */
    int layout_generation;
} ModalInstanceMap;

static ModalInstanceMap s_m = {0};

/* The detail card, its TRAVEL button, the RETRY button and the button back to
 * the followed objective as last drawn; zero when none shows. */
static Rectangle s_card, s_travel_btn, s_retry_btn, s_follow_btn;

/* The portals of the route on show, and the map and the route tree it was drawn for. */
static bool s_route_edge[IMAP_MAX_EDGES];
static int  s_route_node = -1, s_route_revision = -1;

/* The links of the route from this world to the selected world, each with the
 * way it is travelled, the jumps to each world, the first world on the way,
 * and the universe and selection they were drawn for. */
static bool s_umap_route[UMAP_MAX_LINKS];
static bool s_umap_reverse[UMAP_MAX_LINKS];
static int  s_umap_hops[UMAP_MAX_NODES];
static int  s_umap_next = -1;
static int  s_umap_key[3] = { -2, -2, -2 };

/* A mark of the world view as last drawn, and what a tap on it selects. A group
 * of `count` marks zooms in instead. Later marks lie on top. */
typedef struct {
    Rectangle rect;
    MapDetailKind kind;
    int index;
    int count;
} MapHit;

#define IMAP_HITS_MAX (IMAP_MAX_EDGES + IMAP_MAX_PRESENCE_POIS + 1)
static MapHit s_hits[IMAP_HITS_MAX];
static int    s_hit_count;

static const MapHit* hit_at(Vector2 at) {
    for (int i = s_hit_count - 1; 0 <= i; --i)
        if (CheckCollisionPointRec(at, s_hits[i].rect)) return &s_hits[i];
    return NULL;
}

static void add_hit(Rectangle rect, MapDetailKind kind, int index, int count) {
    if (IMAP_HITS_MAX > s_hit_count) s_hits[s_hit_count++] = (MapHit){ rect, kind, index, count };
}

/* ── Lifecycle ──────────────────────────────────────────────────────────── */

void modal_instance_map_init(void) {
    memset(&s_m, 0, sizeof(s_m));
    s_m.zoom = s_m.zoom_target = IMAP_ZOOM_INIT;
    s_m.selected_node = -1;
    s_m.selected = (MapDetail){ MAP_DETAIL_NONE, -1, -1 };
}

void modal_instance_map_cleanup(void) {
    if (s_m.open) {
        instance_map_data_close();
        if (s_m.universe_open) universe_map_data_close();
        input_gestures_set_blocked(false);
    }
    memset(&s_m, 0, sizeof(s_m));
}

bool modal_instance_map_is_open(void) { return s_m.open; }

static void reset_camera(void) {
    s_m.pan = s_m.pan_target = (Vector2){ 0, 0 };
    s_m.zoom = s_m.zoom_target = IMAP_ZOOM_INIT;
}

void modal_instance_map_toggle(void) {
    if (s_m.open) { modal_instance_map_close(); return; }
    hud_minimap_overlay_hide();
    s_m.open = true;
    s_m.view = VIEW_WORLD;
    s_m.universe_open = false;
    s_m.framed = false;
    s_m.selected_code[0] = '\0';
    s_m.selected_node = -1;
    s_m.selected = (MapDetail){ MAP_DETAIL_NONE, -1, -1 };
    s_m.pressed = s_m.dragging = s_m.pinching = s_m.locate_pending = false;
    s_card = s_travel_btn = s_retry_btn = s_follow_btn = (Rectangle){ 0 };
    s_hit_count = 0;
    reset_camera();
    instance_map_data_open();
    modal_map_set_expanded(true);      /* container morphs to full screen */
    input_gestures_set_blocked(true);  /* map owns pinch while expanded    */
    modal_opened();
}

void modal_instance_map_close(void) {
    if (!s_m.open) return;
    s_m.open = false;
    instance_map_data_close();      /* stops dynamic polling immediately */
    if (s_m.universe_open) universe_map_data_close();
    s_m.universe_open = false;
    modal_map_set_expanded(false);  /* container retracts to the readout */
    input_gestures_set_blocked(false);
    modal_closed();
}

/* ── Projection ─────────────────────────────────────────────────────────── */

static float node_screen_side(void) {
    return IMAP_NODE_SIDE * s_m.zoom;
}

static int view_node_count(void) {
    return VIEW_UNIVERSE == s_m.view ? universe_map_data_graph()->node_count
                                     : instance_map_data_graph()->node_count;
}

/* The engine places the maps; the client packs the universe cards. */
static void pack_view(void) {
    if (VIEW_UNIVERSE != s_m.view) {
        s_m.span = instance_map_data_graph()->span;
        return;
    }
    const UmapGraph* u = universe_map_data_graph();
    Vector2 cells[UMAP_MAX_NODES], sizes[UMAP_MAX_NODES];
    for (int i = 0; u->node_count > i; ++i) {
        const UmapNode* n = &u->nodes[i];
        cells[i] = (Vector2){ (float)n->layout_x, (float)n->layout_y };
        sizes[i] = (Vector2){ n->extent_w, n->extent_h };
    }
    s_m.span = graph_layout_pack(cells, sizes, u->node_count, UMAP_WORLD_GAP, s_m.centers);
}

/* The header strip under the toolbar. */
static Rectangle header_rect(void) {
    return (Rectangle){ s_m.panel.x, s_m.panel.y + toolbar_height(), s_m.panel.width, IMAP_HEADER_H };
}

static Rectangle close_rect(Rectangle header) {
    float side = IMAP_HEADER_H - 2.0f * IMAP_HEADER_PAD;
    return (Rectangle){ header.x + header.width - IMAP_HEADER_PAD - side, header.y + IMAP_HEADER_PAD, side, side };
}

/* The view tabs lead the header. A header in the toolbar band never starts left of the pinned toggle. */
static UITabs view_tabs(Rectangle header) {
    float x = header.x + IMAP_HEADER_PAD;
    if (TOOLBAR_H > header.y && x < toolbar_toggle_right()) x = toolbar_toggle_right();
    float room = close_rect(header).x - IMAP_HEADER_PAD - x;
    float width = fminf((float)VIEW_COUNT * IMAP_TAB_W + UI_TABS_GAP, room);
    return ui_tabs_row(VIEW_COUNT, x, header.y + IMAP_HEADER_PAD, width, IMAP_HEADER_H - 2.0f * IMAP_HEADER_PAD);
}

/* The screen the view frames its graph in: under the header, over the bottom margin. */
static Rectangle frame_area(void) {
    float top = toolbar_height() + IMAP_HEADER_H + 16.0f;
    return (Rectangle){ 20.0f, top, (float)GetScreenWidth() - 40.0f, (float)GetScreenHeight() - 36.0f - top };
}

/* The most pixels one layout cell spans with a span in the frame area. */
static float fit_side(Vector2 span) {
    Rectangle area = frame_area();
    return fminf(area.width / fmaxf(span.x, 1e-3f), area.height / fmaxf(span.y, 1e-3f));
}

/* A layout point on the screen. */
static Vector2 screen_point(Vector2 p) {
    Vector2 c = ui_rect_center(s_m.panel);
    float side = node_screen_side();
    return (Vector2){ c.x + s_m.pan.x + p.x * side, c.y + s_m.pan.y + p.y * side };
}

/* Screen px of one map cell: every mark of the world view takes its size from it. */
static float cell_px(void) {
    return node_screen_side() * instance_map_data_graph()->cell;
}

/* A map rounds each edge from its one value, so maps that touch meet on one pixel at every zoom. */
static Rectangle node_rect(int idx) {
    if (VIEW_UNIVERSE == s_m.view) {
        const UmapNode* u = &universe_map_data_graph()->nodes[idx];
        Vector2 p = screen_point(s_m.centers[idx]);
        float side = node_screen_side();
        return (Rectangle){ p.x - u->extent_w * side * 0.5f, p.y - u->extent_h * side * 0.5f,
                            u->extent_w * side, u->extent_h * side };
    }
    ImapBounds b = instance_map_data_graph()->nodes[idx].bounds;
    Vector2 min = screen_point((Vector2){ b.left, b.top });
    Vector2 max = screen_point((Vector2){ b.right, b.bottom });
    float left = roundf(min.x), top = roundf(min.y);
    return (Rectangle){ left, top, roundf(max.x) - left, roundf(max.y) - top };
}

static Vector2 node_center(int idx) {
    return ui_rect_center(node_rect(idx));
}

static int node_at_point(float x, float y) {
    int hit = -1;
    float best = 1e9f;
    for (int i = 0; i < view_node_count(); ++i) {
        Rectangle r = node_rect(i);
        if (r.x > x || r.x + r.width <= x || r.y > y || r.y + r.height <= y) continue;
        Vector2 p = node_center(i);
        float dx = x - p.x, dy = y - p.y;
        float d2 = dx * dx + dy * dy;
        if (d2 < best) { best = d2; hit = i; }
    }
    return hit;
}

/* ── Camera controls ────────────────────────────────────────────────────── */

/* The panel ends full screen: the closest zoom follows the screen, also while the panel grows. */
static float imap_zoom_max(void) {
    return fmaxf(IMAP_ZOOM_MAX_FLOOR, 4.0f * (float)GetScreenWidth() / IMAP_NODE_SIDE);
}

/* Zoom about an anchor point: the graph point under the anchor stays put. */
static void zoom_about(float factor, Vector2 anchor) {
    float nz = Clamp(s_m.zoom_target * factor, IMAP_ZOOM_MIN, imap_zoom_max());
    factor = nz / s_m.zoom_target;
    Vector2 c = ui_rect_center(s_m.panel);
    Vector2 rel = { anchor.x - c.x - s_m.pan_target.x,
                    anchor.y - c.y - s_m.pan_target.y };
    s_m.pan_target.x += rel.x * (1.0f - factor);
    s_m.pan_target.y += rel.y * (1.0f - factor);
    s_m.zoom_target = nz;
}

bool modal_instance_map_handle_wheel(float wheel_delta) {
    if (!s_m.open) return false;
    Vector2 mp = GetMousePosition();
    if (!CheckCollisionPointRec(mp, s_m.panel)) return false;
    zoom_about(wheel_delta > 0 ? IMAP_ZOOM_STEP : 1.0f / IMAP_ZOOM_STEP, mp);
    return true;
}

static int view_generation(void) {
    return VIEW_UNIVERSE == s_m.view ? universe_map_data_generation() : instance_map_data_generation();
}

/* Fits the graph under the header, up to IMAP_FRAME_CELL_PX per map cell or IMAP_ZOOM_INIT per world card. */
static void frame_view(void) {
    pack_view();
    Rectangle area = frame_area();
    float closest = VIEW_UNIVERSE == s_m.view ? IMAP_ZOOM_INIT
                                              : IMAP_FRAME_CELL_PX / (instance_map_data_graph()->cell * IMAP_NODE_SIDE);
    s_m.zoom = s_m.zoom_target = Clamp(fit_side(s_m.span) / IMAP_NODE_SIDE, IMAP_ZOOM_MIN, fmaxf(IMAP_ZOOM_MIN, closest));
    s_m.pan = s_m.pan_target = (Vector2){ 0.0f, area.y + area.height * 0.5f - (float)GetScreenHeight() * 0.5f };
    s_m.framed = true;
    s_m.viewport_w = GetScreenWidth();
    s_m.viewport_h = GetScreenHeight();
}

/* The code a node of the open view keeps across a refetch: a map code, or a world's instance code. */
static const char* view_node_code(int idx) {
    return VIEW_UNIVERSE == s_m.view ? universe_map_data_graph()->nodes[idx].instance_code
                                     : instance_map_data_graph()->nodes[idx].map_code;
}

/* Inspects a node of the open view, or nothing for -1. In the world view the card shows the map. */
static void select_node(int node) {
    s_m.clear_pending = node != s_m.selected_node;
    s_m.selected_node = node;
    copy_str(s_m.selected_code, sizeof s_m.selected_code, 0 <= node ? view_node_code(node) : "");
    s_m.selected = (MapDetail){ 0 <= node && VIEW_WORLD == s_m.view ? MAP_DETAIL_MAP : MAP_DETAIL_NONE, node, -1 };
}

/* Selects a portal, a place or the entry spawn, and inspects its map. */
static void select_mark(const MapHit* hit) {
    const ImapGraph* gr = instance_map_data_graph();
    select_node(MAP_DETAIL_PORTAL == hit->kind  ? gr->edges[hit->index].source_node
                : MAP_DETAIL_SPAWN == hit->kind ? instance_map_data_find_node(gr->entry_spawn.map_code)
                                                : gr->presence_pois[hit->index].node);
    s_m.clear_pending = true;
    s_m.selected = (MapDetail){ hit->kind, hit->index, -1 };
}

/* Shows the other view; the universe data loads the first time it shows. */
static void select_view(MapView view) {
    if (view == s_m.view) return;
    s_m.view = view;
    select_node(-1);
    s_m.framed = false;
    reset_camera();
    if (VIEW_UNIVERSE == view && !s_m.universe_open) {
        s_m.universe_open = true;
        universe_map_data_open();
    }
    s_m.layout_generation = view_generation();
    pack_view();
}

/* Leaves for the selected world when the universe allows it now; in this page
 * when the servers of that world run the build this client plays on. */
static void travel_to_selected(void) {
    if (UMAP_TRAVEL_OPEN != universe_map_data_travel(s_m.selected_node)) return;
    const UmapNode node = universe_map_data_graph()->nodes[s_m.selected_node];
    bool in_place = universe_map_data_in_place(s_m.selected_node);
    modal_instance_map_close();
    session_gate_travel(node.instance_code, node.path, node.name, in_place);
}

/* Moves the camera to a cell of a map, close enough to read the marks about it.
 * The panel ends full screen, so the camera aims at the screen while it grows. */
static void focus_cell(int node, int cell_x, int cell_y) {
    const ImapGraph* gr = instance_map_data_graph();
    const ImapBounds b = gr->nodes[node].bounds;
    Vector2 p = instance_map_cell_to_card((Rectangle){ b.left, b.top, b.right - b.left, b.bottom - b.top },
                                          &gr->nodes[node], cell_x + 0.5f, cell_y + 0.5f);
    s_m.zoom_target = Clamp(IMAP_LOCATE_CELL_PX / (IMAP_NODE_SIDE * gr->cell), IMAP_ZOOM_MIN, imap_zoom_max());
    float side = IMAP_NODE_SIDE * s_m.zoom_target;
    Vector2 middle = ui_rect_center(frame_area());
    Vector2 c = { (float)GetScreenWidth() * 0.5f, (float)GetScreenHeight() * 0.5f };
    s_m.pan_target = (Vector2){ middle.x - c.x - p.x * side, middle.y - c.y - p.y * side };
}

/* Follows `target` and shows its card, with its destination `destination` in focus close up. */
static void focus_destination(const QuestTarget* target, int destination) {
    if (NULL == target) return;
    quest_target_track(target, destination);
    if (0 < target->destination_count) {
        const ImapPresencePoi* poi = &instance_map_data_graph()->presence_pois[target->destinations[destination].poi];
        select_node(poi->node);
        focus_cell(poi->node, poi->cell_x, poi->cell_y);
    }
    s_m.selected = (MapDetail){ MAP_DETAIL_OBJECTIVE, -1, -1 };
    s_m.clear_pending = false;
}

/* The map a route shows to: the selected map, else the destination in focus of
 * the followed objective. -1 for none. */
static int route_node(void) {
    if (VIEW_WORLD != s_m.view) return -1;
    if (0 <= s_m.selected_node) return s_m.selected_node;
    const QuestTarget* followed = quest_target_tracked();
    if (NULL == followed || 0 == followed->destination_count) return -1;
    int poi = followed->destinations[quest_target_tracked_destination()].poi;
    return instance_map_data_graph()->presence_pois[poi].node;
}

/* The portals of the route from the player's map to route_node(), again only when either changed. */
static void refresh_route(void) {
    int node = route_node();
    if (node == s_route_node && map_route_revision() == s_route_revision) return;
    s_route_node = node;
    s_route_revision = map_route_revision();
    memset(s_route_edge, 0, sizeof s_route_edge);
    const ImapGraph* gr = instance_map_data_graph();
    for (int e = 0 <= node ? map_route_entry(node) : -1; 0 <= e; e = map_route_entry(gr->edges[e].source_node))
        s_route_edge[e] = true;
}

/* The route from this world to the selected world, again only when either or the universe changed. */
static void refresh_universe_route(void) {
    const UmapGraph* u = universe_map_data_graph();
    int key[3] = { u->current, s_m.selected_node, universe_map_data_generation() };
    if (0 == memcmp(key, s_umap_key, sizeof key)) return;
    memcpy(s_umap_key, key, sizeof key);
    int from[2 * UMAP_MAX_LINKS], to[2 * UMAP_MAX_LINKS], link[2 * UMAP_MAX_LINKS], count = 0;
    for (int i = 0; u->link_count > i; i++) {
        const UmapLink* l = &u->links[i];
        for (int way = 0; (l->bidirectional ? 2 : 1) > way; way++) {
            from[count] = 0 == way ? l->source : l->target;
            to[count] = 0 == way ? l->target : l->source;
            link[count++] = i;
        }
    }
    int entry[UMAP_MAX_NODES];
    map_route_tree(u->node_count, from, to, count, u->current, s_umap_hops, entry);
    memset(s_umap_route, 0, sizeof s_umap_route);
    s_umap_next = -1;
    for (int n = s_m.selected_node; 0 <= n && 0 <= entry[n]; n = from[entry[n]]) {
        s_umap_route[link[entry[n]]] = true;
        s_umap_reverse[link[entry[n]]] = u->links[link[entry[n]]].source != from[entry[n]];
        s_umap_next = n;
    }
}

void modal_instance_map_locate(void) {
    if (!s_m.open) modal_instance_map_toggle();
    select_view(VIEW_WORLD);
    s_m.locate_pending = true;
}

/* Release resolution: a clean (non-drag) press acts on what it landed on. The
 * header and the detail card take their own presses, never the map under them. */
static void resolve_click(Vector2 at) {
    Rectangle header = header_rect();
    if (CheckCollisionPointRec(at, close_rect(header))) {
        modal_instance_map_close();
        return;
    }
    UITabs tabs = view_tabs(header);
    int tab = ui_tabs_hit(&tabs, (int)at.x, (int)at.y);
    if (0 <= tab) {
        select_view((MapView)tab);
        return;
    }
    if (VIEW_UNIVERSE == s_m.view && 0 <= s_m.selected_node && CheckCollisionPointRec(at, s_travel_btn)) {
        travel_to_selected();
        return;
    }
    if (CheckCollisionPointRec(at, s_retry_btn)) {
        if (VIEW_UNIVERSE == s_m.view) universe_map_data_retry();
        else instance_map_data_retry();
        return;
    }
    if (CheckCollisionPointRec(at, header)) return;
    if (VIEW_WORLD == s_m.view && CheckCollisionPointRec(at, s_follow_btn)) {
        focus_destination(quest_target_tracked(), quest_target_tracked_destination());
        return;
    }
    if (CheckCollisionPointRec(at, s_card)) {
        int row = VIEW_WORLD == s_m.view ? map_detail_row_at(at) : -1;
        if (MAP_DETAIL_PAGE == row) {
            s_m.selected.item = map_detail_next_page();
        } else if (MAP_DETAIL_UNFOLLOW == row) {
            quest_target_untrack();
            select_node(-1);
        } else if (MAP_DETAIL_FOLLOW <= row) {
            int index = row - MAP_DETAIL_FOLLOW;
            const QuestTarget* target = quest_target_count() > index ? quest_target_get(index) : NULL;
            for (int d = 0; NULL != target && target->destination_count > d; d++)
                if (s_m.selected.index == target->destinations[d].poi) {
                    focus_destination(target, d);
                    break;
                }
        } else if (MAP_DETAIL_OBJECTIVE == s_m.selected.kind && 0 <= row) {
            focus_destination(quest_target_tracked(), row);
        } else if (MAP_DETAIL_BACK == row) {
            s_m.selected.item = -1;
        } else if (0 <= row) {
            s_m.selected.item = row;
        }
        return;
    }
    const MapHit* mark = VIEW_WORLD == s_m.view ? hit_at(at) : NULL;
    if (NULL != mark && 1 < mark->count) zoom_about(IMAP_GROUP_ZOOM, ui_rect_center(mark->rect));
    else if (NULL != mark) select_mark(mark);
    else select_node(node_at_point(at.x, at.y)); /* tapping empty space deselects */
}

static void track_pinch(void) {
    Vector2 a = GetTouchPosition(0);
    Vector2 b = GetTouchPosition(1);
    float   d = sqrtf((a.x - b.x) * (a.x - b.x) + (a.y - b.y) * (a.y - b.y));
    Vector2 mid = { (a.x + b.x) * 0.5f, (a.y + b.y) * 0.5f };
    if (!s_m.pinching) {
        s_m.pinching   = true;
        s_m.pinch_dist = d;
        s_m.pinch_mid  = mid;
        s_m.dragging   = false;
        s_m.pressed    = false;
        return;
    }
    if (s_m.pinch_dist > 1.0f && d > 1.0f) {
        zoom_about(d / s_m.pinch_dist, mid);
        s_m.pan_target.x += mid.x - s_m.pinch_mid.x;
        s_m.pan_target.y += mid.y - s_m.pinch_mid.y;
    }
    s_m.pinch_dist = d;
    s_m.pinch_mid  = mid;
}

static void track_pointer(void) {
    if (GetTouchPointCount() >= 2) { track_pinch(); return; }
    s_m.pinching = false;

    Vector2 p    = input_pointer_position();
    bool    down = input_pointer_down();

    if (down && !s_m.pointer_was_down) {
        s_m.touch     = 0 < GetTouchPointCount();
        s_m.pressed   = CheckCollisionPointRec(p, s_m.panel);
        s_m.dragging  = false;
        s_m.press_pos = p;
    } else if (down && s_m.pressed) {
        float dx = p.x - s_m.press_pos.x, dy = p.y - s_m.press_pos.y;
        if (!s_m.dragging && dx * dx + dy * dy > IMAP_DRAG_SLOP_PX * IMAP_DRAG_SLOP_PX)
            s_m.dragging = true;
        if (s_m.dragging) {
            /* 1:1 pan while the finger is down; interpolation handles release. */
            s_m.pan_target.x += p.x - s_m.last_pointer.x;
            s_m.pan_target.y += p.y - s_m.last_pointer.y;
            s_m.pan.x        += p.x - s_m.last_pointer.x;
            s_m.pan.y        += p.y - s_m.last_pointer.y;
        }
    } else if (!down && s_m.pointer_was_down && s_m.pressed) {
        if (!s_m.dragging) resolve_click(s_m.press_pos);
        s_m.pressed  = false;
        s_m.dragging = false;
    }

    s_m.last_pointer     = p;
    s_m.pointer_was_down = down;
}

void modal_instance_map_update(float dt) {
    if (!s_m.open) return;

    instance_map_data_update(dt);
    if (s_m.universe_open) universe_map_data_update(dt);
    quest_target_update();

    /* A refetched graph keeps the map or world a selection names; a portal or a place falls back to its map.
     * The card of the followed objective stays while the objective does. */
    if (view_generation() != s_m.layout_generation) {
        s_m.layout_generation = view_generation();
        pack_view();
        int node = -1;
        for (int i = 0; '\0' != s_m.selected_code[0] && view_node_count() > i; i++)
            if (0 == strcmp(s_m.selected_code, view_node_code(i))) node = i;
        MapDetailKind kind = s_m.selected.kind;
        select_node(node);
        if (MAP_DETAIL_OBJECTIVE == kind) s_m.selected = (MapDetail){ MAP_DETAIL_OBJECTIVE, -1, -1 };
        s_hit_count = 0;
    }
    if (MAP_DETAIL_OBJECTIVE == s_m.selected.kind && NULL == quest_target_tracked()) select_node(-1);
    ImapDataState state = VIEW_UNIVERSE == s_m.view ? universe_map_data_state() : instance_map_data_state();
    if (IMAP_DATA_READY == state && (!s_m.framed || GetScreenWidth() != s_m.viewport_w ||
                                    GetScreenHeight() != s_m.viewport_h)) frame_view();
    if (s_m.locate_pending && VIEW_WORLD == s_m.view && IMAP_DATA_READY == state && s_m.framed) {
        s_m.locate_pending = false;
        focus_destination(quest_target_tracked(), quest_target_tracked_destination());
    }

    track_pointer();
    if (VIEW_UNIVERSE == s_m.view && IMAP_DATA_READY == state) refresh_universe_route();
    else refresh_route();

    /* Smooth camera interpolation (frame-rate independent). */
    float a = ease_exp_factor(IMAP_CAM_LAMBDA, dt);
    s_m.zoom  = ease_approach(s_m.zoom, s_m.zoom_target, a);
    s_m.pan.x = ease_approach(s_m.pan.x, s_m.pan_target.x, a);
    s_m.pan.y = ease_approach(s_m.pan.y, s_m.pan_target.y, a);
}

/* ── Input dispatch glue ────────────────────────────────────────────────── */

bool modal_instance_map_covers_point(int mx, int my) {
    if (!s_m.open) return false;
    return CheckCollisionPointRec((Vector2){ (float)mx, (float)my }, s_m.panel);
}

bool modal_instance_map_handle_click(int mx, int my) {
    /* Consume presses inside the panel; the gesture tracker resolves them.
     * Presses outside fall through — the overlay is non-blocking. */
    return modal_instance_map_covers_point(mx, my);
}

/* ── Drawing ────────────────────────────────────────────────────────────── */

static Color fade_c(Color c, float f) {
    c.a = (unsigned char)((float)c.a * f);
    return c;
}

static void shadow_label(const char* s, int x, int y, int fs, Color c) {
    text_draw_outlined(s, x, y, fs, c, (Color){ 0, 0, 0, c.a }, 1);
}

/* The first letter of the first two words of a name, upper case. */
static void initials_of(const char* name, char* out, size_t size) {
    size_t n = 0;
    bool word_start = true;
    for (const char* c = name; '\0' != *c && n + 1 < size && n < 2; c++) {
        bool letter = isalnum((unsigned char)*c);
        if (letter && word_start) out[n++] = (char)toupper((unsigned char)*c);
        word_start = !letter;
    }
    out[n] = '\0';
}

/* Matching pictures fill rounded map bounds; other pictures keep their proportions. */
static void draw_picture(Rectangle r, Vector2 extent, const char* url, const char* name, FetchPriority priority, float fade) {
    Texture2D tex = '\0' != url[0] ? fetch_texture(url, 1, priority, true) : (Texture2D){ 0 };
    DrawRectangleRec(r, fade_c((Color){ 10, 18, 32, 255 }, fade));
    if (0 != tex.id) {
        bool matches = 1e-4f > fabsf((float)tex.width / (float)tex.height - extent.x / extent.y);
        DrawTexturePro(tex, (Rectangle){ 0, 0, (float)tex.width, (float)tex.height },
                       matches ? r : ui_rect_contain(r, (float)tex.width, (float)tex.height), (Vector2){ 0, 0 }, 0.0f,
                       Fade(WHITE, fade));
        return;
    }
    /* Dots only where the panel shows the card: a zoomed card is far larger than the screen. */
    int left = (int)fmaxf(0.0f, s_m.panel.x - r.x), top = (int)fmaxf(0.0f, s_m.panel.y - r.y);
    int right = (int)fminf(r.width, s_m.panel.x + s_m.panel.width - r.x);
    int bottom = (int)fminf(r.height, s_m.panel.y + s_m.panel.height - r.y);
    for (int y = 4 + (top / 8) * 8; y < bottom; y += 8)
        for (int x = 4 + (y / 8) % 2 * 4 + (left / 8) * 8; x < right; x += 8)
            DrawRectangle((int)r.x + x, (int)r.y + y, 1, 1, fade_c(IMAP_NODE_LINE, fade * 0.35f));
    char mark[3];
    initials_of(name, mark, sizeof mark);
    int size = (int)fminf(48.0f, fmaxf(10.0f, r.height * 0.4f));
    int width = MeasureText(mark, size);
    shadow_label(mark, (int)(r.x + (r.width - (float)width) * 0.5f), (int)(r.y + (r.height - (float)size) * 0.42f), size,
                 fade_c(IMAP_TEXT_DIM, fade));
}

static void draw_pixel_border(Rectangle bounds, Color accent, bool focused, float fade) {
    if (bounds.width < 8.0f || bounds.height < 8.0f) {
        DrawRectangleRec(bounds, fade_c(accent, fade));
        return;
    }
    Color dark = fade_c((Color){ 4, 8, 16, 255 }, fade);
    Color light = fade_c(focused ? WHITE : (Color){ 110, 140, 180, 220 }, fade);
    Color shade = fade_c((Color){ 2, 4, 10, 255 }, fade);
    Color edge = fade_c(accent, fade);
    int width = (int)bounds.width;
    int height = (int)bounds.height;

    DrawRectangle((int)bounds.x, (int)bounds.y, width, 2, light);
    DrawRectangle((int)bounds.x, (int)bounds.y, 2, height, light);
    DrawRectangle((int)bounds.x, (int)(bounds.y + bounds.height - 2.0f), width, 2, shade);
    DrawRectangle((int)(bounds.x + bounds.width - 2.0f), (int)bounds.y, 2, height, shade);
    DrawRectangle((int)bounds.x, (int)(bounds.y + 4.0f), 2, height - 8, edge);
    if (focused) {
        DrawRectangleLinesEx(bounds, 1.0f, fade_c(WHITE, fade));
        DrawRectangle((int)bounds.x, (int)bounds.y, 4, 4, dark);
        DrawRectangle((int)(bounds.x + bounds.width - 4.0f), (int)bounds.y, 4, 4, dark);
        DrawRectangle((int)bounds.x, (int)(bounds.y + bounds.height - 4.0f), 4, 4, dark);
        DrawRectangle((int)(bounds.x + bounds.width - 4.0f),
                      (int)(bounds.y + bounds.height - 4.0f), 4, 4, dark);
    }
}

static void draw_pixel_panel(Rectangle bounds, Color fill, Color accent, bool focused, float fade) {
    if (bounds.width < 4.0f || bounds.height < 4.0f) {
        DrawRectangleRec(bounds, fade_c(fill, fade));
        return;
    }
    DrawRectangleRec(bounds, fade_c(BLACK, fade));
    Rectangle inner = ui_rect_inset(bounds, 2.0f);
    DrawRectangleRec(inner, fade_c(fill, fade));
    draw_pixel_border(bounds, accent, focused, fade);
}

/* An outline inside `bounds`. */
static void draw_pixel_outline(Rectangle bounds, int thickness, Color color) {
    if (bounds.width <= 0.0f || bounds.height <= 0.0f) return;
    DrawRectangle((int)bounds.x, (int)bounds.y, (int)bounds.width, thickness, color);
    DrawRectangle((int)bounds.x, (int)bounds.y, thickness, (int)bounds.height, color);
    DrawRectangle((int)bounds.x, (int)(bounds.y + bounds.height - thickness), (int)bounds.width, thickness, color);
    DrawRectangle((int)(bounds.x + bounds.width - thickness), (int)bounds.y, thickness, (int)bounds.height, color);
}

/* Background grid follows the packed map-card tile scale. */
static void draw_backdrop(float fade) {
    Color line = fade_c(IMAP_GRID_LINE, fade);
    float step = node_screen_side();
    if (step < 18.0f) step = 18.0f;
    float ox = fmodf(s_m.pan.x, step);
    float oy = fmodf(s_m.pan.y, step);

    for (float x = s_m.panel.x + ox - step; x < s_m.panel.x + s_m.panel.width + step; x += step) {
        DrawRectangle((int)x, (int)s_m.panel.y, 1, (int)s_m.panel.height, line);
    }
    for (float y = s_m.panel.y + oy - step; y < s_m.panel.y + s_m.panel.height + step; y += step) {
        DrawRectangle((int)s_m.panel.x, (int)y, (int)s_m.panel.width, 1, line);
    }
}

/* Resolve one edge endpoint on its node card: a known cell anchors to that
 * cell (the portal landmark); a random destination (-1) roams across the
 * whole card — the link keeps pointing at ever-changing spots of the map,
 * mirroring the teleport's "anywhere on this map" behaviour. */
static Vector2 edge_endpoint(int node, int cell_x, int cell_y, double t, int salt) {
    const ImapNode* n = &instance_map_data_graph()->nodes[node];
    Rectangle card = node_rect(node);
    if (cell_x < 0 || cell_y < 0) {
        float sx = (float)salt * 2.1f, sy = (float)salt * 1.3f;
        float fx = 0.5f + 0.36f * sinf((float)t * 0.83f + sx) * cosf((float)t * 0.31f + sy * 0.7f);
        float fy = 0.5f + 0.36f * sinf((float)t * 0.67f + sy) * cosf((float)t * 0.41f + sx * 1.9f);
        fx = floorf(fx * 8.0f + 0.5f) / 8.0f;
        fy = floorf(fy * 8.0f + 0.5f) / 8.0f;
        return (Vector2){ card.x + fx * card.width, card.y + fy * card.height };
    }
    return instance_map_cell_to_card(card, n, (float)cell_x + 0.5f, (float)cell_y + 0.5f);
}

/* A pixel line `width` px wide in `color` over a dark outline 4 px wider, with a light thread. */
static void draw_pixel_diagonal(Vector2 a, Vector2 b, Color color, int width, float fade) {
    float dx = b.x - a.x;
    float dy = b.y - a.y;
    int steps = (int)(fmaxf(fabsf(dx), fabsf(dy)) / 4.0f) + 1;
    if (steps > 384) steps = 384;
    Color shadow = fade_c((Color){ 0, 0, 0, 215 }, fade);
    Color core = fade_c(color, fade);
    Color light = fade_c((Color){ 225, 245, 255, 210 }, fade);
    DrawLineEx(a, b, (float)width + 4.0f, shadow);
    DrawLineEx(a, b, (float)width, core);
    DrawLineEx(a, b, 1.0f, light);
    int last_x = INT_MIN;
    int last_y = INT_MIN;
    for (int i = 0; i <= steps; i++) {
        float progress = (float)i / (float)steps;
        int x = (int)roundf(a.x + dx * progress) - width / 2;
        int y = (int)roundf(a.y + dy * progress) - width / 2;
        if (x == last_x && y == last_y) continue;
        DrawRectangle(x - 2, y - 2, width + 4, width + 4, shadow);
        DrawRectangle(x, y, width, width, core);
        DrawRectangle(x, y, 1, 1, light);
        last_x = x;
        last_y = y;
    }
}

static Vector2 pixel_path_point(Vector2 a, Vector2 b, float progress) {
    Vector2 point = { a.x + (b.x - a.x) * progress,
                      a.y + (b.y - a.y) * progress };
    point.x = floorf(point.x + 0.5f);
    point.y = floorf(point.y + 0.5f);
    return point;
}

/* The colour of a link: an exit to another map, or a link inside one map. */
static Color edge_color(const ImapEdge* e) {
    return e->intra ? IMAP_EDGE_INTRA : instance_map_presence_color(IMAP_PRESENCE_PORTAL);
}

static bool edge_random(const ImapEdge* e) { return 0 > e->target_cell_x || 0 > e->target_cell_y; }

/* How much a link stands out. The selected or hovered portal leads; a portal
 * of the route on show leads in the route colour; while a portal is selected,
 * the other links step back. */
typedef enum { LINK_DIM = 0, LINK_PLAIN, LINK_ROUTE, LINK_FOCUS } LinkEmphasis;

static LinkEmphasis link_emphasis(int idx, const MapHit* hover) {
    bool selected = MAP_DETAIL_PORTAL == s_m.selected.kind && idx == s_m.selected.index;
    bool hovered = NULL != hover && MAP_DETAIL_PORTAL == hover->kind && idx == hover->index;
    if (selected || hovered) return LINK_FOCUS;
    if (s_route_edge[idx]) return LINK_ROUTE;
    return MAP_DETAIL_PORTAL == s_m.selected.kind ? LINK_DIM : LINK_PLAIN;
}

/* Maps that touch show a link across their edge only while one of them is selected or a route takes it. */
static bool edge_shown(const ImapEdge* e, LinkEmphasis emphasis) {
    return LINK_ROUTE <= emphasis || !e->joined || s_m.selected_node == e->source_node ||
           s_m.selected_node == e->target_node;
}

/* A thin link with a dark outline; a random destination dashes it. */
static void draw_thin_link(Vector2 a, Vector2 b, Color color, bool dashed, float alpha) {
    float length = Vector2Distance(a, b);
    Vector2 dir = Vector2Scale(Vector2Subtract(b, a), 1.0f / length);
    float dash = dashed ? 6.0f : length, step = dashed ? 10.0f : length;
    for (float d = 0.0f; length > d; d += step) {
        Vector2 from = Vector2Add(a, Vector2Scale(dir, d));
        Vector2 to = Vector2Add(a, Vector2Scale(dir, fminf(length, d + dash)));
        DrawLineEx(from, to, 3.0f, Fade(BLACK, alpha * 0.85f));
        DrawLineEx(from, to, 1.0f, Fade(color, alpha));
    }
}

/* Pass one: the line of a link. A link in focus carries the bright line and the
 * pulse; a link of the route a wider line in the route colour. */
static void draw_edge_line(const ImapEdge* e, int idx, LinkEmphasis emphasis, float fade, double t) {
    if (!edge_shown(e, emphasis)) return;
    Vector2 a = edge_endpoint(e->source_node, e->source_cell_x, e->source_cell_y, t, idx * 2);
    Vector2 b = edge_endpoint(e->target_node, e->target_cell_x, e->target_cell_y, t, idx * 2 + 1);
    if (16.0f > Vector2DistanceSqr(a, b)) return;
    if (LINK_ROUTE > emphasis) {
        draw_thin_link(a, b, edge_color(e), edge_random(e), fade * (LINK_DIM == emphasis ? 0.3f : 0.75f));
        return;
    }
    bool route = LINK_ROUTE == emphasis;
    Color color = route ? MAP_MARKER_ROUTE : edge_color(e);
    draw_pixel_diagonal(a, b, color, route ? 5 : 3, fade);
    float phase = (float)fmod(t * 0.55 + idx * 0.19, 1.0);
    map_marker_draw_pulse(pixel_path_point(a, b, phase), color, fade);
}

static float gate_radius(void) {
    return map_marker_size(cell_px(), MAP_MARKER_GATE_MIN, MAP_MARKER_GATE_MAX);
}

/* Where each gate stood this frame; a gate of an unknown cell has none. */
static Vector2 s_gate_at[IMAP_MAX_EDGES];
static bool    s_gate_shown[IMAP_MAX_EDGES];

/* Pass two, over every line: the gate where a portal stands, always, and the end
 * of a shown link. A fixed destination ends in an arrow on its cell; a random one
 * keeps its roaming end in a ring and names no cell. In focus or on the route,
 * the destination map takes an outline; a gate on the route takes a ring. */
static void draw_edge_ends(const ImapEdge* e, int idx, LinkEmphasis emphasis, float fade, double t) {
    Vector2 a = edge_endpoint(e->source_node, e->source_cell_x, e->source_cell_y, t, idx * 2);
    Vector2 b = edge_endpoint(e->target_node, e->target_cell_x, e->target_cell_y, t, idx * 2 + 1);
    float r = gate_radius(), length = Vector2Distance(a, b);
    bool lead = LINK_ROUTE <= emphasis, route = LINK_ROUTE == emphasis;
    Color color = route ? MAP_MARKER_ROUTE : edge_color(e);
    float alpha = fade * (LINK_DIM == emphasis ? 0.45f : 1.0f);
    if (lead && !e->intra) draw_pixel_outline(node_rect(e->target_node), 2, fade_c(color, fade));
    if (edge_shown(e, emphasis) && 2.0f * r < length) {
        Vector2 dir = Vector2Scale(Vector2Subtract(b, a), 1.0f / length);
        float size = lead ? r : r * 0.7f;
        if (edge_random(e)) {
            DrawRing(b, size * 0.6f, size * 0.6f + 2.0f, 0.0f, 360.0f, 16, Fade(color, alpha));
        } else {
            map_marker_draw_arrow(Vector2Subtract(b, Vector2Scale(dir, size)), dir, size, color, alpha);
            if (lead) map_marker_draw_focus((Rectangle){ b.x - 3.0f, b.y - 3.0f, 6.0f, 6.0f }, color, fade);
        }
    }
    s_gate_shown[idx] = 0 <= e->source_cell_x && 0 <= e->source_cell_y;
    if (!s_gate_shown[idx]) return;
    s_gate_at[idx] = a;
    if (route) DrawRing(a, r + 4.0f, r + 7.0f, 0.0f, 360.0f, 24, fade_c(MAP_MARKER_ROUTE, fade));
    map_marker_draw_gate(a, lead ? r + 2.0f : r, edge_color(e), fade);
    add_hit((Rectangle){ a.x - r - 4.0f, a.y - r - 4.0f, 2.0f * r + 8.0f, 2.0f * r + 8.0f }, MAP_DETAIL_PORTAL, idx, 1);
}

/* Whether a link starts at a cell of a map: its gate then marks the portal there. */
static bool edge_starts_at(int node, int cell_x, int cell_y) {
    const ImapGraph* gr = instance_map_data_graph();
    for (int i = 0; gr->edge_count > i; ++i)
        if (node == gr->edges[i].source_node && cell_x == gr->edges[i].source_cell_x &&
            cell_y == gr->edges[i].source_cell_y) return true;
    return false;
}

/* Every rim goes under every picture, so only the rim around maps that touch shows. */
static void draw_surface_rim(float fade) {
    const ImapGraph* gr = instance_map_data_graph();
    for (int i = 0; gr->node_count > i; i++) DrawRectangleRec(ui_rect_inset(node_rect(i), -3.0f), fade_c(IMAP_RIM_SHADOW, fade));
    for (int i = 0; gr->node_count > i; i++) DrawRectangleRec(ui_rect_inset(node_rect(i), -1.0f), fade_c(IMAP_RIM, fade));
}

/* Marks draw inside their map, so a mark never opens a gap between maps. Selection
 * and hover outline the map and never tint its picture. */
static void draw_node_marks(int idx, int hovered_node, float fade, double t) {
    const ImapNode* n = &instance_map_data_graph()->nodes[idx];
    Rectangle card = node_rect(idx);
    if (0 == strcmp(n->map_code, g_local_player.map_code))
        draw_pixel_outline(card, 2, fade_c(MAP_MARKER_PLAYER, fade * (0.4f + 0.25f * sinf((float)t * 3.2f))));
    if (idx == s_m.selected_node) draw_pixel_outline(card, 2, fade_c(IMAP_SELECTED, fade));
    else if (idx == hovered_node) draw_pixel_outline(card, 1, fade_c(IMAP_HOVER, fade));
}

/* Copies text into out, cut with an ellipsis to max_width pixels. */
static void fit_label(char* out, size_t size, const char* text, int font_size, int max_width) {
    assert(4 <= size);
    copy_str(out, size, text);
    if (strlen(text) < size && max_width >= MeasureText(out, font_size)) return;
    if (max_width < MeasureText("...", font_size)) {
        out[0] = '\0';
        return;
    }
    int length = (int)strlen(out);
    while (0 < length && (size <= (size_t)length + 3 || max_width < MeasureText(out, font_size))) {
        length = text_utf8_floor(out, length - 1);
        if (size > (size_t)length + 3) memcpy(out + length, "...", 4);
    }
}

/* A name plate centred on its card; a long name shrinks its text before it loses letters.
 * Returns the plate, or no area when the card is too small to hold it. */
static Rectangle draw_node_name(const char* name, Rectangle card, bool selected, float fade) {
    int font_size = (int)Clamp(fminf(card.width * 0.07f, card.height * 0.14f), 10.0f, 20.0f);
    while (10 < font_size && (int)(card.width * 0.8f) - (font_size / 2 + 2) * 2 < MeasureText(name, font_size)) font_size--;
    int pad = font_size / 2 + 2;
    char label[IMAP_NAME_MAX];
    fit_label(label, sizeof(label), name, font_size, (int)(card.width * 0.8f) - pad * 2);
    int width = MeasureText(label, font_size);
    float height = (float)(font_size + pad);
    if (0 == width || card.height - 4.0f < height) return (Rectangle){ 0 };
    Vector2 center = ui_rect_center(card);
    Rectangle plate = { floorf(center.x - (float)(width + pad * 2) * 0.5f), floorf(center.y - height * 0.5f),
                        (float)(width + pad * 2), height };
    Color edge = selected ? IMAP_SELECTED : IMAP_PLATE_EDGE;
    DrawRectangleRec(ui_rect_inset(plate, -1.0f), fade_c(IMAP_RIM_SHADOW, fade));
    DrawRectangleRec(plate, fade_c(IMAP_PLATE, fade));
    DrawRectangleLinesEx(plate, 1.0f, fade_c(edge, fade));
    DrawRectangle((int)plate.x + 1, (int)plate.y + 1, (int)plate.width - 2, 1, fade_c(IMAP_PLATE_GLOW, fade));
    shadow_label(label, (int)plate.x + pad, (int)(plate.y + (height - (float)font_size) * 0.5f), font_size,
                 fade_c(selected ? IMAP_SELECTED : IMAP_PLATE_TEXT, fade));
    return plate;
}

/* The name plate of each map as last drawn: a destination label keeps clear of it. */
static Rectangle s_plates[IMAP_MAX_NODES];

/* A place: an action pin with the icon of its first service and a badge for
 * more, and a quest badge in the state that matters most to the player — on the
 * corner of the pin when the place has both. A quest badge keeps its own size
 * at any zoom; a quest the player can take bobs. */
static Rectangle draw_place(const ImapPresencePoi* poi, Vector2 at, float side, float fade, double t) {
    Rectangle mark = { 0 };
    if (0 < poi->action_count) {
        const char* icon = "hand";
        int services = 0;
        for (int s = IMAP_SERVICE_COUNT - 1; 0 <= s; --s)
            if (poi->services & (1u << s)) {
                icon = map_detail_service_icon((ImapService)s);
                services++;
            }
        map_marker_draw_pin(at, side, icon, services, IMAP_PLATE_EDGE, fade);
        mark = map_marker_pin_rect(at, side);
    }
    if (0 < poi->quest_count) {
        float q = floorf(map_marker_size(cell_px(), MAP_MARKER_QUEST_MIN, MAP_MARKER_QUEST_MAX));
        Rectangle box = 0 < poi->action_count
            ? (Rectangle){ mark.x + mark.width - q * 0.5f, mark.y - q * 0.5f, q, q }
            : (Rectangle){ floorf(at.x - q * 0.5f), floorf(at.y - q * 0.5f), q, q };
        Rectangle badge = box;
        if (IMAP_QUEST_AVAILABLE == poi->quest_state) badge.y -= roundf(2.0f + 2.0f * sinf((float)t * 4.0f));
        bool muted = IMAP_QUEST_LOCKED == poi->quest_state || IMAP_QUEST_COMPLETED == poi->quest_state;
        map_marker_draw_quest(badge, map_detail_quest_icon(poi->quest_state), muted, poi->quest_count, fade);
        if (0 == poi->action_count) return box;
        float left = fminf(mark.x, box.x), top = fminf(mark.y, box.y);
        mark = (Rectangle){ left, top, fmaxf(mark.x + mark.width, box.x + box.width) - left,
                            fmaxf(mark.y + mark.height, box.y + box.height) - top };
    }
    return mark;
}

static bool portal_presence(const ImapPresencePoi* poi) {
    return IMAP_PRESENCE_PORTAL == poi->presence_status || IMAP_PRESENCE_PORTAL_RANDOM == poi->presence_status;
}

/* How each POI serves a quest objective this frame: none, a target, a
 * destination of the followed objective, or its destination in focus; and
 * whether an active `talk` objective sends the player there. */
enum { POI_TARGET_NONE = 0, POI_TARGET, POI_TARGET_FOLLOWED, POI_TARGET_FOCUS };
static uint8_t s_poi_target[IMAP_MAX_PRESENCE_POIS];
static bool    s_poi_talk[IMAP_MAX_PRESENCE_POIS];

static void mark_targets(void) {
    memset(s_poi_target, POI_TARGET_NONE, sizeof s_poi_target);
    memset(s_poi_talk, 0, sizeof s_poi_talk);
    const QuestTarget* followed = quest_target_tracked();
    int focus = quest_target_tracked_destination();
    for (int i = 0; quest_target_count() > i; i++) {
        const QuestTarget* target = quest_target_get(i);
        for (int d = 0; target->destination_count > d; d++) {
            uint8_t state = target != followed ? POI_TARGET : d == focus ? POI_TARGET_FOCUS : POI_TARGET_FOLLOWED;
            int poi = target->destinations[d].poi;
            if (state > s_poi_target[poi]) s_poi_target[poi] = state;
            if (QUEST_OBJECTIVE_TALK == target->objective.type) s_poi_talk[poi] = true;
        }
    }
}

/* Bot and resource marks of one class that overlap on screen: one mark with a count. */
typedef struct {
    Vector2 at;
    ImapPresenceStatus status;
    int first, count;
    bool alone;     /* the selection or a quest target: it never groups */
} PresenceGroup;

/* The bots and resources of one map. A mark that overlaps an earlier mark of its
 * class joins its group; resources draw first and hostile bots last, on top. */
static void draw_presence(int idx, Rectangle card, float side, float fade) {
    static PresenceGroup groups[IMAP_MAX_PRESENCE_POIS];
    const ImapGraph* gr = instance_map_data_graph();
    int count = 0;
    for (int i = 0; gr->presence_poi_count > i; ++i) {
        const ImapPresencePoi* poi = &gr->presence_pois[i];
        if (idx != poi->node || 0 < poi->action_count || 0 < poi->quest_count || portal_presence(poi)) continue;
        Vector2 at = instance_map_cell_to_card(card, &gr->nodes[idx], poi->cell_x + 0.5f, poi->cell_y + 0.5f);
        bool alone = (MAP_DETAIL_PLACE == s_m.selected.kind && i == s_m.selected.index) || s_poi_target[i];
        int g = 0;
        while (!alone && count > g && (groups[g].alone || groups[g].status != poi->presence_status ||
                                       side <= Vector2Distance(groups[g].at, at)))
            g++;
        if (alone || count == g) groups[count++] = (PresenceGroup){ at, poi->presence_status, i, 1, alone };
        else groups[g].count++;
    }
    static const ImapPresenceStatus ORDER[] = { IMAP_PRESENCE_RESOURCE, IMAP_PRESENCE_PASSIVE, IMAP_PRESENCE_HOSTILE };
    for (int o = 0; (int)(sizeof ORDER / sizeof ORDER[0]) > o; ++o)
        for (int g = 0; count > g; ++g) {
            const PresenceGroup* group = &groups[g];
            if (ORDER[o] != group->status) continue;
            map_marker_draw_presence(group->at, side, group->status, group->count, fade);
            float room = side + 6.0f, badge = 1 < group->count ? IMAP_BADGE_W : 0.0f;
            add_hit((Rectangle){ group->at.x - room * 0.5f, group->at.y - room * 0.5f, room + badge, room },
                    MAP_DETAIL_PLACE, group->first, group->count);
        }
}

/* Overlay pass, per map: the name plate, the bots and resources, then the
 * places and the portal POIs that start no link. Every mark keeps the screen
 * size of map_marker_size at any zoom. */
static void draw_node_overlay(int idx, float fade, double t) {
    const ImapGraph* gr = instance_map_data_graph();
    const ImapNode*  n  = &gr->nodes[idx];
    Rectangle card = node_rect(idx);
    char label[IMAP_NAME_MAX];
    instance_map_node_label(gr, idx, label, sizeof label);
    s_plates[idx] = draw_node_name(label, card, idx == s_m.selected_node, fade);

    float cell = cell_px();
    draw_presence(idx, card, map_marker_size(cell, MAP_MARKER_PRESENCE_MIN, MAP_MARKER_PRESENCE_MAX), fade);
    float pin = map_marker_size(cell, MAP_MARKER_PIN_MIN, MAP_MARKER_PIN_MAX);
    for (int i = 0; i < gr->presence_poi_count; ++i) {
        const ImapPresencePoi* poi = &gr->presence_pois[i];
        if (poi->node != idx) continue;
        Vector2 at = instance_map_cell_to_card(card, n, poi->cell_x + 0.5f, poi->cell_y + 0.5f);
        if (0 < poi->action_count || 0 < poi->quest_count)
            add_hit(draw_place(poi, at, pin, fade, t), MAP_DETAIL_PLACE, i, 1);
        else if (portal_presence(poi) && !edge_starts_at(idx, poi->cell_x, poi->cell_y))
            map_marker_draw_gate(at, gate_radius(), instance_map_presence_color(poi->presence_status), fade);
    }
}

/* Where the entry spawn stood this frame, and whether it shows. */
static Vector2 s_spawn_at;
static bool    s_spawn_shown;

/* A fixed entry spawn on its cell, over every bot and place. A random one has no cell to show. */
static void draw_entry_spawn(float fade) {
    const ImapGraph* gr = instance_map_data_graph();
    const ImapEntrySpawn* spawn = &gr->entry_spawn;
    int node = IMAP_SPAWN_FIXED == spawn->mode ? instance_map_data_find_node(spawn->map_code) : -1;
    s_spawn_shown = 0 <= node;
    if (!s_spawn_shown) return;
    s_spawn_at = instance_map_cell_to_card(node_rect(node), &gr->nodes[node], spawn->cell_x + 0.5f,
                                           spawn->cell_y + 0.5f);
    float side = map_marker_size(cell_px(), MAP_MARKER_SPAWN_MIN, MAP_MARKER_SPAWN_MAX), room = side + 8.0f;
    map_marker_draw_spawn(s_spawn_at, side, fade);
    add_hit((Rectangle){ s_spawn_at.x - room * 0.5f, s_spawn_at.y - room * 0.5f, room, room }, MAP_DETAIL_SPAWN, 0, 1);
}

/* The mark of the selection or of the hover, as last drawn. */
static const MapHit* find_hit(MapDetailKind kind, int index) {
    for (int i = 0; s_hit_count > i; ++i)
        if (kind == s_hits[i].kind && index == s_hits[i].index) return &s_hits[i];
    return NULL;
}

/* The mark of a POI as last drawn; a square about its cell when it has none. */
static Rectangle poi_mark(int poi) {
    const MapHit* hit = find_hit(MAP_DETAIL_PLACE, poi);
    if (NULL != hit) return hit->rect;
    const ImapGraph* gr = instance_map_data_graph();
    const ImapPresencePoi* p = &gr->presence_pois[poi];
    Vector2 at = instance_map_cell_to_card(node_rect(p->node), &gr->nodes[p->node], p->cell_x + 0.5f,
                                           p->cell_y + 0.5f);
    float side = map_marker_size(cell_px(), MAP_MARKER_PRESENCE_MIN, MAP_MARKER_PRESENCE_MAX) + 6.0f;
    return (Rectangle){ at.x - side * 0.5f, at.y - side * 0.5f, side, side };
}

/* Gold brackets around the mark of each quest target: full for the followed
 * objective, with a halo on its destination in focus; quieter for the rest. An
 * NPC a `talk` objective sends the player to adds a talk badge on its corner,
 * as large as a quest badge at any zoom. */
static void draw_quest_targets(float fade, double t) {
    float side = floorf(map_marker_size(cell_px(), MAP_MARKER_QUEST_MIN, MAP_MARKER_QUEST_MAX));
    for (int poi = 0; instance_map_data_graph()->presence_poi_count > poi; poi++) {
        if (POI_TARGET_NONE == s_poi_target[poi]) continue;
        float alpha = POI_TARGET == s_poi_target[poi] ? 0.55f : 1.0f;
        Rectangle mark = poi_mark(poi);
        map_marker_draw_target(mark, POI_TARGET_FOCUS == s_poi_target[poi], fade * alpha, t);
        if (s_poi_talk[poi])
            map_marker_draw_talk((Rectangle){ mark.x - side * 0.5f, mark.y - side * 0.5f, side, side }, fade);
    }
}

/* Where the own player stands on the map, its mark radius and whether its tag
 * shows. False while its map is not on the graph. */
static bool player_at(Vector2* at, float* radius, bool* tagged) {
    int idx = instance_map_data_find_node(g_local_player.map_code);
    if (0 > idx) return false;
    const ImapNode* n = &instance_map_data_graph()->nodes[idx];
    Rectangle card = node_rect(idx);
    Vector2 pos = game_state_own_render()->interp_pos;
    *radius = map_marker_size(cell_px(), MAP_MARKER_PLAYER_MIN_R, MAP_MARKER_PLAYER_MAX_R);
    *tagged = IMAP_TAG_CELL_PX <= cell_px();
    *at = instance_map_cell_to_card(card, n, pos.x, pos.y);
    return true;
}

/* The room of the player mark and its tag: no label covers them. */
static Rectangle player_room(void) {
    Vector2 at;
    float r = 0.0f;
    bool tagged = false;
    if (!player_at(&at, &r, &tagged)) return (Rectangle){ 0 };
    float top = tagged ? r + 40.0f : r + 6.0f;
    return (Rectangle){ at.x - r - 6.0f, at.y - top, 2.0f * r + 12.0f, top + r + 6.0f };
}

/* The own player, over every other mark. Zoomed in, a tag over it shows its
 * status and its stats. */
static void draw_player_marker(float fade, double t) {
    Vector2 at;
    float r = 0.0f;
    bool tagged = false;
    if (!player_at(&at, &r, &tagged)) return;
    if (tagged) {
        uint8_t status = 0 != local_player_status_icon() ? local_player_status_icon() : STATUS_ICON_PLAYER;
        char stats[16];
        snprintf(stats, sizeof stats, "%d", game_state_progress(g_local_player.id)->stats_sum);
        float icon = 20.0f, width = icon + 10.0f + (float)MeasureText(stats, 14);
        Rectangle tag = { floorf(at.x - width * 0.5f), floorf(at.y - r - 34.0f), width, icon + 6.0f };
        draw_pixel_panel(tag, (Color){ 12, 18, 34, 238 }, MAP_MARKER_PLAYER, false, fade);
        const char* status_icon = presentation_runtime_status_icon(status);
        if (NULL != status_icon && '\0' != status_icon[0])
            ui_icon_draw_ex(status_icon, tag.x + 3.0f + icon * 0.5f, tag.y + tag.height * 0.5f, icon, 0.0f,
                            Fade(WHITE, fade));
        shadow_label(stats, (int)(tag.x + icon + 6.0f), (int)(tag.y + (tag.height - 14.0f) * 0.5f), 14,
                     fade_c(IMAP_TEXT, fade));
    }
    map_marker_draw_player(at, r, local_player_view_facing(), fade, t);
}

/* Whether a hit is a bot or a resource: a POI with no quest and no action. */
static bool presence_hit(const MapHit* hit) {
    const ImapPresencePoi* poi = &instance_map_data_graph()->presence_pois[hit->index];
    return MAP_DETAIL_PLACE == hit->kind && 0 == poi->action_count && 0 == poi->quest_count;
}

/* A label clear of every rect of `taken`. A label that `leads` may cover the
 * first `marks` rects, the bots and resources, when no side is clear of them. */
static bool place_label(Vector2 anchor, Vector2 size, float gap, Rectangle bounds, const Rectangle* taken, int count,
                        int marks, bool leads, Rectangle* out) {
    return map_marker_place_label(anchor, size, gap, bounds, taken, count, out) ||
           (leads && map_marker_place_label(anchor, size, gap, bounds, taken + marks, count - marks, out));
}

/* The destination label of each portal text, made once per graph. */
static char s_edge_text[IMAP_MAX_EDGES][IMAP_NAME_MAX * 2 + 24];
static int  s_edge_text_generation = -1;

/* A label of the frame. Labels take their places in the order they are
 * queued:
 *   1. the followed target;
 *   2. the quests the player can take, and the NPCs a `talk` objective names;
 *   3. the portal in focus and the portals of the route;
 *   4. the entry spawn and the selected bot or resource;
 *   5. the other quest targets;
 *   6. the NPCs, then the bots and the resources, with their stat sums;
 *   7. the portals of the player's map and of the selected map, then the rest.
 * Steps 5 and 6 show only once the cells read. A label never covers
 * a mark, a name plate, the player, the detail card, a button or a label placed
 * before it; one with no free side waits for the next zoom. A label that leads
 * may cover bots and resources. A POI takes one label, and a label that does
 * not lead never repeats the text of a placed label nearby. */
typedef struct {
    Vector2 at;
    float   gap;
    char    text[IMAP_NAME_MAX * 2 + 24];
    Color   color;
    bool    arrow, strong, leads;
    int     value;      /* a stat sum, or -1 */
    float   alpha;
} MapLabel;

#define IMAP_LABELS_MAX 256
static MapLabel s_labels[IMAP_LABELS_MAX];
static int      s_label_count;
static bool     s_poi_labelled[IMAP_MAX_PRESENCE_POIS];

/* A label `gap` px off `at`; NULL when the frame holds no more. */
static MapLabel* queue_label(Vector2 at, float gap, Color color, bool leads) {
    if (IMAP_LABELS_MAX <= s_label_count) return NULL;
    MapLabel* label = &s_labels[s_label_count++];
    *label = (MapLabel){ .at = at, .gap = gap, .color = color, .leads = leads, .value = -1, .alpha = 1.0f };
    return label;
}

/* A label off the mark of a POI in view that has none yet; NULL otherwise. */
static MapLabel* queue_poi_label(int poi, Rectangle bounds, Color color, bool leads) {
    Rectangle mark = poi_mark(poi);
    Vector2 at = ui_rect_center(mark);
    if (s_poi_labelled[poi] || !CheckCollisionPointRec(at, bounds)) return NULL;
    MapLabel* label = queue_label(at, mark.height * 0.5f + 4.0f, color, leads);
    if (NULL != label) s_poi_labelled[poi] = true;
    return label;
}

/* What the player does at each destination of `target` in view. The followed
 * one labels its destination in focus at any zoom, its other ones and every
 * other target only once the cells read. */
static void queue_target_labels(const QuestTarget* target, bool followed, bool readable, Rectangle bounds) {
    int focus = followed ? quest_target_tracked_destination() : -1;
    for (int d = 0; target->destination_count > d; d++) {
        if (d != focus && !readable) continue;
        MapLabel* label = queue_poi_label(target->destinations[d].poi, bounds, MODAL_QUEST, d == focus);
        if (NULL == label) continue;
        quest_target_action_text(target, d, label->text, sizeof label->text);
        label->strong = d == focus;
        label->alpha = followed ? 1.0f : 0.8f;
    }
}

/* The first quest the player can take at each quest giver in view. */
static void queue_offer_labels(Rectangle bounds) {
    const ImapGraph* gr = instance_map_data_graph();
    for (int q = 0; gr->quest_count > q; q++) {
        const ImapProvider* quest = &gr->quests[q];
        if (IMAP_QUEST_AVAILABLE != instance_map_quest_state(quest->code)) continue;
        MapLabel* label = queue_poi_label(quest->poi, bounds, MODAL_QUEST, false);
        const char* title = '\0' != quest->label[0] ? quest->label : quest->code;
        if (NULL != label) snprintf(label->text, sizeof label->text, "NEW QUEST: %s", title);
    }
}

/* What a bot or a resource is, in the colour of its class, and its stat sum as placed. */
static void queue_entity_label(int poi, Rectangle bounds) {
    const ImapGraph* gr = instance_map_data_graph();
    const ImapPresencePoi* p = &gr->presence_pois[poi];
    if (0 > p->first_placed) return;
    MapLabel* label = queue_poi_label(poi, bounds, instance_map_presence_color(p->presence_status), false);
    if (NULL == label) return;
    copy_str(label->text, sizeof label->text, gr->placed[p->first_placed].item_id);
    label->value = gr->placed[p->first_placed].stats;
}

/* The name of each NPC in view whose action has one. */
static void queue_npc_labels(Rectangle bounds) {
    const ImapGraph* gr = instance_map_data_graph();
    for (int a = 0; gr->action_count > a; a++) {
        const ImapProvider* action = &gr->actions[a];
        if ('\0' == action->label[0]) continue;
        MapLabel* label = queue_poi_label(action->poi, bounds, IMAP_TEXT, false);
        if (NULL != label) copy_str(label->text, sizeof label->text, action->label);
    }
}

/* The destination label of each gate in view of one kind: 0 in focus or on the
 * route, 1 on the player's map or the selected map, 2 any other. */
static void queue_portal_labels(int kind, const MapHit* hover, Rectangle bounds) {
    const ImapGraph* gr = instance_map_data_graph();
    int player_node = instance_map_data_find_node(g_local_player.map_code);
    for (int e = 0; gr->edge_count > e; ++e) {
        const ImapEdge* edge = &gr->edges[e];
        if (!s_gate_shown[e] || !CheckCollisionPointRec(s_gate_at[e], bounds)) continue;
        LinkEmphasis emphasis = link_emphasis(e, hover);
        bool near = edge->source_node == player_node || edge->source_node == s_m.selected_node;
        if (kind != (LINK_ROUTE <= emphasis ? 0 : near ? 1 : 2)) continue;
        MapLabel* label = queue_label(s_gate_at[e], gate_radius() + 6.0f, edge_color(edge), 0 == kind);
        if (NULL == label) return;
        copy_str(label->text, sizeof label->text, s_edge_text[e]);
        label->arrow = true;
        label->strong = 0 == kind;
        label->alpha = LINK_DIM == emphasis ? 0.6f : 1.0f;
    }
}

/* `card` and `button` are the detail card and the button that the frame shows; zero when none shows. */
static void draw_labels(const MapHit* hover, Rectangle bounds, Rectangle card, Rectangle button, float fade) {
    const ImapGraph* gr = instance_map_data_graph();
    if (s_edge_text_generation != instance_map_data_generation()) {
        s_edge_text_generation = instance_map_data_generation();
        for (int e = 0; gr->edge_count > e; ++e)
            instance_map_edge_destination(gr, &gr->edges[e], s_edge_text[e], sizeof s_edge_text[e]);
    }
    s_label_count = 0;
    memset(s_poi_labelled, 0, sizeof s_poi_labelled);
    bool readable = IMAP_TAG_CELL_PX <= cell_px();
    const QuestTarget* followed = quest_target_tracked();
    if (NULL != followed) queue_target_labels(followed, true, readable, bounds);
    queue_offer_labels(bounds);
    for (int i = 0; quest_target_count() > i; i++)
        if (quest_target_get(i) != followed && QUEST_OBJECTIVE_TALK == quest_target_get(i)->objective.type)
            queue_target_labels(quest_target_get(i), false, true, bounds);
    queue_portal_labels(0, hover, bounds);
    if (s_spawn_shown && CheckCollisionPointRec(s_spawn_at, bounds)) {
        float gap = map_marker_size(cell_px(), MAP_MARKER_SPAWN_MIN, MAP_MARKER_SPAWN_MAX) * 0.5f + 6.0f;
        MapLabel* label = queue_label(s_spawn_at, gap, MAP_MARKER_SPAWN, true);
        assert(NULL != label);
        char entry[IMAP_NAME_MAX + 32];
        map_detail_entry_text(&gr->entry_spawn, entry, sizeof entry);
        snprintf(label->text, sizeof label->text, "ENTRY SPAWN: %s", entry);
        label->strong = MAP_DETAIL_SPAWN == s_m.selected.kind;
    }
    if (MAP_DETAIL_PLACE == s_m.selected.kind && 0 == s_poi_target[s_m.selected.index])
        queue_entity_label(s_m.selected.index, bounds);
    if (readable) {
        for (int i = 0; quest_target_count() > i; i++)
            if (quest_target_get(i) != followed) queue_target_labels(quest_target_get(i), false, readable, bounds);
        queue_npc_labels(bounds);
        for (int i = 0; s_hit_count > i; ++i)
            if (presence_hit(&s_hits[i]) && 1 == s_hits[i].count) queue_entity_label(s_hits[i].index, bounds);
    }
    queue_portal_labels(1, hover, bounds);
    if (6.5f < gate_radius()) queue_portal_labels(2, hover, bounds);

    static Rectangle taken[IMAP_HITS_MAX + IMAP_MAX_NODES + IMAP_LABELS_MAX + 3];
    int count = 0;
    for (int i = 0; s_hit_count > i; ++i)
        if (presence_hit(&s_hits[i])) taken[count++] = s_hits[i].rect;
    int marks = count;
    taken[count++] = player_room();
    taken[count++] = card;
    taken[count++] = button;
    for (int i = 0; s_hit_count > i; ++i)
        if (!presence_hit(&s_hits[i])) taken[count++] = s_hits[i].rect;
    for (int i = 0; gr->node_count > i; ++i)
        if (0.0f < s_plates[i].width) taken[count++] = s_plates[i];
    static int placed[IMAP_LABELS_MAX];
    int placed_count = 0;
    for (int i = 0; s_label_count > i; i++) {
        const MapLabel* label = &s_labels[i];
        bool repeat = false;
        for (int j = 0; !label->leads && !repeat && placed_count > j; j++) {
            const MapLabel* other = &s_labels[placed[j]];
            repeat = 0 == strcmp(label->text, other->text) &&
                     IMAP_LABEL_REPEAT_PX > Vector2Distance(label->at, other->at);
        }
        Vector2 size = map_marker_label_size(label->text, MAP_DETAIL_FONT_LABEL, label->arrow, label->value);
        Rectangle box;
        if (repeat || !place_label(label->at, size, label->gap, bounds, taken, count, marks, label->leads, &box))
            continue;
        taken[count++] = box;
        placed[placed_count++] = i;
        map_marker_draw_label(box, label->text, MAP_DETAIL_FONT_LABEL, label->color, label->arrow, label->strong,
                              label->value, fade * label->alpha);
    }
}

/* The selection and the hover frame their mark; a touch shows no hover. */
static void draw_mark_focus(const MapHit* hover, float fade) {
    if (MAP_DETAIL_PORTAL == s_m.selected.kind || MAP_DETAIL_PLACE == s_m.selected.kind ||
        MAP_DETAIL_SPAWN == s_m.selected.kind) {
        const MapHit* mark = find_hit(s_m.selected.kind, s_m.selected.index);
        if (NULL != mark) map_marker_draw_focus(mark->rect, IMAP_SELECTED, fade);
    }
    if (NULL != hover && (hover->kind != s_m.selected.kind || hover->index != s_m.selected.index))
        map_marker_draw_focus(hover->rect, WHITE, fade * 0.6f);
}

/* A new selection leaves its detail panel once: the camera moves the selected
 * mark, or else the selected node, the shorter way out, right of the panel or above it. */
static void keep_clear_of(Rectangle panel) {
    if (!s_m.clear_pending || s_m.pressed || s_m.pinching || 0.01f < fabsf(s_m.zoom - s_m.zoom_target)) return;
    const MapHit* mark = VIEW_WORLD == s_m.view ? find_hit(s_m.selected.kind, s_m.selected.index) : NULL;
    Rectangle card = NULL != mark ? mark->rect : node_rect(s_m.selected_node);
    card.x += s_m.pan_target.x - s_m.pan.x;
    card.y += s_m.pan_target.y - s_m.pan.y;
    s_m.clear_pending = false;
    if (!CheckCollisionRecs(card, panel)) return;
    float right = panel.x + panel.width + 16.0f - card.x;
    float up = card.y + card.height + 16.0f - panel.y;
    if (right < up) s_m.pan_target.x += right;
    else s_m.pan_target.y -= up;
}

/* The opaque fill of the map canvas and of its header. */
static Color canvas_color(void) {
    Color c = MODAL_PANEL_BG;
    c.a = 255;
    return c;
}

/* Top of the room a detail card may take: under the header. */
static float detail_card_top(void) {
    return header_rect().y + IMAP_HEADER_H + IMAP_CARD_MARGIN;
}

/* A detail card of height `h` at the bottom left, over the hint line. A card
 * taller than the room keeps its top under the header. */
static Rectangle detail_card_rect(float h) {
    float w = fminf(IMAP_CARD_W, s_m.panel.width - 2.0f * IMAP_CARD_MARGIN);
    float bottom = s_m.panel.y + s_m.panel.height - IMAP_HINT_H - IMAP_CARD_MARGIN;
    float top = fmaxf(detail_card_top(), bottom - h);
    return (Rectangle){ s_m.panel.x + IMAP_CARD_MARGIN, top, w, bottom - top };
}

/* Where the detail card of the world view shows this frame; zero when nothing is selected. */
static Rectangle info_card_rect(void) {
    if (MAP_DETAIL_NONE == s_m.selected.kind) return (Rectangle){ 0 };
    return detail_card_rect(map_detail_height(s_m.selected));
}

/* The detail card of the world view in `card`: the selected map, portal, place or followed objective. */
static void draw_info_panel(Rectangle card, float fade) {
    s_card = card;
    if (MAP_DETAIL_NONE == s_m.selected.kind) return;
    keep_clear_of(card);
    map_detail_draw(card, s_m.selected, fade);
}

/* While a quest is followed and its card is away, a button in the corner takes the view back to it. */
static Rectangle follow_button_rect(void) {
    if (MAP_DETAIL_OBJECTIVE == s_m.selected.kind || NULL == quest_target_tracked()) return (Rectangle){ 0 };
    float w = 132.0f;
    return (Rectangle){ s_m.panel.x + s_m.panel.width - IMAP_CARD_MARGIN - w,
                        s_m.panel.y + s_m.panel.height - IMAP_HINT_H - IMAP_CARD_MARGIN - IMAP_CARD_BTN_H, w,
                        IMAP_CARD_BTN_H };
}

static void draw_follow_button(Rectangle button) {
    s_follow_btn = button;
    if (0.0f >= button.width) return;
    UIButtonPixelRetroStyle style = { .bg = MODAL_BUTTON, .icon_id = "target", .label = "OBJECTIVE", .font_size = 12,
                                      .text_color = MODAL_QUEST, .enabled = true };
    ui_button_pixel_retro_draw(button, &style, !s_m.touch && ui_button_hit(button, GetMouseX(), GetMouseY()));
}

/* ── Universe view ──────────────────────────────────────────────────────── */

static Color umap_status_color(UmapStatus status) {
    if (universe_map_data_stale()) return UMAP_COLOR_OFFLINE;
    switch (status) {
        case UMAP_AVAILABLE: return UMAP_COLOR_ONLINE;
        case UMAP_DRAINING:  return UMAP_COLOR_CLOSING;
        case UMAP_OFFLINE:
        default:             return UMAP_COLOR_OFFLINE;
    }
}

static const char* umap_status_label(UmapStatus status) {
    if (universe_map_data_stale()) return "UNKNOWN";
    switch (status) {
        case UMAP_AVAILABLE: return "ONLINE";
        case UMAP_DRAINING:  return "CLOSING";
        case UMAP_OFFLINE:
        default:             return "OFFLINE";
    }
}

/* What the travel control of a world says. */
static const char* umap_travel_label(UmapTravel travel) {
    switch (travel) {
        case UMAP_TRAVEL_OPEN:       return "TRAVEL";
        case UMAP_TRAVEL_HERE:       return "YOU ARE HERE";
        case UMAP_TRAVEL_NO_ROUTE:   return "NO DIRECT LINK FROM HERE";
        case UMAP_TRAVEL_NOT_SERVED: return "NOT IN THIS RELEASE";
        case UMAP_TRAVEL_OFFLINE:    return "WORLD OFFLINE";
        case UMAP_TRAVEL_DRAINING:   return "WORLD CLOSING";
        case UMAP_TRAVEL_STALE:      return "CHECKING AVAILABILITY";
    }
    return "";
}

/* How far a ray from the centre of a card runs inside it, plus `margin`. */
static float card_reach(Rectangle card, Vector2 dir, float margin) {
    float x = 0.0f != dir.x ? (card.width * 0.5f + margin) / fabsf(dir.x) : FLT_MAX;
    float y = 0.0f != dir.y ? (card.height * 0.5f + margin) / fabsf(dir.y) : FLT_MAX;
    return fminf(x, y);
}

/* A link between two worlds; a one-way link ends in an arrow. The links of the
 * route to the selected world lead, wide, with a pulse and an arrow the way the
 * route travels them, and the other links step back; with no selection the
 * links of the player's world lead. */
static void draw_route(const UmapLink* link, int idx, float fade, double t) {
    const UmapGraph* u = universe_map_data_graph();
    bool selected = 0 <= s_m.selected_node, route = s_umap_route[idx];
    bool own = link->source == u->current || link->target == u->current;
    Color base = route || (!selected && own) ? MAP_MARKER_ROUTE : fade_c(MAP_MARKER_ROUTE, selected ? 0.3f : 0.55f);
    bool reverse = route && s_umap_reverse[idx];
    Vector2 a = node_center(reverse ? link->target : link->source);
    Vector2 b = node_center(reverse ? link->source : link->target);
    float length = Vector2Distance(a, b);
    if (1.0f > length) return;
    Vector2 dir = Vector2Scale(Vector2Subtract(b, a), 1.0f / length);
    float start = card_reach(node_rect(reverse ? link->target : link->source), dir, 6.0f);
    float end = card_reach(node_rect(reverse ? link->source : link->target), dir, 6.0f);
    if (length <= start + end) return;
    Vector2 from = Vector2Add(a, Vector2Scale(dir, start));
    Vector2 to = Vector2Subtract(b, Vector2Scale(dir, end));
    if (route) {
        draw_pixel_diagonal(from, to, base, 5, fade);
        map_marker_draw_pulse(pixel_path_point(from, to, (float)fmod(t * 0.45 + idx * 0.23, 1.0)), base, fade);
    } else {
        draw_thin_link(from, to, base, false, fade);
    }
    if (route || !link->bidirectional)
        map_marker_draw_arrow(Vector2Subtract(to, Vector2Scale(dir, 10.0f)), dir, 10.0f, base, fade);
}

/* A status pill: a square of the status colour and its word. */
static void draw_status_pill(float x, float y, UmapStatus status, float fade) {
    const char* label = umap_status_label(status);
    int width = MeasureText(label, 10);
    Rectangle pill = { floorf(x), floorf(y), (float)width + 18.0f, 16.0f };
    DrawRectangleRec(pill, fade_c((Color){ 4, 8, 16, 225 }, fade));
    DrawRectangle((int)pill.x + 4, (int)pill.y + 5, 6, 6, fade_c(umap_status_color(status), fade));
    shadow_label(label, (int)pill.x + 13, (int)pill.y + 3, 10, fade_c(IMAP_TEXT, fade));
}

/* A world card takes the outlines of a map; the player's world shows the player mark. */
static void draw_world_card(int idx, float fade, double t) {
    const UmapGraph* u = universe_map_data_graph();
    const UmapNode*  n = &u->nodes[idx];
    Rectangle card = node_rect(idx);
    bool selected = idx == s_m.selected_node;
    bool here = idx == u->current;
    bool hovered = CheckCollisionPointRec(GetMousePosition(), card);
    float shade = n->served ? fade : fade * 0.5f;

    draw_picture(card, (Vector2){ n->extent_w, n->extent_h }, n->preview_url, n->name, FETCH_P2, shade);
    if (here) draw_pixel_outline(card, 2, fade_c(MAP_MARKER_PLAYER, shade));
    if (selected) draw_pixel_outline(ui_rect_inset(card, here ? 2.0f : 0.0f), 2, fade_c(IMAP_SELECTED, shade));
    else if (hovered) draw_pixel_outline(ui_rect_inset(card, here ? 2.0f : 0.0f), 1, fade_c(IMAP_HOVER, shade));
    draw_status_pill(card.x + 5.0f, card.y + 5.0f, n->status, fade);
    draw_node_name(n->name, card, selected, fade);
    if (here) {
        float r = MAP_MARKER_PLAYER_MIN_R;
        map_marker_draw_player((Vector2){ card.x + card.width - r - 12.0f, card.y + r + 12.0f }, r,
                               local_player_view_facing(), fade, t);
    }
}

#define UMAP_CARD_TEXT_H (MAP_DETAIL_HEADER_H + MAP_DETAIL_PAD * 3.0f + MAP_DETAIL_LINE_H * 6.0f + IMAP_CARD_BTN_H)
#define UMAP_PICTURE_MIN_H 40.0f

/* The thumbnail its author set for a world; the card already shows its preview.
 * It takes only the room the text leaves, and none under UMAP_PICTURE_MIN_H. */
static float universe_picture_h(const UmapNode* n) {
    if ('\0' == n->thumbnail_url[0]) return 0.0f;
    float width = fminf(IMAP_CARD_W, s_m.panel.width - 2.0f * IMAP_CARD_MARGIN) - MAP_DETAIL_PAD * 2.0f;
    float bottom = s_m.panel.y + s_m.panel.height - IMAP_HINT_H - IMAP_CARD_MARGIN;
    float room = bottom - detail_card_top() - UMAP_CARD_TEXT_H - MAP_DETAIL_PAD;
    float h = fminf(fminf(width * 0.45f, s_m.panel.height * 0.22f), room);
    return UMAP_PICTURE_MIN_H <= h ? h : 0.0f;
}

/* The detail card of a world: its name, thumbnail, status, route, summary, entry spawn and description over TRAVEL. */
static Rectangle universe_panel_rect(const UmapNode* n) {
    float picture_h = universe_picture_h(n);
    return detail_card_rect(UMAP_CARD_TEXT_H + (0.0f < picture_h ? picture_h + MAP_DETAIL_PAD : 0.0f));
}

static void draw_universe_panel(float fade) {
    const UmapGraph* u = universe_map_data_graph();
    s_card = s_travel_btn = (Rectangle){ 0 };
    if (s_m.selected_node < 0 || s_m.selected_node >= u->node_count) return;
    const UmapNode* n = &u->nodes[s_m.selected_node];
    UmapTravel travel = universe_map_data_travel(s_m.selected_node);

    Rectangle r = universe_panel_rect(n);
    keep_clear_of(r);
    s_card = r;
    map_detail_card(r, n->name, fade);

    float y = r.y + MAP_DETAIL_HEADER_H + MAP_DETAIL_PAD;
    float picture_h = universe_picture_h(n);
    if (0.0f < picture_h) {
        Rectangle picture = { r.x + MAP_DETAIL_PAD, y, r.width - MAP_DETAIL_PAD * 2.0f, picture_h };
        draw_picture(picture, (Vector2){ picture.width, picture.height }, n->thumbnail_url, n->name, FETCH_P1, fade);
        y += picture_h + MAP_DETAIL_PAD;
    }

    float mark = 8.0f;
    DrawRectangle((int)(r.x + MAP_DETAIL_PAD), (int)(y + (MAP_DETAIL_LINE_H - mark) * 0.5f), (int)mark, (int)mark,
                  fade_c(umap_status_color(n->status), fade));
    const char* status = universe_map_data_stale() ? "AVAILABILITY UNKNOWN" : umap_status_label(n->status);
    map_detail_line(r, y, mark + 6.0f, NULL, status, MAP_DETAIL_FONT_BODY, MODAL_TEXT, fade);
    y += MAP_DETAIL_LINE_H;

    int hops = s_umap_hops[s_m.selected_node];
    char route[IMAP_NAME_MAX + 40];
    if (0 == hops) snprintf(route, sizeof route, "YOU ARE HERE");
    else if (1 == hops) snprintf(route, sizeof route, "DIRECT ROUTE FROM YOUR WORLD");
    else if (0 < hops) snprintf(route, sizeof route, "ROUTE: %d JUMPS - FIRST TO %s", hops, u->nodes[s_umap_next].name);
    else snprintf(route, sizeof route, "NO KNOWN ROUTE FROM YOUR WORLD");
    map_detail_line(r, y, 0.0f, NULL, route, MAP_DETAIL_FONT_LABEL, 0 <= hops ? MAP_MARKER_ROUTE : MODAL_LABEL, fade);
    y += MAP_DETAIL_LINE_H;
    char routes[160] = "", from[160] = "";
    for (int i = 0; u->link_count > i; ++i) {
        const UmapLink* link = &u->links[i];
        bool out = s_m.selected_node == link->source, in = s_m.selected_node == link->target;
        if (out || (in && link->bidirectional))
            snprintf(routes + strlen(routes), sizeof routes - strlen(routes), "%s%s", '\0' == routes[0] ? "" : ", ",
                     u->nodes[out ? link->target : link->source].name);
        else if (in)
            snprintf(from + strlen(from), sizeof from - strlen(from), "%s%s", '\0' == from[0] ? "" : ", ",
                     u->nodes[link->source].name);
    }
    char line[360];
    snprintf(line, sizeof line, "%s%s%s%s%s", '\0' != routes[0] ? "ROUTES TO " : "", routes,
             '\0' != routes[0] && '\0' != from[0] ? " - " : "", '\0' != from[0] ? "ONE WAY FROM " : "", from);
    map_detail_line(r, y, 0.0f, "forward", '\0' != line[0] ? line : "NO ROUTES", MAP_DETAIL_FONT_LABEL,
                    MAP_MARKER_ROUTE, fade);
    y += MAP_DETAIL_LINE_H;
    char summary[IMAP_CODE_MAX + 24];
    snprintf(summary, sizeof summary, "%d MAPS - %s", n->map_count, n->instance_code);
    map_detail_line(r, y, 0.0f, NULL, summary, MAP_DETAIL_FONT_LABEL, MODAL_LABEL, fade);
    y += MAP_DETAIL_LINE_H;
    char entry[IMAP_NAME_MAX + 32], text[IMAP_NAME_MAX + 40];
    map_detail_entry_text(&n->entry, entry, sizeof entry);
    if ('\0' != entry[0]) {
        snprintf(text, sizeof text, "ENTRY: %s", entry);
        map_detail_line(r, y, 0.0f, "home", text, MAP_DETAIL_FONT_LABEL,
                        IMAP_SPAWN_FIXED == n->entry.mode ? MAP_MARKER_SPAWN : MODAL_LABEL, fade);
        y += MAP_DETAIL_LINE_H;
    }
    map_detail_line(r, y, 0.0f, NULL, '\0' != n->description[0] ? n->description : "No description available.",
                    MAP_DETAIL_FONT_LABEL, MODAL_LABEL, fade);

    s_travel_btn = (Rectangle){ r.x + MAP_DETAIL_PAD, r.y + r.height - MAP_DETAIL_PAD - IMAP_CARD_BTN_H,
                                r.width - MAP_DETAIL_PAD * 2.0f, IMAP_CARD_BTN_H };
    bool open = UMAP_TRAVEL_OPEN == travel;
    UIButtonPixelRetroStyle style = {
        .bg = open ? MODAL_ACCEPT : MODAL_BUTTON, .icon_id = open ? "forward" : NULL,
        .label = umap_travel_label(travel), .font_size = 14, .text_color = open ? WHITE : MODAL_LABEL,
        .enabled = open,
    };
    ui_button_pixel_retro_draw(s_travel_btn, &style, open && ui_button_hit(s_travel_btn, GetMouseX(), GetMouseY()));
}

/* A view without a graph: its state centred on the canvas. Loading blinks; an
 * error offers RETRY. */
static void draw_view_status(const char* message, ImapDataState state, float fade, double t) {
    float blink = IMAP_DATA_LOADING == state ? 0.6f + 0.4f * sinf((float)t * 3.0f) : 1.0f;
    Vector2 c = ui_rect_center(s_m.panel);
    shadow_label(message, (int)(c.x - MeasureText(message, 14) / 2.0f), (int)c.y, 14, fade_c(IMAP_TEXT, fade * blink));
    if (IMAP_DATA_ERROR != state) return;
    s_retry_btn = (Rectangle){ floorf(c.x - 70.0f), c.y + 28.0f, 140.0f, IMAP_CARD_BTN_H };
    UIButtonPixelRetroStyle style = { .bg = MODAL_BUTTON, .icon_id = "reload", .label = "RETRY", .font_size = 14,
                                      .text_color = MODAL_TEXT, .enabled = true };
    ui_button_pixel_retro_draw(s_retry_btn, &style, ui_button_hit(s_retry_btn, GetMouseX(), GetMouseY()));
}

static void draw_universe(float fade, double t) {
    const UmapGraph* u = universe_map_data_graph();
    ImapDataState state = universe_map_data_state();
    if (IMAP_DATA_READY != state) {
        draw_view_status(IMAP_DATA_ERROR == state ? "UNIVERSE UNAVAILABLE" : "LOADING UNIVERSE...", state, fade, t);
        return;
    }
    if (0 == u->node_count) {
        draw_view_status("NO WORLDS IN THIS UNIVERSE", state, fade, t);
        return;
    }
    /* Routes → cards: a route runs under the cards it joins. */
    for (int i = 0; i < u->link_count; ++i) draw_route(&u->links[i], i, fade, t);
    for (int i = 0; i < u->node_count; ++i) draw_world_card(i, fade, t);
    draw_universe_panel(fade);
}

/* The header: the view tabs, the name of the view and the close button on a
 * modal header strip; then the hint line. The header fades in under a veil. */
static void draw_header(float fade) {
    const ImapGraph* gr = instance_map_data_graph();
    const UmapGraph* u = universe_map_data_graph();
    bool universe = VIEW_UNIVERSE == s_m.view;

    Rectangle header = header_rect();
    DrawRectangleRec(header, canvas_color());
    DrawRectangleRec(header, (Color){ MODAL_PANEL_BORDER.r, MODAL_PANEL_BORDER.g, MODAL_PANEL_BORDER.b, 40 });
    DrawRectangle((int)header.x, (int)(header.y + header.height) - 1, (int)header.width, 1, MODAL_PANEL_BORDER);

    UITabs tabs = view_tabs(header);
    ui_tabs_draw(&tabs, IMAP_VIEW_TABS, s_m.view);
    Rectangle close = close_rect(header);
    ui_button_draw_close(close);

    char sub[IMAP_NAME_MAX + 24];
    switch (universe ? universe_map_data_state() : instance_map_data_state()) {
        case IMAP_DATA_READY:
            if (universe) snprintf(sub, sizeof(sub), "%s", u->name[0] ? u->name : u->code);
            else snprintf(sub, sizeof(sub), "%s", gr->name[0] ? gr->name : gr->instance_code);
            break;
        case IMAP_DATA_LOADING: snprintf(sub, sizeof(sub), "SCANNING TOPOLOGY..."); break;
        case IMAP_DATA_ERROR:   snprintf(sub, sizeof(sub), "UPLINK UNAVAILABLE");   break;
        default:                sub[0] = '\0';                                       break;
    }
    Rectangle last = ui_tabs_rect(&tabs, VIEW_COUNT - 1);
    float sub_x = last.x + last.width + IMAP_HEADER_PAD;
    float sub_end = close.x - MODAL_HEADER_TITLE_GAP;

    /* Where the player is, apart from what the view inspects: a red dot, the map and the cell. */
    char where[IMAP_NAME_MAX * 2 + 32], map[IMAP_NAME_MAX * 2];
    int node = IMAP_DATA_READY == instance_map_data_state() ? instance_map_data_find_node(g_local_player.map_code) : -1;
    if (0 <= node) instance_map_node_label(gr, node, map, sizeof map);
    else copy_str(map, sizeof map, g_local_player.map_code);
    Vector2 pos = game_state_own_render()->interp_pos;
    snprintf(where, sizeof where, "YOU: %s (%d, %d)", map, (int)pos.x, (int)pos.y);
    float chip_w = (float)MeasureText(where, MAP_DETAIL_FONT_LABEL) + 18.0f;
    if (chip_w + 120.0f <= sub_end - sub_x) {
        sub_end -= chip_w + IMAP_HEADER_PAD;
        float chip_x = close.x - MODAL_HEADER_TITLE_GAP - chip_w, cy = header.y + header.height * 0.5f;
        DrawCircleV((Vector2){ chip_x + 5.0f, cy }, 4.0f, MAP_MARKER_PLAYER);
        DrawText(where, (int)(chip_x + 14.0f), (int)(cy - MAP_DETAIL_FONT_LABEL * 0.5f), MAP_DETAIL_FONT_LABEL,
                 MODAL_LABEL);
    }
    modal_draw_clipped_text(sub, (int)sub_x, (int)(header.y + (header.height - MAP_DETAIL_FONT_TITLE) * 0.5f),
                            (int)(sub_end - sub_x), MAP_DETAIL_FONT_TITLE, MODAL_TEXT);

    if (fade < 1.0f) {
        Color veil = canvas_color();
        veil.a = (unsigned char)(255.0f * (1.0f - fade));
        DrawRectangleRec(header, veil);
    }

    const char* hint = universe ? "DRAG PAN - WHEEL / PINCH ZOOM - TAP A WORLD - ARROW: ONE-WAY ROUTE"
                                : "DRAG PAN - WHEEL / PINCH ZOOM - TAP A MARK OR A MAP - A GROUP ZOOMS IN";
    int hw = MeasureText(hint, 10);
    shadow_label(hint, (int)(s_m.panel.x + s_m.panel.width - hw - IMAP_CARD_MARGIN),
                 (int)(s_m.panel.y + s_m.panel.height - IMAP_HINT_H), 10, fade_c(MODAL_LABEL, fade * 0.9f));
}

static Rectangle lerp_rect(Rectangle a, Rectangle b, float t) {
    return (Rectangle){
        a.x + (b.x - a.x) * t,
        a.y + (b.y - a.y) * t,
        a.width + (b.width - a.width) * t,
        a.height + (b.height - a.height) * t,
    };
}

void modal_instance_map_draw(int screen_width, int screen_height) {
    /* The container morph (modal_map_expand_progress) doubles as the
     * open/close transition: keep drawing during the retract until the
     * container has fully returned to the compact readout. */
    float mt = modal_map_expand_progress();
    if (!s_m.open && mt <= 0.004f) return;

    double t = GetTime();

    /* Container: modal_map's compact box grows to the entire screen. */
    Rectangle compact = modal_map_bounds();
    Rectangle full    = { 0, 0, (float)screen_width, (float)screen_height };
    s_m.panel = lerp_rect(compact, full, mt);

    float chrome  = mt;                                   /* panel shell   */
    float content = (mt - 0.55f) / 0.45f;                 /* graph + text  */
    if (content < 0.0f) content = 0.0f;
    if (content > 1.0f) content = 1.0f;

    if (s_m.panel.width < 4.0f || s_m.panel.height < 4.0f) return;
    DrawRectangleRec(s_m.panel, fade_c(canvas_color(), chrome));
    DrawRectangleLinesEx(s_m.panel, 1.0f, fade_c(MODAL_PANEL_BORDER, chrome));
    /* The graph never draws under the translucent toolbar. */
    float clip_top = fmaxf(s_m.panel.y, toolbar_height());
    BeginScissorMode((int)s_m.panel.x, (int)clip_top, (int)s_m.panel.width,
                     (int)fmaxf(0.0f, s_m.panel.y + s_m.panel.height - clip_top));

    draw_backdrop(chrome);

    s_retry_btn = s_follow_btn = (Rectangle){ 0 };
    const ImapGraph* gr = instance_map_data_graph();
    ImapDataState state = instance_map_data_state();
    if (content > 0.0f && VIEW_UNIVERSE == s_m.view) {
        draw_universe(content, t);
    } else if (content > 0.0f && IMAP_DATA_READY == state) {
        /* Rim → pictures → map outlines → links, the one in focus last → gates
         * and link ends → names, bots, resources and places → the entry spawn →
         * quest targets → labels → the frame of the selection → the player: each
         * layer lands on the one under it. A hover reads the marks of the last
         * frame; a touch shows none. */
        Vector2 pointer = GetMousePosition();
        const MapHit* last = s_m.touch ? NULL : hit_at(pointer);
        MapHit hover = NULL != last ? *last : (MapHit){ .kind = MAP_DETAIL_NONE };
        const MapHit* hovered = NULL != last ? &hover : NULL;
        s_hit_count = 0;
        draw_surface_rim(content);
        for (int i = 0; i < gr->node_count; ++i) {
            const ImapNode* n = &gr->nodes[i];
            draw_picture(node_rect(i), (Vector2){ n->extent_w, n->extent_h }, n->preview_url, n->name, FETCH_P1,
                         content);
        }
        int hovered_node = s_m.touch || NULL != hovered ? -1 : node_at_point(pointer.x, pointer.y);
        mark_targets();
        for (int i = 0; i < gr->node_count; ++i) draw_node_marks(i, hovered_node, content, t);
        for (int layer = 0; 3 > layer; ++layer)
            for (int e = 0; e < gr->edge_count; ++e) {
                LinkEmphasis emphasis = link_emphasis(e, hovered);
                if (layer == (LINK_ROUTE > emphasis ? 0 : LINK_ROUTE == emphasis ? 1 : 2))
                    draw_edge_line(&gr->edges[e], e, emphasis, content, t);
            }
        for (int e = 0; e < gr->edge_count; ++e)
            draw_edge_ends(&gr->edges[e], e, link_emphasis(e, hovered), content, t);
        for (int i = 0; i < gr->node_count; ++i) draw_node_overlay(i, content, t);
        draw_entry_spawn(content);
        draw_quest_targets(content, t);
        Rectangle canvas = { s_m.panel.x, detail_card_top() - IMAP_CARD_MARGIN, s_m.panel.width,
                             s_m.panel.y + s_m.panel.height - detail_card_top() + IMAP_CARD_MARGIN - IMAP_HINT_H };
        Rectangle card = info_card_rect(), button = follow_button_rect();
        draw_labels(hovered, canvas, card, button, content);
        draw_mark_focus(hovered, content);
        draw_player_marker(content, t);
        draw_info_panel(card, content);
        draw_follow_button(button);
    } else if (content > 0.0f) {
        const char* message = IMAP_DATA_ERROR == state ? "INSTANCE INTELLIGENCE UNAVAILABLE"
                                                        : "LOADING INSTANCE INTELLIGENCE...";
        draw_view_status(message, state, content, t);
    }

    EndScissorMode();

    if (content > 0.05f) draw_header(content);
}
