#include "modal_instance_map.h"
#include "graph_layout.h"
#include "ease.h"

#include "data/instance_map_data.h"
#include "data/universe_map_data.h"
#include "modal.h"
#include "modal_map.h"
#include "session_gate.h"
#include "text.h"
#include "toolbar.h"
#include "ui_button.h"
#include "ui_icon.h"
#include "ui_rect.h"

#include "sim/local_player.h"
#include "content/presentation_runtime.h"
#include "sim/game_state.h"
#include "input/input.h"
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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ── Presentation constants ────────────────────────────────────────────── */
#define IMAP_NODE_SIDE      88.0f
#define IMAP_CAPABILITY_REVEAL_CELL 9.0f   /* px of one map cell where capability tabs show */
#define IMAP_ZOOM_MIN       0.45f
#define IMAP_ZOOM_INIT      5.0f
#define IMAP_ZOOM_MAX_FLOOR 3.2f
#define IMAP_ZOOM_STEP      1.15f
#define IMAP_DRAG_SLOP_PX   9.0f
#define IMAP_CAM_LAMBDA     11.0f   /* exp smoothing rate for pan/zoom      */
#define IMAP_MAP_GAP        0.025f  /* maps on layout cells nearly touch       */
#define UMAP_WORLD_GAP      0.2f    /* room for the routes between worlds      */

static const Color IMAP_BG        = { 6, 10, 22, 205 };
static const Color IMAP_GRID_LINE = { 60, 140, 190, 26 };
static const Color IMAP_EDGE      = { 60, 200, 255, 255 };
static const Color IMAP_EDGE_INTRA= { 170, 110, 255, 255 };
static const Color IMAP_NODE_LINE = { 90, 210, 250, 220 };
static const Color IMAP_SELECTED  = { 255, 210, 90, 255 };
static const Color IMAP_PLAYER    = { 110, 255, 150, 255 };
static const Color IMAP_TEXT      = { 205, 225, 245, 235 };
static const Color IMAP_TEXT_DIM  = { 130, 150, 180, 190 };
static const Color UMAP_ROUTE     = { 255, 190, 90, 255 };
static const Color UMAP_COLOR_ONLINE  = { 110, 255, 150, 255 };
static const Color UMAP_COLOR_CLOSING = { 255, 200, 80, 255 };
static const Color UMAP_COLOR_OFFLINE = { 150, 155, 170, 255 };

/* The two views of the container: the maps of this world, and the worlds of its universe. */
typedef enum { VIEW_WORLD = 0, VIEW_UNIVERSE } MapView;

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
    Vector2 centers[IMAP_MAX_NODES];
    Vector2 span;                        /* of the packed graph, in cells */
    int viewport_w, viewport_h;          /* the screen the view was framed for */
    int selected_node;    /* -1 = none, in the open view     */
    char selected_code[IMAP_CODE_MAX];
    int layout_generation;
} ModalInstanceMap;

static ModalInstanceMap s_m = {0};

static Rectangle s_close_btn;
static Rectangle s_world_tab, s_universe_tab, s_travel_btn;

/* ── Lifecycle ──────────────────────────────────────────────────────────── */

