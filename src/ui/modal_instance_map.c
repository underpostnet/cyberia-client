#include "modal_instance_map.h"

#include "instance_map_data.h"
#include "modal_map.h"
#include "text.h"
#include "ui_button.h"

#include "game_state.h"

#include <assert.h>
#include <math.h>
#include <raylib.h>
#include <stdio.h>
#include <string.h>

/* ── Presentation constants ──────────────────────────────────────────────
 * Stylised strategic view: tilted (isometric-flavoured) graph plane with
 * glowing connections. Clarity over realism — no 3D rendering.            */
#define IMAP_ISO_Y          0.56f   /* vertical squash of the graph plane   */
#define IMAP_WORLD_SCALE    250.0f  /* graph units → px at zoom 1           */
#define IMAP_NODE_RADIUS    30.0f
#define IMAP_ZOOM_MIN       0.45f
#define IMAP_ZOOM_MAX       3.2f
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

/* ── Lifecycle ──────────────────────────────────────────────────────────── */

void modal_instance_map_init(void) {
    memset(&s_m, 0, sizeof(s_m));
    s_m.zoom = s_m.zoom_target = 1.0f;
    s_m.selected_node = -1;
}

void modal_instance_map_cleanup(void) {
    if (s_m.open) instance_map_data_close();
    memset(&s_m, 0, sizeof(s_m));
}

bool modal_instance_map_is_open(void) { return s_m.open; }

static void reset_camera(void) {
    s_m.pan = s_m.pan_target = (Vector2){ 0, 0 };
    s_m.zoom = s_m.zoom_target = 1.0f;
}

void modal_instance_map_toggle(void) {
    if (s_m.open) { modal_instance_map_close(); return; }
    s_m.open = true;
    s_m.selected_node = -1;
    s_m.pressed = s_m.dragging = s_m.pinching = false;
    reset_camera();
    instance_map_data_open();
    modal_map_set_expanded(true);   /* container morphs to full screen */
}

