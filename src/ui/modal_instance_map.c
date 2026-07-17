#include "modal_instance_map.h"

#include "instance_map_data.h"
#include "modal_map.h"
#include "text.h"
#include "toolbar.h"
#include "ui_button.h"

#include "game_state.h"
#include "input/input.h"
#include "texture_cache.h"

#include <assert.h>
#include <math.h>
#include <raylib.h>
#include <stdio.h>
#include <string.h>

/* ── Presentation constants ──────────────────────────────────────────────
 * Stylised strategic view: tilted (isometric-flavoured) graph plane with
 * glowing connections. Clarity over realism — no 3D rendering. Nodes are
 * square map cards: the preview capture fills the card and strategic POIs
 * (quest/action providers, portals, player) sit at their real map cells.  */
#define IMAP_ISO_Y          0.56f   /* vertical squash of the graph plane   */
#define IMAP_WORLD_SCALE    250.0f  /* graph units → px at zoom 1           */
#define IMAP_NODE_RADIUS    40.0f   /* half of the square card side at zoom 1 */
#define IMAP_ZOOM_MIN       0.45f
#define IMAP_ZOOM_INIT      1.5f
#define IMAP_ZOOM_MAX_FLOOR 3.2f
#define IMAP_ZOOM_STEP      1.15f
#define IMAP_DRAG_SLOP_PX   9.0f
#define IMAP_CAM_LAMBDA     11.0f   /* exp smoothing rate for pan/zoom      */

static const Color IMAP_BG        = { 6, 10, 22, 205 };
static const Color IMAP_GRID_LINE = { 60, 140, 190, 26 };
static const Color IMAP_EDGE      = { 60, 200, 255, 255 };
static const Color IMAP_EDGE_INTRA= { 170, 110, 255, 255 };
static const Color IMAP_NODE_FILL = { 14, 26, 44, 235 };
static const Color IMAP_NODE_LINE = { 90, 210, 250, 220 };
static const Color IMAP_SELECTED  = { 255, 210, 90, 255 };
static const Color IMAP_PLAYER    = { 110, 255, 150, 255 };
static const Color IMAP_QUEST     = { 250, 205, 70, 255 };
static const Color IMAP_ACTION    = { 90, 230, 235, 255 };
static const Color IMAP_TEXT      = { 205, 225, 245, 235 };
static const Color IMAP_TEXT_DIM  = { 130, 150, 180, 190 };

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

    int selected_node;    /* -1 = none                      */
    int layout_generation;
} ModalInstanceMap;

static ModalInstanceMap s_m = {0};

/* Node preview backgrounds: each map's auto-captured Object Layer render,
 * fetched lazily from /api/file/blob/:id through the engine fetch pipeline. */
static TextureCache* s_preview_cache = NULL;

static void on_preview_blob(const FetchResponse* r) {
    texture_cache_on_blob_fetched(s_preview_cache, r);
}

/* ── Lifecycle ──────────────────────────────────────────────────────────── */

void modal_instance_map_init(void) {
    memset(&s_m, 0, sizeof(s_m));
    s_m.zoom = s_m.zoom_target = IMAP_ZOOM_INIT;
    s_m.selected_node = -1;
    s_preview_cache = texture_cache_create(IMAP_MAX_NODES, "imap-preview", on_preview_blob);
}