void modal_instance_map_init(void) {
    memset(&s_m, 0, sizeof(s_m));
    s_m.zoom = s_m.zoom_target = IMAP_ZOOM_INIT;
    s_m.selected_node = -1;
    s_close_btn = (Rectangle){ 0 };
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
    s_m.open = true;
    s_m.view = VIEW_WORLD;
    s_m.universe_open = false;
    s_m.framed = false;
    s_m.selected_code[0] = '\0';
    s_m.selected_node = -1;
    s_m.pressed = s_m.dragging = s_m.pinching = false;
    s_close_btn = (Rectangle){ 0 };
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

/* Packs the open view on the layout cells of the editor, its empty rows and columns removed. */
static void pack_view(void) {
    if (VIEW_UNIVERSE != s_m.view) {
        s_m.span = instance_map_layout(instance_map_data_graph(), IMAP_MAP_GAP, s_m.centers);
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

/* The screen the view frames its graph in: under the header, over the bottom margin. */
static Rectangle frame_area(void) {
    float top = toolbar_height() + 64.0f;
    return (Rectangle){ 20.0f, top, (float)GetScreenWidth() - 40.0f, (float)GetScreenHeight() - 36.0f - top };
}

/* The most pixels one layout cell spans with a span in the frame area. */
static float fit_side(Vector2 span) {
    Rectangle area = frame_area();
    return fminf(area.width / fmaxf(span.x, 1.0f), area.height / fmaxf(span.y, 1.0f));
}

static Vector2 node_center(int idx) {
    Vector2 c = ui_rect_center(s_m.panel);
    float side = node_screen_side();
    return (Vector2){ c.x + s_m.pan.x + s_m.centers[idx].x * side, c.y + s_m.pan.y + s_m.centers[idx].y * side };
}

static float node_screen_radius(void) {
    return node_screen_side() * 0.5f;
}

/* A map card has the proportions of its map, a world card those of its
 * preview. A map card sits on whole pixels, so two joined maps meet. */
static Rectangle node_rect(int idx) {
    Vector2 p = node_center(idx);
    float side = node_screen_side();
    if (VIEW_UNIVERSE == s_m.view) {
        const UmapNode* u = &universe_map_data_graph()->nodes[idx];
        return (Rectangle){ p.x - u->extent_w * side * 0.5f, p.y - u->extent_h * side * 0.5f,
                            u->extent_w * side, u->extent_h * side };
    }
    Rectangle r = instance_map_node_rect(&instance_map_data_graph()->nodes[idx], p, side);
    float left = roundf(r.x), top = roundf(r.y);
    return (Rectangle){ left, top, roundf(r.x + r.width) - left, roundf(r.y + r.height) - top };
}

static int node_at_point(float x, float y) {
    int hit = -1;
    float best = 1e9f;
    for (int i = 0; i < view_node_count(); ++i) {
        if (!CheckCollisionPointRec((Vector2){ x, y }, node_rect(i))) continue;
        Vector2 p = node_center(i);
        float dx = x - p.x, dy = y - p.y;
        float d2 = dx * dx + dy * dy;
        if (d2 < best) { best = d2; hit = i; }
    }
    return hit;
}

/* ── Camera controls ────────────────────────────────────────────────────── */

static float imap_zoom_max(void) {
    float w = s_m.panel.width > 4.0f ? s_m.panel.width : (float)GetScreenWidth();
    float max = 4.0f * w / IMAP_NODE_SIDE;
    return max > IMAP_ZOOM_MAX_FLOOR ? max : IMAP_ZOOM_MAX_FLOOR;
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

/* Fits the packed graph under the header, never closer than IMAP_ZOOM_INIT. */
static void frame_view(void) {
    pack_view();
    Rectangle area = frame_area();
    s_m.zoom = s_m.zoom_target = Clamp(fit_side(s_m.span) / IMAP_NODE_SIDE, IMAP_ZOOM_MIN, IMAP_ZOOM_INIT);
    s_m.pan = s_m.pan_target = (Vector2){ 0.0f, area.y + area.height * 0.5f - (float)GetScreenHeight() * 0.5f };
    s_m.framed = true;
    s_m.viewport_w = GetScreenWidth();
    s_m.viewport_h = GetScreenHeight();
}

/* Shows the other view; the universe data loads the first time it shows. */
static void select_view(MapView view) {
    if (view == s_m.view) return;
    s_m.view = view;
    s_m.selected_node = -1;
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

/* Release resolution: a clean (non-drag) press acts on what it landed on. */
static void resolve_click(Vector2 at) {
    if (CheckCollisionPointRec(at, s_close_btn)) {
        modal_instance_map_close();
        return;
    }
    if (CheckCollisionPointRec(at, s_world_tab)) {
        select_view(VIEW_WORLD);
        return;
    }
    if (CheckCollisionPointRec(at, s_universe_tab)) {
        select_view(VIEW_UNIVERSE);
        return;
    }
    if (VIEW_UNIVERSE == s_m.view && 0 <= s_m.selected_node && CheckCollisionPointRec(at, s_travel_btn)) {
        travel_to_selected();
        return;
    }
    int hit = node_at_point(at.x, at.y);
    s_m.clear_pending = hit != s_m.selected_node;
    s_m.selected_node = hit; /* tapping empty space deselects */
    if (VIEW_UNIVERSE == s_m.view)
        copy_str(s_m.selected_code, sizeof s_m.selected_code,
                  0 <= hit ? universe_map_data_graph()->nodes[hit].instance_code : "");
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

    /* A refetched graph keeps a selection that still names a node. */
    if (view_generation() != s_m.layout_generation) {
        s_m.layout_generation = view_generation();
        pack_view();
        if (VIEW_UNIVERSE == s_m.view) {
            const UmapGraph* graph = universe_map_data_graph();
            s_m.selected_node = -1;
            for (int i = 0; graph->node_count > i; i++)
                if (0 == strcmp(s_m.selected_code, graph->nodes[i].instance_code)) s_m.selected_node = i;
        }
        if (s_m.selected_node >= view_node_count()) s_m.selected_node = -1;
    }
    ImapDataState state = VIEW_UNIVERSE == s_m.view ? universe_map_data_state() : instance_map_data_state();
    if (IMAP_DATA_READY == state && (!s_m.framed || GetScreenWidth() != s_m.viewport_w ||
                                    GetScreenHeight() != s_m.viewport_h)) frame_view();

    track_pointer();

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

/* A picture: the texture its url loads, contained in `r` at its own aspect ratio,
 * else a dotted tile with the initials of the name, so a card never shows an
 * empty panel while it loads or when none exists. */
static void draw_picture(Rectangle r, const char* url, const char* name, FetchPriority priority, float fade) {
    Texture2D tex = '\0' != url[0] ? fetch_texture(url, 1, priority, true) : (Texture2D){ 0 };
    DrawRectangleRec(r, fade_c((Color){ 10, 18, 32, 255 }, fade));
    if (0 != tex.id) {
        DrawTexturePro(tex, (Rectangle){ 0, 0, (float)tex.width, (float)tex.height },
                       ui_rect_contain(r, (float)tex.width, (float)tex.height), (Vector2){ 0, 0 }, 0.0f,
                       Fade(WHITE, fade));
        return;
    }
    for (int y = 4; y < (int)r.height; y += 8)
        for (int x = 4 + (y / 8) % 2 * 4; x < (int)r.width; x += 8)
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

/* An outline on every side but the `joined` ones (IMAP_EDGE_* bits). */
static void draw_open_outline(Rectangle bounds, unsigned joined, int thickness, Color color) {
    if (bounds.width <= 0.0f || bounds.height <= 0.0f) return;
    if (!(joined & IMAP_EDGE_TOP)) DrawRectangle((int)bounds.x, (int)bounds.y, (int)bounds.width, thickness, color);
    if (!(joined & IMAP_EDGE_LEFT)) DrawRectangle((int)bounds.x, (int)bounds.y, thickness, (int)bounds.height, color);
    if (!(joined & IMAP_EDGE_BOTTOM))
        DrawRectangle((int)bounds.x, (int)(bounds.y + bounds.height - thickness), (int)bounds.width, thickness, color);
    if (!(joined & IMAP_EDGE_RIGHT))
        DrawRectangle((int)(bounds.x + bounds.width - thickness), (int)bounds.y, thickness, (int)bounds.height, color);
}

static void draw_pixel_outline(Rectangle bounds, int thickness, Color color) {
    draw_open_outline(bounds, 0, thickness, color);
}

static void draw_pixel_active_pulse(Rectangle bounds, Color accent, float fade, double time) {
    float wave = 0.5f + 0.5f * sinf((float)time * 5.0f);
    int offset = 5 + (int)(wave * 6.0f);
    Rectangle outer = { floorf(bounds.x) - offset, floorf(bounds.y) - offset,
                        floorf(bounds.width) + offset * 2.0f,
                        floorf(bounds.height) + offset * 2.0f };
    Color outer_color = accent;
    outer_color.a = (unsigned char)(120.0f * wave * fade);
    Color inner_color = accent;
    inner_color.a = (unsigned char)(220.0f * (0.55f + 0.45f * wave) * fade);
    draw_pixel_outline(outer, 3, outer_color);
    draw_pixel_outline(bounds, 3, inner_color);
    DrawRectangle((int)outer.x, (int)outer.y, 4, 4, inner_color);
    DrawRectangle((int)(outer.x + outer.width - 4.0f), (int)outer.y, 4, 4, inner_color);
    DrawRectangle((int)outer.x, (int)(outer.y + outer.height - 4.0f), 4, 4, inner_color);
    DrawRectangle((int)(outer.x + outer.width - 4.0f),
                  (int)(outer.y + outer.height - 4.0f), 4, 4, inner_color);
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

static void draw_pixel_diagonal(Vector2 a, Vector2 b, Color color, float fade) {
    float dx = b.x - a.x;
    float dy = b.y - a.y;
    int steps = (int)(fmaxf(fabsf(dx), fabsf(dy)) / 4.0f) + 1;
    if (steps > 384) steps = 384;
    Color shadow = fade_c((Color){ 0, 0, 0, 215 }, fade);
    Color core = fade_c(color, fade);
    Color light = fade_c((Color){ 225, 245, 255, 210 }, fade);
    DrawLineEx(a, b, 7.0f, shadow);
    DrawLineEx(a, b, 3.0f, core);
    DrawLineEx(a, b, 1.0f, light);
    int last_x = INT_MIN;
    int last_y = INT_MIN;
    for (int i = 0; i <= steps; i++) {
        float progress = (float)i / (float)steps;
        int x = (int)roundf(a.x + dx * progress);
        int y = (int)roundf(a.y + dy * progress);
        if (x == last_x && y == last_y) continue;
        DrawRectangle(x - 3, y - 3, 7, 7, shadow);
        DrawRectangle(x - 1, y - 1, 3, 3, core);
        DrawRectangle(x - 1, y - 1, 1, 1, light);
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

/* Direct diagonal pixel connection with a thicker travelling energy block. */
static void draw_edge(const ImapEdge* e, int idx, float fade, double t) {
    Color base = e->intra ? IMAP_EDGE_INTRA : IMAP_EDGE;

    /* Two joined maps show their link by the edge they share. */
    const ImapGraph* graph = instance_map_data_graph();
    if (0 != instance_map_shared_edges(&graph->nodes[e->source_node], &graph->nodes[e->target_node])) return;
    Vector2 a = edge_endpoint(e->source_node, e->source_cell_x, e->source_cell_y, t, idx * 2);
    Vector2 b = edge_endpoint(e->target_node, e->target_cell_x, e->target_cell_y, t, idx * 2 + 1);

    float dx = b.x - a.x, dy = b.y - a.y;
    if (dx * dx + dy * dy < 16.0f) {
        int px = (int)roundf(a.x);
        int py = (int)roundf(a.y);
        DrawRectangle(px - 8, py - 8, 17, 17, fade_c(BLACK, fade));
        DrawRectangle(px - 5, py - 5, 11, 11, fade_c(base, fade));
        DrawRectangle(px - 2, py - 2, 5, 5, Fade(WHITE, fade));
        return;
    }

    draw_pixel_diagonal(a, b, base, fade);

    float phase = (float)fmod(t * 0.55 + idx * 0.19, 1.0);
    Vector2 pulse = pixel_path_point(a, b, phase);
    int px = (int)pulse.x;
    int py = (int)pulse.y;
    DrawRectangle(px - 7, py - 7, 15, 15, fade_c(BLACK, fade));
    DrawRectangle(px - 5, py - 5, 11, 11, fade_c(base, fade));
    DrawRectangle(px - 2, py - 2, 5, 5, Fade(WHITE, fade));
}

/* Map cell → screen position inside the node card (the preview capture and
 * the gameplay grid share the same cell space, so POIs land where they are
 * on the real map). Cell centers use +0.5. */
/* POI marker size: grows with the card so zooming in scales the landmarks. */
static float poi_radius(float half_side) {
    float r = half_side * 0.14f;
    return r < 7.0f ? 7.0f : r;
}

static uint8_t presence_status_icon(ImapPresenceStatus presence) {
    switch (presence) {
        case IMAP_PRESENCE_PASSIVE:       return STATUS_ICON_PASSIVE;
        case IMAP_PRESENCE_HOSTILE:       return STATUS_ICON_HOSTILE;
        case IMAP_PRESENCE_RESOURCE:      return STATUS_ICON_RESOURCE;
        case IMAP_PRESENCE_PORTAL:        return STATUS_ICON_PORTAL;
        case IMAP_PRESENCE_PORTAL_RANDOM: return STATUS_ICON_PORTAL_RANDOM;
        case IMAP_PRESENCE_NONE:
        default:                          return STATUS_ICON_NONE;
    }
}

static void draw_presence_marker(Vector2 at, const char* icon, Color tint,
                                 float size, float fade) {
    float shell_size = floorf(size) + 6.0f;
    Rectangle shell = { floorf(at.x - shell_size * 0.5f), floorf(at.y - shell_size * 0.5f),
                        shell_size, shell_size };
    draw_pixel_panel(shell, (Color){ 10, 16, 30, 220 }, tint, false, fade);
    if (icon && '\0' != icon[0]) {
        ui_icon_draw_ex(icon, at.x, at.y, size, 0.0f, Fade(WHITE, fade));
        return;
    }
    float fallback_size = floorf(size * 0.55f);
    DrawRectangle((int)(at.x - fallback_size * 0.5f), (int)(at.y - fallback_size * 0.5f),
                  (int)fallback_size, (int)fallback_size, fade_c(tint, fade));
}

static void draw_zoomed_capability_tab(Vector2 at, const char* status_icon,
                                       uint8_t capabilities, bool action_active,
                                       bool quest_active, bool show_stats,
                                       int stats_sum, Color icon_tint,
                                       Color pulse_accent, float fade,
                                       double time) {
    int icon_size = 24;
    int font_size = 16;
    int stats_width = 0;
    bool show_status = status_icon && '\0' != status_icon[0];
    const char* capability_icons[2] = { 0 };
    Color capability_colors[2] = { 0 };
    int capability_count = 0;
    if (capabilities & IMAP_CAPABILITY_ACTION) {
        capability_icons[capability_count] =
            presentation_runtime_status_icon(STATUS_ICON_ACTION_PROVIDER);
        capability_colors[capability_count++] = action_active ? WHITE : IMAP_TEXT_DIM;
    }
    if (capabilities & IMAP_CAPABILITY_QUEST) {
        capability_icons[capability_count] =
            presentation_runtime_status_icon(STATUS_ICON_QUEST_PROVIDER);
        capability_colors[capability_count++] = quest_active ? MODAL_QUEST : IMAP_TEXT_DIM;
    }
    char stats[16] = { 0 };
    if (show_stats) {
        snprintf(stats, sizeof(stats), "%d", stats_sum);
        stats_width = MeasureText(stats, font_size);
    }

    float width = 0.0f;
    bool has_item = false;
    if (show_stats) {
        width += icon_size + 4.0f + stats_width;
        has_item = true;
    }
    if (show_status) {
        if (has_item) width += 6.0f;
        width += icon_size;
        has_item = true;
    }
    for (int i = 0; i < capability_count; ++i) {
        if (has_item) width += 4.0f;
        width += icon_size;
        has_item = true;
    }
    if (width < 30.0f) width = 30.0f;
    Rectangle tab = { floorf(at.x - width * 0.5f - 5.0f), floorf(at.y - icon_size * 0.5f - 5.0f),
                      width + 10.0f, icon_size + 10.0f };
    draw_pixel_panel(tab, (Color){ 12, 18, 34, 238 }, icon_tint, false, fade);
    if (action_active || quest_active)
        draw_pixel_active_pulse(tab, action_active ? MODAL_ACTION : MODAL_QUEST, fade, time);
    else if (0 != pulse_accent.a)
        draw_pixel_active_pulse(tab, pulse_accent, fade, time);

    float x = tab.x + 5.0f;
    has_item = false;
    if (show_stats) {
        ui_icon_draw_ex("stats", x + icon_size * 0.5f, at.y, (float)icon_size, 0.0f,
                        Fade(WHITE, fade));
        x += icon_size + 4.0f;
        shadow_label(stats, (int)x, (int)(at.y - font_size * 0.5f), font_size,
                     fade_c(IMAP_TEXT, fade));
        x += stats_width;
        has_item = true;
    }
    if (show_status) {
        if (has_item) x += 6.0f;
        ui_icon_draw_ex(status_icon, x + icon_size * 0.5f, at.y, (float)icon_size, 0.0f,
                        Fade(WHITE, fade));
        x += icon_size;
        has_item = true;
    }
    for (int i = 0; i < capability_count; ++i) {
        if (has_item) x += 4.0f;
        const char* icon = capability_icons[i];
        if (icon && '\0' != icon[0]) {
            ui_icon_draw_ex(icon, x + icon_size * 0.5f, at.y, (float)icon_size, 0.0f,
                            Fade(WHITE, fade));
        }
        DrawRectangle((int)(x + icon_size * 0.5f - 2.0f), (int)(at.y + icon_size * 0.5f - 3.0f),
                      4, 3, fade_c(capability_colors[i], fade));
        x += icon_size;
        has_item = true;
    }
}

/* Card pass runs before edges so links land on the map surface. */
static void draw_node_card(int idx, float fade) {
    const ImapNode*  n  = &instance_map_data_graph()->nodes[idx];
    Rectangle card = node_rect(idx);

    bool selected = idx == s_m.selected_node;
    bool hovered = CheckCollisionPointRec(GetMousePosition(), card);
    Color accent = selected ? IMAP_SELECTED : hovered ? (Color){ 120, 220, 255, 255 } : IMAP_NODE_LINE;
    draw_picture(card, n->preview_url, n->name, FETCH_P1, fade);
    /* A joined edge keeps no line: the mark stays on the open sides and over the map. */
    if (selected || hovered) {
        draw_open_outline(card, instance_map_joined_edges(instance_map_data_graph(), idx), 2, fade_c(accent, fade));
        DrawRectangleRec(card, fade_c(accent, fade * (selected ? 0.16f : 0.08f)));
    }
}

/* Copies text into out, cut with an ellipsis to max_width pixels. */
static void fit_label(char* out, size_t size, const char* text, int font_size, int max_width) {
    copy_str(out, size, text);
    int length = (int)strlen(out);
    while (length > 3 && MeasureText(out, font_size) > max_width) {
        length = text_utf8_floor(out, length - 1);
        out[length] = '\0';
    }
    if (length < (int)strlen(text) && length >= 3) {
        int dot = text_utf8_floor(out, length - 3);
        memcpy(out + dot, "...", 4);
    }
}

/* Labels stay within their map surface. A joinable map takes a tag inside it,
 * which never lines the edge it may share. */
static void draw_node_name(const char* name, Rectangle card, bool selected, bool tag, float fade) {
    if (32.0f > card.width || 20.0f > card.height) return;
    int font_size = 150.0f <= card.width ? 15 : 11;
    char label[IMAP_NAME_MAX];
    fit_label(label, sizeof(label), name, font_size, (int)card.width - (tag ? 20 : 8));
    int width = MeasureText(label, font_size);
    float label_height = (float)text_line_height(font_size) + 4.0f;
    if (tag) {
        Rectangle pill = { floorf(card.x + (card.width - (float)width) * 0.5f - 6.0f),
                           floorf(card.y + card.height - label_height - 6.0f), (float)width + 12.0f, label_height };
        DrawRectangleRec(pill, fade_c((Color){ 4, 8, 16, 220 }, fade));
        DrawRectangleLinesEx(pill, 1.0f, fade_c(selected ? IMAP_SELECTED : IMAP_NODE_LINE, fade));
        shadow_label(label, (int)pill.x + 6, (int)(pill.y + 2.0f), font_size,
                     fade_c(selected ? IMAP_SELECTED : IMAP_TEXT, fade));
        return;
    }
    Rectangle strip = { card.x, card.y + card.height - label_height, card.width, label_height };
    DrawRectangleRec(strip, fade_c((Color){ 4, 8, 16, 220 }, fade));
    DrawRectangle((int)strip.x, (int)strip.y, (int)strip.width, 1,
                  fade_c(selected ? IMAP_SELECTED : IMAP_NODE_LINE, fade));
    shadow_label(label, (int)(strip.x + (strip.width - (float)width) * 0.5f),
                 (int)(strip.y + 2.0f), font_size,
                 fade_c(selected ? IMAP_SELECTED : IMAP_TEXT, fade));
}

/* Overlay pass: static presence is always shown; capability tabs appear only
 * once a map cell is large enough on screen. Overview markers scale with the
 * card, so a dense map keeps its surface visible. */
static void draw_node_overlay(int idx, float fade, double t) {
    const ImapGraph* gr = instance_map_data_graph();
    const ImapNode*  n  = &gr->nodes[idx];
    Rectangle card = node_rect(idx);

    bool selected  = idx == s_m.selected_node;
    bool is_player = 0 == strcmp(n->map_code, g_local_player.map_code);

    bool zoomed = (0 < n->grid_x ? card.width / (float)n->grid_x : card.width) >= IMAP_CAPABILITY_REVEAL_CELL;
    float icon_size = zoomed ? poi_radius(node_screen_radius()) * 2.2f
                             : Clamp(fminf(card.width, card.height) * 0.05f, 12.0f, 24.0f);
    for (int i = 0; i < gr->presence_poi_count; ++i) {
        const ImapPresencePoi* poi = &gr->presence_pois[i];
        if (poi->node != idx) continue;
        Vector2 at = instance_map_cell_to_card(card, n, poi->cell_x + 0.5f, poi->cell_y + 0.5f);
        uint8_t status = presence_status_icon(poi->presence_status);
        const char* icon = presentation_runtime_status_icon(status);
        Color tint = instance_map_presence_color(poi->presence_status);
        if (zoomed) {
            draw_zoomed_capability_tab(at, icon, poi->capabilities, poi->action_active,
                                       poi->quest_active, false, 0,
                                       tint, (Color){ 0 }, fade, t);
        } else {
            draw_presence_marker(at, icon, tint, icon_size, fade);
        }
    }

    /* Local player: presence outline on the card + the same POI label as the
     * rest at the live map cell — presence status, capabilities, and sum
     * stats fed from the player's real state — wrapped in a green pulse. */
    if (is_player) {
        float grow = 9.0f + 3.0f * sinf((float)t * 3.2f);
        Rectangle ring = { card.x - grow, card.y - grow, card.width + grow * 2.0f, card.height + grow * 2.0f };
        draw_open_outline(ring, instance_map_joined_edges(gr, idx), 3, fade_c(IMAP_PLAYER, fade * 0.85f));
        Vector2 mp = instance_map_cell_to_card(card, n,
                                  game_state_own_render()->interp_pos.x,
                                  game_state_own_render()->interp_pos.y);
        uint8_t self_status = 0 != local_player_status_icon()
            ? local_player_status_icon() : STATUS_ICON_PLAYER;
        const char* self_ic = presentation_runtime_status_icon(self_status);
        if (zoomed) {
            draw_zoomed_capability_tab(mp, self_ic, 0, false, false, true,
                                       game_state_progress(g_local_player.id)->stats_sum,
                                       IMAP_PLAYER, IMAP_PLAYER, fade, t);
        } else {
            float shell = floorf(icon_size) + 6.0f;
            Rectangle marker = { floorf(mp.x - shell * 0.5f), floorf(mp.y - shell * 0.5f),
                                 shell, shell };
            draw_pixel_active_pulse(marker, IMAP_PLAYER, fade, t);
            draw_presence_marker(mp, self_ic, IMAP_PLAYER, icon_size, fade);
        }
    }
    char label[IMAP_NAME_MAX];
    instance_map_node_label(gr, idx, label, sizeof label);
    draw_node_name(label, card, selected, n->placed, fade);
}

/* A new selection leaves its detail panel once: the camera moves the node the
 * shorter way out, right of the panel or above it. */
static void keep_clear_of(Rectangle panel) {
    if (!s_m.clear_pending || s_m.pressed || s_m.pinching || 0.01f < fabsf(s_m.zoom - s_m.zoom_target)) return;
    Rectangle card = node_rect(s_m.selected_node);
    card.x += s_m.pan_target.x - s_m.pan.x;
    card.y += s_m.pan_target.y - s_m.pan.y;
    s_m.clear_pending = false;
    if (!CheckCollisionRecs(card, panel)) return;
    float right = panel.x + panel.width + 16.0f - card.x;
    float up = card.y + card.height + 16.0f - panel.y;
    if (right < up) s_m.pan_target.x += right;
    else s_m.pan_target.y -= up;
}

/* The detail panel of a map: the name, the sector and one row per kind of
 * provider and for the portals. */
static Rectangle info_panel_rect(const ImapNode* n) {
    int lines = 2 + (0 < n->quest_provider_count) + (0 < n->action_provider_count) + (0 < n->portal_count);
    float h = fmaxf(90.0f, 24.0f + 20.0f * (float)lines);
    return (Rectangle){ s_m.panel.x + 14.0f, s_m.panel.y + s_m.panel.height - h - 14.0f, 280.0f, h };
}

static void draw_info_panel(float fade) {
    const ImapGraph* gr = instance_map_data_graph();
    if (s_m.selected_node < 0 || s_m.selected_node >= gr->node_count) return;
    const ImapNode* n = &gr->nodes[s_m.selected_node];

    float panel_w = 280.0f;
    float pad = 12.0f;
    float line_h = 20.0f;
    int name_font = 14;
    int sub_font = 10;
    int stat_font = 12;
    int icon_sz = 14;

    char quest_buf[96];
    snprintf(quest_buf, sizeof(quest_buf), "%d active / %d",
             instance_map_data_node_active_quests(s_m.selected_node), n->quest_provider_count);
    char action_buf[96];
    snprintf(action_buf, sizeof(action_buf), "%d active / %d",
             instance_map_data_node_active_actions(s_m.selected_node), n->action_provider_count);
    Rectangle r = info_panel_rect(n);
    keep_clear_of(r);

    /* ── Panel background with pixel border ── */
    Color fill = (Color){ 12, 20, 38, 240 };
    draw_pixel_panel(r, fill, IMAP_SELECTED, true, fade);

    /* Left accent strip (3px) like the quest grid buttons. */
    DrawRectangle((int)r.x, (int)(r.y + 4.0f), 3, (int)(r.height - 8.0f),
                  fade_c(IMAP_SELECTED, fade));

    int x = (int)(r.x + pad + 6.0f); /* offset for accent strip */
    int y = (int)(r.y + pad);

    /* ── Node name ── */
    {
        char name_buf[IMAP_NAME_MAX];
        fit_label(name_buf, sizeof(name_buf), n->name, name_font, (int)(panel_w - pad * 2.0f - 14.0f));
        shadow_label(name_buf, x, y, name_font, fade_c(IMAP_SELECTED, fade));
    }
    y += line_h;

    /* ── Sector code + grid dimensions ── */
    {
        char line[96];
        snprintf(line, sizeof(line), "SECTOR %s - %dx%d", n->map_code, n->grid_x, n->grid_y);
        shadow_label(line, x, y, sub_font, fade_c(IMAP_TEXT_DIM, fade));
    }
    y += line_h;

    /* ── Quest providers ── */
    if (n->quest_provider_count > 0) {
        ui_icon_draw_ex("quest", (float)x + (float)icon_sz * 0.5f,
                        (float)y + (float)line_h * 0.5f, (float)icon_sz, 0.0f,
                        fade_c(MODAL_QUEST, fade));
        shadow_label(quest_buf, x + icon_sz + 6, y, stat_font, fade_c(MODAL_QUEST, fade));
        y += line_h;
    }

    /* ── Action providers ── */
    if (n->action_provider_count > 0) {
        const char* action_icon = presentation_runtime_status_icon(STATUS_ICON_ACTION_PROVIDER);
        if (action_icon && '\0' != action_icon[0]) {
            ui_icon_draw_ex(action_icon, (float)x + (float)icon_sz * 0.5f,
                            (float)y + (float)line_h * 0.5f, (float)icon_sz, 0.0f,
                            fade_c(MODAL_ACTION, fade));
        }
        shadow_label(action_buf, x + icon_sz + 6, y, stat_font, fade_c(MODAL_ACTION, fade));
        y += line_h;
    }

    /* ── Portal links ── */
    if (n->portal_count > 0) {
        const char* portal_icon = presentation_runtime_status_icon(STATUS_ICON_PORTAL);
        if (portal_icon && '\0' != portal_icon[0]) {
            ui_icon_draw_ex(portal_icon, (float)x + (float)icon_sz * 0.5f,
                            (float)y + (float)line_h * 0.5f, (float)icon_sz, 0.0f,
                            fade_c(IMAP_TEXT, fade));
        }
        char line[48];
        snprintf(line, sizeof(line), "%d portal%s", n->portal_count,
                 n->portal_count == 1 ? "" : "s");
        shadow_label(line, x + icon_sz + 6, y, stat_font, fade_c(IMAP_TEXT, fade));
    }
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
        case UMAP_TRAVEL_NO_ROUTE:   return "NO ROUTE FROM HERE";
        case UMAP_TRAVEL_NOT_SERVED: return "NOT IN THIS RELEASE";
        case UMAP_TRAVEL_OFFLINE:    return "WORLD OFFLINE";
        case UMAP_TRAVEL_DRAINING:   return "WORLD CLOSING";
        case UMAP_TRAVEL_STALE:      return "CHECKING AVAILABILITY";
    }
    return "";
}

/* An arrowhead at tip, pointing along from → tip. */
static void draw_route_arrow(Vector2 from, Vector2 tip, Color color, float fade) {
    Vector2 dir = Vector2Normalize(Vector2Subtract(tip, from));
    Vector2 side = { -dir.y, dir.x };
    for (int pass = 0; pass < 2; ++pass) {
        float size = 0 == pass ? 12.0f : 8.0f;
        Vector2 back = Vector2Subtract(tip, Vector2Scale(dir, size * 1.5f));
        Vector2 a = Vector2Add(back, Vector2Scale(side, size));
        Vector2 b = Vector2Subtract(back, Vector2Scale(side, size));
        Color c = 0 == pass ? fade_c(BLACK, fade) : fade_c(color, fade);
        /* raylib fills one winding; both orders cover either direction. */
        DrawTriangle(tip, a, b, c);
        DrawTriangle(tip, b, a, c);
    }
}

/* How far a ray from the centre of a card runs inside it, plus `margin`. */
static float card_reach(Rectangle card, Vector2 dir, float margin) {
    float x = 0.0f != dir.x ? (card.width * 0.5f + margin) / fabsf(dir.x) : FLT_MAX;
    float y = 0.0f != dir.y ? (card.height * 0.5f + margin) / fabsf(dir.y) : FLT_MAX;
    return fminf(x, y);
}

/* A route between two worlds: pulses travel its directions, and a one-way route ends in an arrow. */
static void draw_route(const UmapLink* link, int idx, float fade, double t) {
    const UmapGraph* u = universe_map_data_graph();
    bool local = link->source == u->current || link->target == u->current;
    Color base = local ? UMAP_ROUTE : fade_c(UMAP_ROUTE, 0.55f);
    Vector2 a = node_center(link->source);
    Vector2 b = node_center(link->target);
    float length = Vector2Distance(a, b);
    if (1.0f > length) return;
    Vector2 dir = Vector2Scale(Vector2Subtract(b, a), 1.0f / length);
    float start = card_reach(node_rect(link->source), dir, 6.0f);
    float end = card_reach(node_rect(link->target), dir, 6.0f);
    if (length <= start + end) return;
    Vector2 from = Vector2Add(a, Vector2Scale(dir, start));
    Vector2 to = Vector2Subtract(b, Vector2Scale(dir, end));
    draw_pixel_diagonal(from, to, base, fade);

    float phase = (float)fmod(t * 0.45 + idx * 0.23, 1.0);
    Vector2 pulses[2] = { pixel_path_point(from, to, phase), pixel_path_point(to, from, phase) };
    for (int i = 0; i < (link->bidirectional ? 2 : 1); ++i) {
        DrawRectangle((int)pulses[i].x - 6, (int)pulses[i].y - 6, 13, 13, fade_c(BLACK, fade));
        DrawRectangle((int)pulses[i].x - 4, (int)pulses[i].y - 4, 9, 9, fade_c(base, fade));
        DrawRectangle((int)pulses[i].x - 1, (int)pulses[i].y - 1, 3, 3, Fade(WHITE, fade));
    }
    if (!link->bidirectional) draw_route_arrow(from, to, base, fade);
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

static void draw_world_card(int idx, float fade) {
    const UmapGraph* u = universe_map_data_graph();
    const UmapNode*  n = &u->nodes[idx];
    Rectangle card = node_rect(idx);
    bool selected = idx == s_m.selected_node;
    bool here = idx == u->current;
    bool hovered = CheckCollisionPointRec(GetMousePosition(), card);
    float shade = n->served ? fade : fade * 0.5f;

    Color accent = selected ? IMAP_SELECTED : here ? IMAP_PLAYER : IMAP_NODE_LINE;
    draw_picture(card, n->preview_url, n->name, FETCH_P2, shade);
    if (selected || hovered || here) DrawRectangleLinesEx(card, 2.0f, fade_c(accent, shade));
    draw_status_pill(card.x + 5.0f, card.y + 5.0f, n->status, fade);
    draw_node_name(n->name, card, selected, false, fade);
}

#define UMAP_PANEL_PAD    12.0f
#define UMAP_PANEL_LINE_H 20.0f
#define UMAP_PANEL_BTN_H  36.0f

/* The thumbnail its author set for a world; the card already shows its preview. */
static float universe_picture_h(const UmapNode* n, float panel_w) {
    return '\0' != n->thumbnail_url[0] ? fminf((panel_w - UMAP_PANEL_PAD * 2.0f) * 0.45f, s_m.panel.height * 0.22f)
                                       : 0.0f;
}

static Rectangle universe_panel_rect(const UmapNode* n) {
    float w = fminf(300.0f, s_m.panel.width - 28.0f);
    float h = UMAP_PANEL_PAD * 4.0f + universe_picture_h(n, w) + UMAP_PANEL_LINE_H * 5.0f + UMAP_PANEL_BTN_H;
    return (Rectangle){ s_m.panel.x + 14.0f, s_m.panel.y + s_m.panel.height - h - 34.0f, w, h };
}

static void draw_universe_panel(float fade) {
    const UmapGraph* u = universe_map_data_graph();
    s_travel_btn = (Rectangle){ 0 };
    if (s_m.selected_node < 0 || s_m.selected_node >= u->node_count) return;
    const UmapNode* n = &u->nodes[s_m.selected_node];
    UmapTravel travel = universe_map_data_travel(s_m.selected_node);

    float pad = UMAP_PANEL_PAD, line_h = UMAP_PANEL_LINE_H, button_h = UMAP_PANEL_BTN_H;
    Rectangle r = universe_panel_rect(n);
    keep_clear_of(r);
    float panel_w = r.width;
    float picture_h = universe_picture_h(n, panel_w);
    draw_pixel_panel(r, (Color){ 12, 20, 38, 240 }, IMAP_SELECTED, true, fade);
    DrawRectangle((int)r.x, (int)(r.y + 4.0f), 3, (int)(r.height - 8.0f), fade_c(IMAP_SELECTED, fade));

    Rectangle picture = { r.x + pad, r.y + pad, panel_w - pad * 2.0f, picture_h };
    if (0.0f < picture_h) draw_picture(picture, n->thumbnail_url, n->name, FETCH_P1, fade);

    int x = (int)(r.x + pad + 6.0f);
    int y = (int)(picture.y + picture_h + pad);
    int text_w = (int)(panel_w - pad * 2.0f - 14.0f);
    char line[IMAP_NAME_MAX];

    fit_label(line, sizeof(line), n->name, 14, text_w);
    shadow_label(line, x, y, 14, fade_c(IMAP_SELECTED, fade));
    y += (int)line_h;

    DrawRectangle(x, y + 3, 7, 7, fade_c(umap_status_color(n->status), fade));
    shadow_label(universe_map_data_stale() ? "AVAILABILITY UNKNOWN" : umap_status_label(n->status),
                  x + 12, y, 12, fade_c(IMAP_TEXT, fade));
    y += (int)line_h;

    const char* route = UMAP_TRAVEL_HERE == travel ? "YOU ARE HERE"
                      : universe_map_data_linked(s_m.selected_node) ? "A ROUTE LEADS HERE"
                                                                    : "NO ROUTE FROM YOUR WORLD";
    shadow_label(route, x, y, 11, fade_c(IMAP_TEXT_DIM, fade));
    y += (int)line_h;
    char summary[IMAP_NAME_MAX];
    snprintf(summary, sizeof summary, "%d MAPS - %s", n->map_count, n->instance_code);
    fit_label(line, sizeof line, summary, 11, text_w);
    shadow_label(line, x, y, 11, fade_c(IMAP_TEXT_DIM, fade));
    y += (int)line_h;
    fit_label(line, sizeof line, '\0' != n->description[0] ? n->description : "No description available.", 11, text_w);
    shadow_label(line, x, y, 11, fade_c(IMAP_TEXT_DIM, fade));

    s_travel_btn = (Rectangle){ r.x + pad, r.y + r.height - pad - button_h, panel_w - pad * 2.0f, button_h };
    bool open = UMAP_TRAVEL_OPEN == travel;
    UIButtonPixelRetroStyle style = {
        .bg = open ? MODAL_ACCEPT : (Color){ 34, 38, 48, 255 }, .icon_id = open ? "forward" : NULL,
        .label = umap_travel_label(travel), .font_size = 14, .text_color = open ? WHITE : IMAP_TEXT_DIM,
        .enabled = open,
    };
    ui_button_pixel_retro_draw(s_travel_btn, &style, open && ui_button_hit(s_travel_btn, GetMouseX(), GetMouseY()));
}

static void draw_universe(float fade, double t) {
    const UmapGraph* u = universe_map_data_graph();
    if (IMAP_DATA_READY != universe_map_data_state()) {
        const char* msg = IMAP_DATA_ERROR == universe_map_data_state() ? "UNIVERSE UNAVAILABLE" : "LOADING UNIVERSE...";
        float blink = IMAP_DATA_ERROR == universe_map_data_state() ? 1.0f : 0.6f + 0.4f * sinf((float)t * 3.0f);
        Vector2 c = ui_rect_center(s_m.panel);
        shadow_label(msg, (int)(c.x - MeasureText(msg, 14) / 2.0f), (int)c.y, 14, fade_c(IMAP_TEXT, fade * blink));
        return;
    }
    if (0 == u->node_count) {
        Vector2 center = ui_rect_center(s_m.panel);
        shadow_label("NO WORLDS IN THIS UNIVERSE", (int)center.x - 110, (int)center.y, 14, IMAP_TEXT_DIM);
        return;
    }
    /* Routes → cards: a route runs under the cards it joins. */
    for (int i = 0; i < u->link_count; ++i) draw_route(&u->links[i], i, fade, t);
    for (int i = 0; i < u->node_count; ++i) draw_world_card(i, fade);
    draw_universe_panel(fade);
}

/* A view tab of the header. */
static void draw_view_tab(Rectangle r, const char* label, bool active, int mx, int my, float fade) {
    bool hovered = !active && CheckCollisionPointRec((Vector2){ (float)mx, (float)my }, r);
    Color fill = active ? (Color){ 60, 88, 124, 255 } : hovered ? (Color){ 40, 58, 86, 255 } : (Color){ 22, 32, 52, 255 };
    draw_pixel_panel(r, fill, active ? IMAP_SELECTED : IMAP_NODE_LINE, active || hovered, fade);
    int width = MeasureText(label, 12);
    shadow_label(label, (int)(r.x + (r.width - width) * 0.5f), (int)(r.y + (r.height - 12.0f) * 0.5f), 12,
                 fade_c(active ? IMAP_SELECTED : IMAP_TEXT, fade));
}

static void draw_header(float fade) {
    const ImapGraph* gr = instance_map_data_graph();
    const UmapGraph* u = universe_map_data_graph();

    /* Title sits below the toolbar strip (which keeps the map readout) —
     * reclaims the space when the strip is hidden. */
    float title_y = s_m.panel.y + toolbar_height() + 12.0f;
    /* Left inset clears the toolbar's pinned top-left toggle. */
    float title_x = s_m.panel.x + TOOLBAR_BTN_SIZE + 12.0f;

    int mx = GetMouseX();
    int my = GetMouseY();
    float tab_h = 24.0f;
    float tab_y = title_y - (tab_h - 16.0f) * 0.5f;
    s_world_tab = (Rectangle){ title_x, tab_y, 76.0f, tab_h };
    s_universe_tab = (Rectangle){ title_x + 82.0f, tab_y, 96.0f, tab_h };
    draw_view_tab(s_world_tab, "WORLD", VIEW_WORLD == s_m.view, mx, my, fade);
    draw_view_tab(s_universe_tab, "UNIVERSE", VIEW_UNIVERSE == s_m.view, mx, my, fade);
    float button_side = 28.0f;
    float button_y = title_y - (button_side - 16.0f) * 0.5f;
    s_close_btn = (Rectangle){ s_m.panel.x + s_m.panel.width - button_side - 14.0f, button_y, button_side, button_side };
    bool close_hovered = CheckCollisionPointRec((Vector2){ (float)mx, (float)my }, s_close_btn);
    draw_pixel_panel(s_close_btn, close_hovered ? (Color){ 60, 88, 124, 255 } : (Color){ 30, 44, 68, 255 },
                     close_hovered ? IMAP_SELECTED : IMAP_NODE_LINE, close_hovered, fade);
    ui_icon_draw_ex("close-yellow", s_close_btn.x + button_side * 0.5f, s_close_btn.y + button_side * 0.5f,
                    button_side * 0.56f, 0.0f, Fade(WHITE, fade));

    bool universe = VIEW_UNIVERSE == s_m.view;
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
    shadow_label(sub, (int)title_x,
                 (int)(tab_y + tab_h) + 5, 11,
                 fade_c(IMAP_TEXT_DIM, fade));

    const char* hint = universe ? "DRAG PAN - WHEEL / PINCH ZOOM - TAP A WORLD - ARROW: ONE-WAY ROUTE"
                                : "DRAG PAN - WHEEL / PINCH ZOOM - TAP A MAP";
    int hw = MeasureText(hint, 10);
    shadow_label(hint, (int)(s_m.panel.x + s_m.panel.width - hw - 14.0f),
                 (int)(s_m.panel.y + s_m.panel.height - 20.0f), 10,
                 fade_c(IMAP_TEXT_DIM, fade * 0.9f));
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
    draw_pixel_panel(s_m.panel, IMAP_BG, (Color){ 70, 190, 240, 180 }, false, chrome);
    BeginScissorMode((int)s_m.panel.x, (int)s_m.panel.y,
                     (int)s_m.panel.width, (int)s_m.panel.height);

    draw_backdrop(chrome);

    const ImapGraph* gr = instance_map_data_graph();
    if (content > 0.0f && VIEW_UNIVERSE == s_m.view) {
        draw_universe(content, t);
    } else if (content > 0.0f) {
        if (IMAP_DATA_READY == instance_map_data_state()) {
            /* Cards → edges → overlays: link lines land on the map surfaces
             * and the POI icons plug into them from above. Packed cards share
             * grid edges, so declaration order is the stable painter order. */
            for (int i = 0; i < gr->node_count; ++i) draw_node_card(i, content);
            for (int e = 0; e < gr->edge_count; ++e) draw_edge(&gr->edges[e], e, content, t);
            for (int i = 0; i < gr->node_count; ++i) draw_node_overlay(i, content, t);

            draw_info_panel(content);
        } else {
            const char* msg = IMAP_DATA_ERROR == instance_map_data_state()
                                  ? "INSTANCE INTELLIGENCE UNAVAILABLE"
                                  : "LOADING INSTANCE INTELLIGENCE...";
            int fs = 14;
            int w  = MeasureText(msg, fs);
            float blink = IMAP_DATA_ERROR == instance_map_data_state()
                              ? 1.0f : 0.6f + 0.4f * sinf((float)t * 3.0f);
            Vector2 c = ui_rect_center(s_m.panel);
            shadow_label(msg, (int)(c.x - w / 2.0f), (int)c.y, fs,
                         fade_c(IMAP_TEXT, content * blink));
        }
    }

    EndScissorMode();

    if (content > 0.05f) draw_header(content);
}