void modal_instance_map_close(void) {
    if (!s_m.open) return;
    s_m.open = false;
    instance_map_data_close();      /* stops dynamic polling immediately */
    modal_map_set_expanded(false);  /* container retracts to the readout */
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

static float node_screen_radius(const ImapNode* n) {
    return IMAP_NODE_RADIUS * s_m.zoom * node_depth_scale(n->pos);
}

static int node_at_point(float x, float y) {
    const ImapGraph* gr = instance_map_data_graph();
    int hit = -1;
    float best = 1e9f;
    for (int i = 0; i < gr->node_count; ++i) {
        Vector2 p = graph_to_screen(gr->nodes[i].pos);
        float r = node_screen_radius(&gr->nodes[i]) + 8.0f;
        float dx = x - p.x, dy = y - p.y;
        float d2 = dx * dx + dy * dy;
        if (d2 < r * r && d2 < best) { best = d2; hit = i; }
    }
    return hit;
}

/* ── Camera controls ────────────────────────────────────────────────────── */

static float clampf(float v, float lo, float hi) {
    return v < lo ? lo : (v > hi ? hi : v);
}

/* Zoom about an anchor point: the graph point under the anchor stays put. */
static void zoom_about(float factor, Vector2 anchor) {
    float nz = clampf(s_m.zoom_target * factor, IMAP_ZOOM_MIN, IMAP_ZOOM_MAX);
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

/* Glowing connection: layered strokes + a travelling pulse dot. */
static void draw_edge(const ImapEdge* e, int idx, float fade, double t) {
    const ImapGraph* gr = instance_map_data_graph();
    Color base = e->intra ? IMAP_EDGE_INTRA : IMAP_EDGE;

    if (e->intra) {
        /* Self-loop: orbit ring around the node. */
        Vector2 p = graph_to_screen(gr->nodes[e->source_node].pos);
        float   r = node_screen_radius(&gr->nodes[e->source_node]) + 10.0f;
        DrawRing(p, r - 1.5f, r + 1.5f, 0, 360, 40, fade_c((Color){ base.r, base.g, base.b, 60 }, fade));
        float ang = (float)fmod(t * 90.0 + idx * 40.0, 360.0) * DEG2RAD;
        DrawCircleV((Vector2){ p.x + cosf(ang) * r, p.y + sinf(ang) * r * IMAP_ISO_Y }, 2.5f,
                    fade_c(base, fade));
        return;
    }

    Vector2 a = graph_to_screen(gr->nodes[e->source_node].pos);
    Vector2 b = graph_to_screen(gr->nodes[e->target_node].pos);

    float pulse = 0.75f + 0.25f * sinf((float)t * 2.2f + (float)idx * 1.7f);
    DrawLineEx(a, b, 5.0f, fade_c((Color){ base.r, base.g, base.b, 22 }, fade * pulse));
    DrawLineEx(a, b, 2.6f, fade_c((Color){ base.r, base.g, base.b, 60 }, fade * pulse));
    DrawLineEx(a, b, 1.2f, fade_c((Color){ base.r, base.g, base.b, 170 }, fade));

    /* Travelling energy dot — directional flow along the portal. */
    float ft = (float)fmod(t * 0.45 + idx * 0.19, 1.0);
    Vector2 d = { a.x + (b.x - a.x) * ft, a.y + (b.y - a.y) * ft };
    DrawCircleV(d, 4.5f, fade_c((Color){ base.r, base.g, base.b, 50 }, fade));
    DrawCircleV(d, 2.0f, fade_c((Color){ 255, 255, 255, 200 }, fade));
}

static void draw_provider_badge(Vector2 at, float r, int active, int total,
                                Color color, bool diamond, float fade, double t) {
    if (0 == total) return;
    bool  lit   = active > 0;
    float pulse = lit ? 0.8f + 0.2f * sinf((float)t * 3.0f) : 1.0f;
    Color c     = lit ? color : (Color){ 110, 120, 140, 160 };
    if (lit) DrawCircleV(at, r + 3.5f, fade_c((Color){ c.r, c.g, c.b, 45 }, fade * pulse));
    if (diamond) DrawPoly(at, 4, r, 45.0f, fade_c(c, fade));
    else         DrawCircleV(at, r, fade_c(c, fade));

    char txt[8];
    snprintf(txt, sizeof(txt), "%d", lit ? active : total);
    int fs = 10;
    int w  = MeasureText(txt, fs);
    shadow_label(txt, (int)(at.x - w / 2.0f), (int)(at.y - fs / 2.0f), fs,
                 fade_c((Color){ 10, 14, 24, 255 }, fade));
}

static void draw_node(int idx, float fade, double t) {
    const ImapGraph* gr = instance_map_data_graph();
    const ImapNode*  n  = &gr->nodes[idx];
    Vector2 p = graph_to_screen(n->pos);
    float   r = node_screen_radius(n);

    bool selected  = idx == s_m.selected_node;
    bool is_player = 0 == strcmp(n->map_code, g_game_state.player.map_code);

    /* Ground shadow sells the tilt: squashed ellipse below the node. */
    DrawEllipse((int)p.x, (int)(p.y + r * 0.55f), r * 1.05f, r * 0.38f,
                fade_c((Color){ 0, 0, 0, 70 }, fade));

    /* Halo. */
    Color line = selected ? IMAP_SELECTED : IMAP_NODE_LINE;
    float halo = selected ? 0.9f + 0.1f * sinf((float)t * 4.0f) : 0.55f;
    DrawPoly(p, 6, r + 7.0f, 90.0f, fade_c((Color){ line.r, line.g, line.b, 26 }, fade * halo));

    /* Hex card. */
    DrawPoly(p, 6, r, 90.0f, fade_c(IMAP_NODE_FILL, fade));
    DrawPolyLinesEx(p, 6, r, 90.0f, selected ? 2.5f : 1.5f, fade_c(line, fade));
    DrawPolyLinesEx(p, 6, r - 4.0f, 90.0f, 1.0f,
                    fade_c((Color){ line.r, line.g, line.b, 60 }, fade));

    /* Player presence ring. */
    if (is_player) {
        float pr = r + 11.0f + 3.0f * sinf((float)t * 3.2f);
        DrawRing(p, pr - 1.6f, pr + 1.6f, 0, 360, 48, fade_c(IMAP_PLAYER, fade * 0.85f));
    }

    /* Strategic badges: quest (gold circle), action (cyan diamond). */
    draw_provider_badge((Vector2){ p.x - r * 0.62f, p.y - r * 0.62f }, 8.0f,
                        instance_map_data_node_active_quests(idx), n->quest_provider_count,
                        IMAP_QUEST, false, fade, t);
    draw_provider_badge((Vector2){ p.x + r * 0.62f, p.y - r * 0.62f }, 8.0f,
                        instance_map_data_node_active_actions(idx), n->action_provider_count,
                        IMAP_ACTION, true, fade, t);

    /* Local player marker, offset by cell fraction inside the node. */
    if (is_player && n->grid_x > 0 && n->grid_y > 0) {
        float fx = g_game_state.player.base.interp_pos.x / (float)n->grid_x - 0.5f;
        float fy = g_game_state.player.base.interp_pos.y / (float)n->grid_y - 0.5f;
        Vector2 mp = { p.x + fx * r * 1.1f, p.y + fy * r * 1.1f * IMAP_ISO_Y };
        DrawCircleV(mp, 5.0f, fade_c((Color){ IMAP_PLAYER.r, IMAP_PLAYER.g, IMAP_PLAYER.b, 60 }, fade));
        DrawCircleV(mp, 2.6f, fade_c(IMAP_PLAYER, fade));
    }

    /* Name plate. */
    int fs = 12;
    int w  = MeasureText(n->name, fs);
    shadow_label(n->name, (int)(p.x - w / 2.0f), (int)(p.y + r + 8.0f), fs,
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

    const char* title = "INSTANCE MAP";
    shadow_label(title, (int)(s_m.panel.x + 16.0f), (int)(s_m.panel.y + 12.0f), 16,
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
    shadow_label(sub, (int)(s_m.panel.x + 16.0f),
                 (int)(s_m.panel.y + 12.0f) + text_line_height(16) + 1, 11,
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
            for (int e = 0; e < gr->edge_count; ++e) draw_edge(&gr->edges[e], e, content, t);

            int order[IMAP_MAX_NODES];
            sorted_node_order(order, gr->node_count);
            for (int i = 0; i < gr->node_count; ++i) draw_node(order[i], content, t);

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