void modal_instance_map_cleanup(void) {
    if (s_m.open) {
        instance_map_data_close();
        input_gestures_set_blocked(false);
    }
    if (s_preview_cache) {
        texture_cache_destroy(s_preview_cache);
        s_preview_cache = NULL;
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
    s_m.selected_node = -1;
    s_m.pressed = s_m.dragging = s_m.pinching = false;
    reset_camera();
    instance_map_data_open();
    modal_map_set_expanded(true);      /* container morphs to full screen */
    input_gestures_set_blocked(true);  /* map owns pinch while expanded    */
}

void modal_instance_map_close(void) {
    if (!s_m.open) return;
    s_m.open = false;
    instance_map_data_close();      /* stops dynamic polling immediately */
    modal_map_set_expanded(false);  /* container retracts to the readout */
    input_gestures_set_blocked(false);
}

/* ── Projection ─────────────────────────────────────────────────────────── */

static Vector2 panel_center(void) {
    return (Vector2){ s_m.panel.x + s_m.panel.width * 0.5f,
                      s_m.panel.y + s_m.panel.height * 0.5f };
}

/* Graph space ([-1,1]²) → screen px, tilted plane + camera. */
static Vector2 graph_to_screen(Vector2 g) {
    Vector2 c = panel_center();
    return (Vector2){
        c.x + s_m.pan.x + g.x * IMAP_WORLD_SCALE * s_m.zoom,
        c.y + s_m.pan.y + g.y * IMAP_WORLD_SCALE * IMAP_ISO_Y * s_m.zoom,
    };
}

/* Depth cue: nodes lower on the tilted plane render slightly larger. */
static float node_depth_scale(Vector2 g) {
    return 0.88f + (g.y + 1.0f) * 0.5f * 0.28f;
}

/* Half of the node card side, screen px. */
static float node_screen_radius(const ImapNode* n) {
    return IMAP_NODE_RADIUS * s_m.zoom * node_depth_scale(n->pos);
}

/* Square map card, centered on the node's layout position. */
static Rectangle node_rect(const ImapNode* n) {
    Vector2 p = graph_to_screen(n->pos);
    float   h = node_screen_radius(n);
    return (Rectangle){ p.x - h, p.y - h, h * 2.0f, h * 2.0f };
}

static int node_at_point(float x, float y) {
    const ImapGraph* gr = instance_map_data_graph();
    int hit = -1;
    float best = 1e9f;
    for (int i = 0; i < gr->node_count; ++i) {
        Vector2 p = graph_to_screen(gr->nodes[i].pos);
        Rectangle r = node_rect(&gr->nodes[i]);
        r.x -= 6.0f; r.y -= 6.0f; r.width += 12.0f; r.height += 12.0f;
        if (!CheckCollisionPointRec((Vector2){ x, y }, r)) continue;
        float dx = x - p.x, dy = y - p.y;
        float d2 = dx * dx + dy * dy;
        if (d2 < best) { best = d2; hit = i; }
    }
    return hit;
}

/* ── Camera controls ────────────────────────────────────────────────────── */

static float clampf(float v, float lo, float hi) {
    return v < lo ? lo : (v > hi ? hi : v);
}

/* Zooming in may continue until one node card spans the full panel width. */
static float imap_zoom_max(void) {
    float w = s_m.panel.width > 4.0f ? s_m.panel.width : (float)GetScreenWidth();
    float max = w / (2.0f * IMAP_NODE_RADIUS);
    return max > IMAP_ZOOM_MAX_FLOOR ? max : IMAP_ZOOM_MAX_FLOOR;
}

/* Zoom about an anchor point: the graph point under the anchor stays put. */
static void zoom_about(float factor, Vector2 anchor) {
    float nz = clampf(s_m.zoom_target * factor, IMAP_ZOOM_MIN, imap_zoom_max());
    factor = nz / s_m.zoom_target;
    Vector2 c = panel_center();
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

/* Release resolution: a clean (non-drag) press acts on what it landed on. */
static void resolve_click(Vector2 at) {
    int hit = node_at_point(at.x, at.y);
    s_m.selected_node = hit; /* tapping empty space deselects */
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

    Vector2 p    = GetTouchPointCount() > 0 ? GetTouchPosition(0) : GetMousePosition();
    bool    down = GetTouchPointCount() > 0 || IsMouseButtonDown(MOUSE_BUTTON_LEFT);

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

    /* Refetched graph → stale selection. */
    if (instance_map_data_generation() != s_m.layout_generation) {
        s_m.layout_generation = instance_map_data_generation();
        s_m.selected_node = -1;
    }

    track_pointer();

    /* Smooth camera interpolation (frame-rate independent). */
    float a = 1.0f - expf(-IMAP_CAM_LAMBDA * dt);
    s_m.zoom  += (s_m.zoom_target - s_m.zoom) * a;
    s_m.pan.x += (s_m.pan_target.x - s_m.pan.x) * a;
    s_m.pan.y += (s_m.pan_target.y - s_m.pan.y) * a;
}

/* ── Input dispatch glue ────────────────────────────────────────────────── */

bool modal_instance_map_handle_click(int mx, int my) {
    if (!s_m.open) return false;
    /* Consume presses inside the panel; the gesture tracker resolves them.
     * Presses outside fall through — the overlay is non-blocking. */
    return CheckCollisionPointRec((Vector2){ (float)mx, (float)my }, s_m.panel);
}

bool modal_instance_map_covers_point(int mx, int my) {
    if (!s_m.open) return false;
    return CheckCollisionPointRec((Vector2){ (float)mx, (float)my }, s_m.panel);
}

/* ── Drawing ────────────────────────────────────────────────────────────── */

static Color fade_c(Color c, float f) {
    c.a = (unsigned char)((float)c.a * f);
    return c;
}

static void shadow_label(const char* s, int x, int y, int fs, Color c) {
    DrawText(s, x + 1, y + 1, fs, (Color){ 0, 0, 0, (unsigned char)(150.0f * (c.a / 255.0f)) });
    DrawText(s, x, y, fs, c);
}

/* Tilted background lattice — parallax slower than the graph for depth. */
static void draw_backdrop(float fade) {
    Color line = fade_c(IMAP_GRID_LINE, fade);
    float step = 46.0f * s_m.zoom;
    if (step < 18.0f) step = 18.0f;
    Vector2 c = panel_center();
    float par = 0.35f; /* parallax factor */
    float ox = fmodf(s_m.pan.x * par, step);
    float oy = fmodf(s_m.pan.y * par, step * IMAP_ISO_Y);

    for (float x = s_m.panel.x + ox - step; x < s_m.panel.x + s_m.panel.width + step; x += step) {
        DrawLineEx((Vector2){ x, s_m.panel.y }, (Vector2){ x + 26.0f, s_m.panel.y + s_m.panel.height },
                   1.0f, line);
    }
    for (float y = s_m.panel.y + oy - step; y < s_m.panel.y + s_m.panel.height + step; y += step * IMAP_ISO_Y) {
        DrawLineEx((Vector2){ s_m.panel.x, y }, (Vector2){ s_m.panel.x + s_m.panel.width, y },
                   1.0f, line);
    }
    /* Soft radial focus glow at the plane centre. */
    DrawCircleGradient((Vector2){ c.x + s_m.pan.x, c.y + s_m.pan.y },
                       260.0f * s_m.zoom,
                       fade_c((Color){ 40, 90, 160, 26 }, fade), BLANK);
}

/* Forward decls — edge endpoints anchor to positions on the node cards. */
static Rectangle node_rect(const ImapNode* n);
static Vector2   cell_to_card(Rectangle card, const ImapNode* n, float cell_x, float cell_y);

/* Resolve one edge endpoint on its node card: a known cell anchors to that
 * cell (the portal landmark); a random destination (-1) roams across the
 * whole card — the link keeps pointing at ever-changing spots of the map,
 * mirroring the teleport's "anywhere on this map" behaviour. */
static Vector2 edge_endpoint(const ImapNode* n, int cell_x, int cell_y, double t, int salt) {
    Rectangle card = node_rect(n);
    if (cell_x < 0 || cell_y < 0) {
        float sx = (float)salt * 2.1f, sy = (float)salt * 1.3f;
        /* Slow incommensurate-frequency roam covering the card area... */
        float fx = 0.5f + 0.36f * sinf((float)t * 0.83f + sx) * cosf((float)t * 0.31f + sy * 0.7f);
        float fy = 0.5f + 0.36f * sinf((float)t * 0.67f + sy) * cosf((float)t * 0.41f + sx * 1.9f);
        /* ...plus a fast vibration on top. */
        fx += sinf((float)t * 29.0f + sx) * 0.012f;
        fy += cosf((float)t * 23.0f + sy) * 0.012f;
        return (Vector2){ card.x + fx * card.width, card.y + fy * card.height };
    }
    return cell_to_card(card, n, (float)cell_x + 0.5f, (float)cell_y + 0.5f);
}

/* Glowing connection: layered strokes + a travelling pulse dot, anchored at
 * the real portal positions on the map cards. Multiple portals between the
 * same pair of maps produce visibly distinct lines; intra-map links run
 * inside their own card. */
static void draw_edge(const ImapEdge* e, int idx, float fade, double t) {
    const ImapGraph* gr = instance_map_data_graph();
    Color base = e->intra ? IMAP_EDGE_INTRA : IMAP_EDGE;

    Vector2 a = edge_endpoint(&gr->nodes[e->source_node], e->source_cell_x, e->source_cell_y,
                              t, idx * 2);
    Vector2 b = edge_endpoint(&gr->nodes[e->target_node], e->target_cell_x, e->target_cell_y,
                              t, idx * 2 + 1);

    /* Degenerate link (both endpoints random on the same map): a vibrating
     * pulse ring at the card centre instead of a zero-length line. */
    float dx = b.x - a.x, dy = b.y - a.y;
    if (dx * dx + dy * dy < 16.0f) {
        float pr = 6.0f + 2.0f * sinf((float)t * 5.0f + (float)idx);
        DrawRing(a, pr - 1.5f, pr + 1.5f, 0, 360, 24, fade_c(base, fade * 0.8f));
        return;
    }

    float pulse = 0.75f + 0.25f * sinf((float)t * 2.2f + (float)idx * 1.7f);
    DrawLineEx(a, b, 7.0f, fade_c((Color){ base.r, base.g, base.b, 24 }, fade * pulse));
    DrawLineEx(a, b, 3.6f, fade_c((Color){ base.r, base.g, base.b, 65 }, fade * pulse));
    DrawLineEx(a, b, 1.7f, fade_c((Color){ base.r, base.g, base.b, 175 }, fade));

    /* Travelling energy dot — directional flow along the portal. */
    float ft = (float)fmod(t * 0.45 + idx * 0.19, 1.0);
    Vector2 d = { a.x + (b.x - a.x) * ft, a.y + (b.y - a.y) * ft };
    DrawCircleV(d, 7.0f, fade_c((Color){ base.r, base.g, base.b, 55 }, fade));
    DrawCircleV(d, 3.2f, fade_c((Color){ 255, 255, 255, 210 }, fade));
}

static void draw_provider_badge(Vector2 at, float r, int active, int total,
                                Color color, bool diamond, float fade, double t) {
    if (0 == total) return;
    bool  lit   = active > 0;
    float pulse = lit ? 0.8f + 0.2f * sinf((float)t * 3.0f) : 1.0f;
    Color c     = lit ? color : (Color){ 110, 120, 140, 160 };
    if (lit) DrawCircleV(at, r + 5.0f, fade_c((Color){ c.r, c.g, c.b, 45 }, fade * pulse));
    if (diamond) DrawPoly(at, 4, r + 2.0f, 45.0f, fade_c(c, fade));
    else         DrawCircleV(at, r, fade_c(c, fade));

    char txt[8];
    snprintf(txt, sizeof(txt), "%d", lit ? active : total);
    int fs = (int)(r * 1.15f);
    int w  = MeasureText(txt, fs);
    shadow_label(txt, (int)(at.x - w / 2.0f), (int)(at.y - fs / 2.0f), fs,
                 fade_c((Color){ 10, 14, 24, 255 }, fade));
}

/* Map cell → screen position inside the node card (the preview capture and
 * the gameplay grid share the same cell space, so POIs land where they are
 * on the real map). Cell centers use +0.5. */
static Vector2 cell_to_card(Rectangle card, const ImapNode* n, float cell_x, float cell_y) {
    float fx = n->grid_x > 0 ? cell_x / (float)n->grid_x : 0.5f;
    float fy = n->grid_y > 0 ? cell_y / (float)n->grid_y : 0.5f;
    fx = clampf(fx, 0.0f, 1.0f);
    fy = clampf(fy, 0.0f, 1.0f);
    return (Vector2){ card.x + fx * card.width, card.y + fy * card.height };
}

/* POI marker size: grows with the card so zooming in scales the landmarks. */
static float poi_radius(float half_side) {
    float r = half_side * 0.14f;
    return r < 7.0f ? 7.0f : r;
}

/* Card pass: shadow, halo, fill, preview, borders. Runs before the edges so
 * the link lines land ON the map surface, under the POI overlays. */
static void draw_node_card(int idx, float fade, double t) {
    const ImapGraph* gr = instance_map_data_graph();
    const ImapNode*  n  = &gr->nodes[idx];
    Vector2   p    = graph_to_screen(n->pos);
    float     h    = node_screen_radius(n);
    Rectangle card = node_rect(n);

    bool selected = idx == s_m.selected_node;

    /* Ground shadow sells the tilt: squashed ellipse below the card. */
    DrawEllipse((int)p.x, (int)(card.y + card.height + h * 0.10f), h * 1.05f, h * 0.30f,
                fade_c((Color){ 0, 0, 0, 70 }, fade));

    /* Halo behind the card. */
    Color line = selected ? IMAP_SELECTED : IMAP_NODE_LINE;
    float halo = selected ? 0.9f + 0.1f * sinf((float)t * 4.0f) : 0.55f;
    Rectangle halo_r = { card.x - 7.0f, card.y - 7.0f, card.width + 14.0f, card.height + 14.0f };
    DrawRectangleRounded(halo_r, 0.10f, 6, fade_c((Color){ line.r, line.g, line.b, 26 }, fade * halo));

    /* Square map card: dark fill + the preview capture filling the card. */
    DrawRectangleRounded(card, 0.06f, 6, fade_c(IMAP_NODE_FILL, fade));
    if ('\0' != n->preview_file_id[0]) {
        char url[128];
        snprintf(url, sizeof(url), "/api/file/blob/%s", n->preview_file_id);
        Texture2D tex = texture_cache_get(s_preview_cache, url);
        if (0 != tex.id) {
            Rectangle src  = { 0, 0, (float)tex.width, (float)tex.height };
            Rectangle dest = { card.x + 1.5f, card.y + 1.5f, card.width - 3.0f, card.height - 3.0f };
            DrawTexturePro(tex, src, dest, (Vector2){ 0, 0 }, 0.0f,
                           fade_c((Color){ 215, 225, 240, 245 }, fade));
        }
    }

    DrawRectangleRoundedLines(card, 0.06f, 6, fade_c(line, fade));
    Rectangle inner = { card.x + 4.0f, card.y + 4.0f, card.width - 8.0f, card.height - 8.0f };
    DrawRectangleRoundedLines(inner, 0.06f, 6, fade_c((Color){ line.r, line.g, line.b, 60 }, fade));
}

/* Overlay pass: POIs, player, badges, name — above the edge lines so links
 * appear to plug into the portal icons. */
static void draw_node_overlay(int idx, float fade, double t) {
    const ImapGraph* gr = instance_map_data_graph();
    const ImapNode*  n  = &gr->nodes[idx];
    Vector2   p    = graph_to_screen(n->pos);
    float     h    = node_screen_radius(n);
    Rectangle card = node_rect(n);

    bool selected  = idx == s_m.selected_node;
    bool is_player = 0 == strcmp(n->map_code, g_game_state.player.map_code);

    /* ── POIs at their real map cells (init-spawn positions) ─────────── */
    float pr = poi_radius(h);

    /* Portal landmarks: violet ring + core. */
    for (int i = 0; i < gr->portal_poi_count; ++i) {
        const ImapPortalPoi* poi = &gr->portal_pois[i];
        if (poi->node != idx) continue;
        Vector2 at = cell_to_card(card, n, poi->cell_x + 0.5f, poi->cell_y + 0.5f);
        Color pc   = poi->intra ? IMAP_EDGE_INTRA : IMAP_EDGE;
        float spin = 0.85f + 0.15f * sinf((float)t * 2.6f + (float)i);
        DrawRing(at, pr * 0.55f, pr * 0.85f, 0, 360, 24, fade_c(pc, fade * spin));
        DrawCircleV(at, pr * 0.30f, fade_c((Color){ 255, 255, 255, 200 }, fade));
    }

    /* Quest providers: gold circle + "!". */
    for (int i = 0; i < gr->quest_provider_count; ++i) {
        const ImapProvider* qp = &gr->quest_providers[i];
        if (qp->node != idx) continue;
        Vector2 at  = cell_to_card(card, n, qp->cell_x + 0.5f, qp->cell_y + 0.5f);
        bool    lit = qp->active;
        Color   c   = lit ? IMAP_QUEST : (Color){ 130, 125, 105, 170 };
        if (lit) {
            float pulse = 0.75f + 0.25f * sinf((float)t * 3.4f + (float)i);
            DrawCircleV(at, pr * 1.5f, fade_c((Color){ c.r, c.g, c.b, 50 }, fade * pulse));
        }
        DrawCircleV(at, pr, fade_c(c, fade));
        int fs = (int)(pr * 1.5f);
        int w  = MeasureText("!", fs);
        shadow_label("!", (int)(at.x - w / 2.0f), (int)(at.y - fs / 2.0f), fs,
                     fade_c((Color){ 20, 16, 8, 255 }, fade));
    }

    /* Action providers: cyan diamond. */
    for (int i = 0; i < gr->action_provider_count; ++i) {
        const ImapProvider* ap = &gr->action_providers[i];
        if (ap->node != idx) continue;
        Vector2 at  = cell_to_card(card, n, ap->cell_x + 0.5f, ap->cell_y + 0.5f);
        bool    lit = ap->active;
        Color   c   = lit ? IMAP_ACTION : (Color){ 105, 130, 135, 170 };
        if (lit) {
            float pulse = 0.75f + 0.25f * sinf((float)t * 3.4f + (float)i);
            DrawPoly(at, 4, pr * 1.6f, 45.0f, fade_c((Color){ c.r, c.g, c.b, 50 }, fade * pulse));
        }
        DrawPoly(at, 4, pr * 1.1f, 45.0f, fade_c(c, fade));
    }

    /* Local player: presence outline + marker at the live map cell. */
    if (is_player) {
        float grow = 9.0f + 3.0f * sinf((float)t * 3.2f);
        Rectangle ring = { card.x - grow, card.y - grow, card.width + grow * 2.0f, card.height + grow * 2.0f };
        DrawRectangleRoundedLines(ring, 0.10f, 6, fade_c(IMAP_PLAYER, fade * 0.85f));
        Vector2 mp = cell_to_card(card, n,
                                  g_game_state.player.base.interp_pos.x,
                                  g_game_state.player.base.interp_pos.y);
        DrawCircleV(mp, pr * 1.4f, fade_c((Color){ IMAP_PLAYER.r, IMAP_PLAYER.g, IMAP_PLAYER.b, 60 }, fade));
        DrawCircleV(mp, pr * 0.65f, fade_c(IMAP_PLAYER, fade));
    }

    /* Strategic count badges pinned to the card's top corners. */
    draw_provider_badge((Vector2){ card.x, card.y }, 12.0f,
                        instance_map_data_node_active_quests(idx), n->quest_provider_count,
                        IMAP_QUEST, false, fade, t);
    draw_provider_badge((Vector2){ card.x + card.width, card.y }, 12.0f,
                        instance_map_data_node_active_actions(idx), n->action_provider_count,
                        IMAP_ACTION, true, fade, t);

    /* Name plate. */
    int fs = 16;
    int w  = MeasureText(n->name, fs);
    shadow_label(n->name, (int)(p.x - w / 2.0f), (int)(card.y + card.height + 10.0f), fs,
                 fade_c(selected ? IMAP_SELECTED : IMAP_TEXT, fade));
}

/* Painter order: higher on the tilted plane (smaller y) draws first. */
static void sorted_node_order(int* order, int n) {
    const ImapGraph* gr = instance_map_data_graph();
    for (int i = 0; i < n; ++i) order[i] = i;
    for (int i = 1; i < n; ++i) {
        int v = order[i];
        int j = i - 1;
        while (j >= 0 && gr->nodes[order[j]].pos.y > gr->nodes[v].pos.y) {
            order[j + 1] = order[j];
            j--;
        }
        order[j + 1] = v;
    }
}

static void draw_info_panel(float fade) {
    const ImapGraph* gr = instance_map_data_graph();
    if (s_m.selected_node < 0 || s_m.selected_node >= gr->node_count) return;
    const ImapNode* n = &gr->nodes[s_m.selected_node];

    float w = 240.0f, h = 118.0f, pad = 12.0f;
    Rectangle r = { s_m.panel.x + 14.0f, s_m.panel.y + s_m.panel.height - h - 14.0f, w, h };
    DrawRectangleRounded(r, 0.14f, 8, fade_c((Color){ 10, 18, 32, 235 }, fade));
    DrawRectangleRoundedLines(r, 0.14f, 8, fade_c(IMAP_SELECTED, fade * 0.8f));

    int x = (int)(r.x + pad), y = (int)(r.y + pad);
    shadow_label(n->name, x, y, 15, fade_c(IMAP_SELECTED, fade));
    y += text_line_height(15) + 2;

    char line[96];
    snprintf(line, sizeof(line), "SECTOR %s  ·  %dx%d", n->map_code, n->grid_x, n->grid_y);
    shadow_label(line, x, y, 10, fade_c(IMAP_TEXT_DIM, fade));
    y += text_line_height(10) + 5;

    snprintf(line, sizeof(line), "Quest Providers   %d active / %d",
             instance_map_data_node_active_quests(s_m.selected_node), n->quest_provider_count);
    shadow_label(line, x, y, 12, fade_c(IMAP_QUEST, fade));
    y += text_line_height(12) + 2;

    snprintf(line, sizeof(line), "Action Providers  %d active / %d",
             instance_map_data_node_active_actions(s_m.selected_node), n->action_provider_count);
    shadow_label(line, x, y, 12, fade_c(IMAP_ACTION, fade));
    y += text_line_height(12) + 2;

    snprintf(line, sizeof(line), "Portal Links      %d", n->portal_count);
    shadow_label(line, x, y, 12, fade_c(IMAP_TEXT, fade));
}

static void draw_header(float fade) {
    const ImapGraph* gr = instance_map_data_graph();

    /* Title sits below the toolbar strip (which keeps the map readout) —
     * reclaims the space when the strip is hidden. */
    float title_y = s_m.panel.y + toolbar_height() + 12.0f;
    const char* title = "INSTANCE MAP";
    /* Left inset clears the toolbar's pinned top-left toggle. */
    float title_x = s_m.panel.x + TOOLBAR_BTN_SIZE + 12.0f;
    shadow_label(title, (int)title_x, (int)title_y, 16,
                 fade_c((Color){ 140, 230, 255, 240 }, fade));

    char sub[IMAP_NAME_MAX + 24];
    switch (instance_map_data_state()) {
        case IMAP_DATA_READY:
            snprintf(sub, sizeof(sub), "%s", gr->name[0] ? gr->name : gr->instance_code);
            break;
        case IMAP_DATA_LOADING: snprintf(sub, sizeof(sub), "SCANNING TOPOLOGY..."); break;
        case IMAP_DATA_ERROR:   snprintf(sub, sizeof(sub), "UPLINK UNAVAILABLE");   break;
        default:                sub[0] = '\0';                                       break;
    }
    shadow_label(sub, (int)title_x,
                 (int)title_y + text_line_height(16) + 1, 11,
                 fade_c(IMAP_TEXT_DIM, fade));

    const char* hint = "DRAG PAN · WHEEL / PINCH ZOOM · TAP NODE";
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

    /* Translucent panel — the gameplay grid stays visible behind it. */
    float roundness = 0.30f * (1.0f - mt) + 0.02f * mt;
    DrawRectangleRounded(s_m.panel, roundness, 10, fade_c(IMAP_BG, chrome));
    DrawRectangleRoundedLines(s_m.panel, roundness, 10,
                              fade_c((Color){ 70, 190, 240, 120 }, chrome));

    if (s_m.panel.width < 4.0f || s_m.panel.height < 4.0f) return;
    BeginScissorMode((int)s_m.panel.x, (int)s_m.panel.y,
                     (int)s_m.panel.width, (int)s_m.panel.height);

    draw_backdrop(chrome);

    const ImapGraph* gr = instance_map_data_graph();
    if (content > 0.0f) {
        if (IMAP_DATA_READY == instance_map_data_state()) {
            int order[IMAP_MAX_NODES];
            sorted_node_order(order, gr->node_count);

            /* Cards → edges → overlays: link lines land on the map surfaces
             * and the POI icons plug into them from above. */
            for (int i = 0; i < gr->node_count; ++i) draw_node_card(order[i], content, t);
            for (int e = 0; e < gr->edge_count; ++e) draw_edge(&gr->edges[e], e, content, t);
            for (int i = 0; i < gr->node_count; ++i) draw_node_overlay(order[i], content, t);

            draw_info_panel(content);
        } else {
            const char* msg = IMAP_DATA_ERROR == instance_map_data_state()
                                  ? "INSTANCE INTELLIGENCE UNAVAILABLE"
                                  : "LOADING INSTANCE INTELLIGENCE...";
            int fs = 14;
            int w  = MeasureText(msg, fs);
            float blink = IMAP_DATA_ERROR == instance_map_data_state()
                              ? 1.0f : 0.6f + 0.4f * sinf((float)t * 3.0f);
            Vector2 c = panel_center();
            shadow_label(msg, (int)(c.x - w / 2.0f), (int)c.y, fs,
                         fade_c(IMAP_TEXT, content * blink));
        }
    }

    EndScissorMode();

    if (content > 0.05f) draw_header(content);
}
