#include "modal_instance_map.h"

#include "instance_map_data.h"
#include "modal_map.h"
#include "text.h"
#include "toolbar.h"
#include "ui_icon.h"

#include "domain/presentation_runtime.h"
#include "game_state.h"
#include "input/input.h"
#include "texture_cache.h"
#include "world_types.h"

#include <assert.h>
#include <limits.h>
#include <math.h>
#include <raylib.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ── Presentation constants ────────────────────────────────────────────── */
#define IMAP_NODE_SIDE      88.0f
#define IMAP_CAPABILITY_REVEAL_SIDE 320.0f
#define IMAP_ZOOM_MIN       0.45f
#define IMAP_ZOOM_INIT      3.5f    /* several wheel steps above the old 1.5 */
#define IMAP_ZOOM_MAX_FLOOR 3.2f
#define IMAP_ZOOM_STEP      1.15f
#define IMAP_DRAG_SLOP_PX   9.0f
#define IMAP_CAM_LAMBDA     11.0f   /* exp smoothing rate for pan/zoom      */
#define IMAP_ROTATE_DURATION 0.42f

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

/* Quarter-turn rotation keeps the packed map-card grid flush. */
static int       s_grid_rotation = 0;  /* settled target orientation */
static int       s_rotation_from = 0;
static int       s_rotation_to = 0;
static int       s_rotation_step = 0;
static float     s_rotation_age = IMAP_ROTATE_DURATION;
static Rectangle s_rotate_left_btn, s_rotate_right_btn;

/* Node preview backgrounds: each map's auto-captured Object Layer render,
 * fetched lazily from the server-supplied previewUrl through the engine
 * fetch pipeline (File blob for persisted maps, cached render for fallback). */
static TextureCache* s_preview_cache = NULL;

static void on_preview_blob(const FetchResponse* r) {
    texture_cache_on_blob_fetched(s_preview_cache, r);
}

/* ── Lifecycle ──────────────────────────────────────────────────────────── */

void modal_instance_map_init(void) {
    memset(&s_m, 0, sizeof(s_m));
    s_m.zoom = s_m.zoom_target = IMAP_ZOOM_INIT;
    s_m.selected_node = -1;
    s_grid_rotation = 0;
    s_rotation_from = 0;
    s_rotation_to = 0;
    s_rotation_step = 0;
    s_rotation_age = IMAP_ROTATE_DURATION;
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
    s_rotation_from = s_grid_rotation;
    s_rotation_to = s_grid_rotation;
    s_rotation_step = 0;
    s_rotation_age = IMAP_ROTATE_DURATION;
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

static float node_screen_side(void) {
    return IMAP_NODE_SIDE * s_m.zoom;
}

static bool grid_rotation_animating(void) {
    return s_rotation_age < IMAP_ROTATE_DURATION;
}

static float grid_rotation_progress(void) {
    if (!grid_rotation_animating()) return 1.0f;
    float t = s_rotation_age / IMAP_ROTATE_DURATION;
    return t * t * (3.0f - 2.0f * t);
}

static float grid_rotation_angle(void) {
    float base = (float)s_rotation_from * PI * 0.5f;
    return base + (float)s_rotation_step * PI * 0.5f * grid_rotation_progress();
}

static float grid_rotation_card_scale(void) {
    if (!grid_rotation_animating()) return 1.0f;
    return 1.0f - (1.0f - 0.70710678f) * sinf(PI * grid_rotation_progress());
}

static Vector2 node_grid_offset(const ImapNode* n, float angle) {
    const ImapGraph* gr = instance_map_data_graph();
    float x = (float)n->grid_col - ((float)gr->grid_cols - 1.0f) * 0.5f;
    float y = (float)n->grid_row - ((float)gr->grid_rows - 1.0f) * 0.5f;
    float cs = cosf(angle);
    float sn = sinf(angle);
    return (Vector2){ x * cs - y * sn, x * sn + y * cs };
}

static Vector2 node_center(const ImapNode* n) {
    Vector2 c = panel_center();
    Vector2 offset = node_grid_offset(n, grid_rotation_angle());
    float side = node_screen_side();
    return (Vector2){
        c.x + s_m.pan.x + offset.x * side,
        c.y + s_m.pan.y + offset.y * side,
    };
}

static float node_screen_radius(void) {
    return node_screen_side() * grid_rotation_card_scale() * 0.5f;
}

static Rectangle node_rect(const ImapNode* n) {
    Vector2 p = node_center(n);
    float   h = node_screen_radius();
    return (Rectangle){ p.x - h, p.y - h, h * 2.0f, h * 2.0f };
}

static int node_at_point(float x, float y) {
    const ImapGraph* gr = instance_map_data_graph();
    int hit = -1;
    float best = 1e9f;
    for (int i = 0; i < gr->node_count; ++i) {
        Vector2 p = node_center(&gr->nodes[i]);
        Rectangle r = node_rect(&gr->nodes[i]);
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

static float imap_zoom_max(void) {
    float w = s_m.panel.width > 4.0f ? s_m.panel.width : (float)GetScreenWidth();
    float max = 4.0f * w / IMAP_NODE_SIDE;
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

static void begin_grid_rotation(int step) {
    if (grid_rotation_animating()) return;
    s_rotation_from = s_grid_rotation;
    s_rotation_step = step;
    s_grid_rotation = (s_grid_rotation + step + 4) % 4;
    s_rotation_to = s_grid_rotation;
    s_rotation_age = 0.0f;
}

/* Release resolution: a clean (non-drag) press acts on what it landed on. */
static void resolve_click(Vector2 at) {
    if (CheckCollisionPointRec(at, s_rotate_left_btn)) {
        begin_grid_rotation(-1);
        return;
    }
    if (CheckCollisionPointRec(at, s_rotate_right_btn)) {
        begin_grid_rotation(1);
        return;
    }
    if (grid_rotation_animating()) return;
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

    if (grid_rotation_animating()) {
        s_rotation_age += dt;
        if (s_rotation_age > IMAP_ROTATE_DURATION) {
            s_rotation_age = IMAP_ROTATE_DURATION;
            s_rotation_from = s_rotation_to;
            s_rotation_step = 0;
        }
    }

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
    Color outline = { 0, 0, 0, c.a };
    for (int dy = -1; dy <= 1; dy++) {
        for (int dx = -1; dx <= 1; dx++) {
            if (0 == dx && 0 == dy) continue;
            DrawText(s, x + dx, y + dy, fs, outline);
        }
    }
    DrawText(s, x, y, fs, c);
}

static Color unfiltered_icon_color(float fade) {
    return (Color){ 255, 255, 255, (unsigned char)(255.0f * fade) };
}

static Rectangle pixel_inner(Rectangle bounds, float inset) {
    return (Rectangle){ bounds.x + inset, bounds.y + inset,
                        bounds.width - 2.0f * inset, bounds.height - 2.0f * inset };
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
    Rectangle inner = pixel_inner(bounds, 2.0f);
    DrawRectangleRec(inner, fade_c(fill, fade));
    draw_pixel_border(bounds, accent, focused, fade);
}

static void draw_pixel_outline(Rectangle bounds, int thickness, Color color) {
    if (bounds.width <= 0.0f || bounds.height <= 0.0f) return;
    DrawRectangle((int)bounds.x, (int)bounds.y, (int)bounds.width, thickness, color);
    DrawRectangle((int)bounds.x, (int)bounds.y, thickness, (int)bounds.height, color);
    DrawRectangle((int)bounds.x, (int)(bounds.y + bounds.height - thickness),
                  (int)bounds.width, thickness, color);
    DrawRectangle((int)(bounds.x + bounds.width - thickness), (int)bounds.y,
                  thickness, (int)bounds.height, color);
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

static void draw_pixel_icon_button(Rectangle button, const char* icon_id, bool enabled,
                                   int mx, int my, float fade) {
    bool hovered = enabled && CheckCollisionPointRec((Vector2){ (float)mx, (float)my }, button);
    Color fill = enabled ? (hovered ? (Color){ 60, 88, 124, 255 }
                                    : (Color){ 30, 44, 68, 255 })
                         : (Color){ 34, 38, 48, 255 };
    Color accent = enabled ? (hovered ? IMAP_SELECTED : IMAP_NODE_LINE)
                           : IMAP_TEXT_DIM;
    draw_pixel_panel(button, fill, accent, hovered, fade);
    if (enabled) {
        float size = button.width < button.height ? button.width : button.height;
        ui_icon_draw_ex(icon_id, button.x + button.width * 0.5f,
                        button.y + button.height * 0.5f, size * 0.56f, 0.0f,
                        unfiltered_icon_color(fade));
    }
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
        float fx = 0.5f + 0.36f * sinf((float)t * 0.83f + sx) * cosf((float)t * 0.31f + sy * 0.7f);
        float fy = 0.5f + 0.36f * sinf((float)t * 0.67f + sy) * cosf((float)t * 0.41f + sx * 1.9f);
        fx = floorf(fx * 8.0f + 0.5f) / 8.0f;
        fy = floorf(fy * 8.0f + 0.5f) / 8.0f;
        return (Vector2){ card.x + fx * card.width, card.y + fy * card.height };
    }
    return cell_to_card(card, n, (float)cell_x + 0.5f, (float)cell_y + 0.5f);
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
    const ImapGraph* gr = instance_map_data_graph();
    Color base = e->intra ? IMAP_EDGE_INTRA : IMAP_EDGE;

    Vector2 a = edge_endpoint(&gr->nodes[e->source_node], e->source_cell_x, e->source_cell_y,
                              t, idx * 2);
    Vector2 b = edge_endpoint(&gr->nodes[e->target_node], e->target_cell_x, e->target_cell_y,
                              t, idx * 2 + 1);

    float dx = b.x - a.x, dy = b.y - a.y;
    if (dx * dx + dy * dy < 16.0f) {
        int px = (int)roundf(a.x);
        int py = (int)roundf(a.y);
        DrawRectangle(px - 8, py - 8, 17, 17, fade_c(BLACK, fade));
        DrawRectangle(px - 5, py - 5, 11, 11, fade_c(base, fade));
        DrawRectangle(px - 2, py - 2, 5, 5, unfiltered_icon_color(fade));
        return;
    }

    draw_pixel_diagonal(a, b, base, fade);

    float phase = (float)fmod(t * 0.55 + idx * 0.19, 1.0);
    Vector2 pulse = pixel_path_point(a, b, phase);
    int px = (int)pulse.x;
    int py = (int)pulse.y;
    DrawRectangle(px - 7, py - 7, 15, 15, fade_c(BLACK, fade));
    DrawRectangle(px - 5, py - 5, 11, 11, fade_c(base, fade));
    DrawRectangle(px - 2, py - 2, 5, 5, unfiltered_icon_color(fade));
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

static Color presence_color(ImapPresenceStatus presence) {
    switch (presence) {
        case IMAP_PRESENCE_HOSTILE:       return (Color){ 220, 76, 70, 255 };
        case IMAP_PRESENCE_RESOURCE:      return (Color){ 105, 200, 105, 255 };
        case IMAP_PRESENCE_PORTAL:        return IMAP_EDGE;
        case IMAP_PRESENCE_PORTAL_RANDOM: return IMAP_EDGE_INTRA;
        case IMAP_PRESENCE_PASSIVE:       return IMAP_TEXT;
        case IMAP_PRESENCE_NONE:
        default:                          return IMAP_TEXT_DIM;
    }
}

static void draw_presence_marker(Vector2 at, const char* icon, Color tint,
                                 float size, float fade) {
    float shell_size = floorf(size) + 6.0f;
    Rectangle shell = { floorf(at.x - shell_size * 0.5f), floorf(at.y - shell_size * 0.5f),
                        shell_size, shell_size };
    draw_pixel_panel(shell, (Color){ 10, 16, 30, 220 }, tint, false, fade);
    if (icon && '\0' != icon[0]) {
        ui_icon_draw_ex(icon, at.x, at.y, size, 0.0f, unfiltered_icon_color(fade));
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
        capability_colors[capability_count++] = quest_active ? IMAP_QUEST : IMAP_TEXT_DIM;
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
        draw_pixel_active_pulse(tab, action_active ? IMAP_ACTION : IMAP_QUEST, fade, time);
    else if (0 != pulse_accent.a)
        draw_pixel_active_pulse(tab, pulse_accent, fade, time);

    float x = tab.x + 5.0f;
    has_item = false;
    if (show_stats) {
        ui_icon_draw_ex("stats", x + icon_size * 0.5f, at.y, (float)icon_size, 0.0f,
                        unfiltered_icon_color(fade));
        x += icon_size + 4.0f;
        shadow_label(stats, (int)x, (int)(at.y - font_size * 0.5f), font_size,
                     fade_c(IMAP_TEXT, fade));
        x += stats_width;
        has_item = true;
    }
    if (show_status) {
        if (has_item) x += 6.0f;
        ui_icon_draw_ex(status_icon, x + icon_size * 0.5f, at.y, (float)icon_size, 0.0f,
                        unfiltered_icon_color(fade));
        x += icon_size;
        has_item = true;
    }
    for (int i = 0; i < capability_count; ++i) {
        if (has_item) x += 4.0f;
        const char* icon = capability_icons[i];
        if (icon && '\0' != icon[0]) {
            ui_icon_draw_ex(icon, x + icon_size * 0.5f, at.y, (float)icon_size, 0.0f,
                            unfiltered_icon_color(fade));
        }
        DrawRectangle((int)(x + icon_size * 0.5f - 2.0f), (int)(at.y + icon_size * 0.5f - 3.0f),
                      4, 3, fade_c(capability_colors[i], fade));
        x += icon_size;
        has_item = true;
    }
}

/* Card pass runs before edges so links land on the map surface. */
static void draw_node_card(int idx, float fade, double time) {
    const ImapGraph* gr = instance_map_data_graph();
    const ImapNode*  n  = &gr->nodes[idx];
    Rectangle card = node_rect(n);

    bool selected = idx == s_m.selected_node;
    bool hovered = !grid_rotation_animating() &&
        CheckCollisionPointRec(GetMousePosition(), card);
    Color accent = selected ? IMAP_SELECTED : hovered ? (Color){ 120, 220, 255, 255 } : IMAP_NODE_LINE;
    Color fill = selected ? (Color){ 55, 44, 24, 245 }
               : hovered ? (Color){ 32, 48, 74, 245 }
                         : IMAP_NODE_FILL;
    draw_pixel_panel(card, fill, accent, selected || hovered, fade);
    if (selected) draw_pixel_active_pulse(card, IMAP_SELECTED, fade, time);
    if ('\0' != n->preview_url[0]) {
        Texture2D tex = texture_cache_get(s_preview_cache, n->preview_url);
        if (0 != tex.id) {
            Rectangle src  = { 0, 0, (float)tex.width, (float)tex.height };
            Rectangle dest = pixel_inner(card, 4.0f);
            DrawTexturePro(tex, src, dest, (Vector2){ 0, 0 }, 0.0f,
                           unfiltered_icon_color(fade));
        }
    }
    draw_pixel_border(card, accent, selected || hovered, fade);
    if (selected || hovered) {
        Color spark = fade_c(selected ? IMAP_SELECTED : WHITE, fade);
        DrawRectangle((int)(card.x + card.width - 7.0f), (int)(card.y + 3.0f), 3, 3, spark);
        DrawRectangle((int)(card.x + card.width - 4.0f), (int)(card.y + 6.0f), 2, 2, spark);
    }
}

static void draw_node_name(const ImapNode* n, Rectangle card, bool selected, float fade) {
    int font_size = card.width >= 150.0f ? 15 : 11;
    int max_width = (int)card.width - 8;
    char label[IMAP_NAME_MAX];
    strncpy(label, n->name, sizeof(label) - 1);
    label[sizeof(label) - 1] = '\0';
    int length = (int)strlen(label);
    while (length > 3 && MeasureText(label, font_size) > max_width) {
        label[--length] = '\0';
    }
    if (length < (int)strlen(n->name) && length >= 3) {
        label[length - 3] = '.';
        label[length - 2] = '.';
        label[length - 1] = '.';
    }
    float label_height = (float)text_line_height(font_size) + 4.0f;
    Rectangle strip = { card.x, card.y + card.height - label_height, card.width, label_height };
    DrawRectangleRec(strip, fade_c((Color){ 4, 8, 16, 220 }, fade));
    DrawRectangle((int)strip.x, (int)strip.y, (int)strip.width, 1,
                  fade_c(selected ? IMAP_SELECTED : IMAP_NODE_LINE, fade));
    int width = MeasureText(label, font_size);
    shadow_label(label, (int)(card.x + (card.width - width) * 0.5f),
                 (int)(strip.y + 2.0f), font_size,
                 fade_c(selected ? IMAP_SELECTED : IMAP_TEXT, fade));
}

/* Overlay pass: static presence is always shown; capability tabs appear only
 * once the card has reached the high-zoom inspection size. */
static void draw_node_overlay(int idx, float fade, double t) {
    const ImapGraph* gr = instance_map_data_graph();
    const ImapNode*  n  = &gr->nodes[idx];
    Rectangle card = node_rect(n);

    bool selected  = idx == s_m.selected_node;
    bool is_player = 0 == strcmp(n->map_code, g_game_state.player.map_code);

    bool zoomed = card.width >= IMAP_CAPABILITY_REVEAL_SIDE;
    float icon_size = zoomed ? poi_radius(node_screen_radius()) * 2.2f : 24.0f;
    for (int i = 0; i < gr->presence_poi_count; ++i) {
        const ImapPresencePoi* poi = &gr->presence_pois[i];
        if (poi->node != idx) continue;
        Vector2 at = cell_to_card(card, n, poi->cell_x + 0.5f, poi->cell_y + 0.5f);
        uint8_t status = presence_status_icon(poi->presence_status);
        const char* icon = presentation_runtime_status_icon(status);
        Color tint = presence_color(poi->presence_status);
        if (zoomed) {
            /* Sum-stats readout only for plain living presence — never
             * portals, quest/action provider cells, or provider/static
             * behaviors (the engine zeroes those, so a 0 means "hidden"). */
            bool living = IMAP_PRESENCE_PASSIVE == poi->presence_status ||
                          IMAP_PRESENCE_HOSTILE == poi->presence_status ||
                          IMAP_PRESENCE_RESOURCE == poi->presence_status;
            bool show_stats = poi->show_stats_value && living &&
                              0 == poi->capabilities;
            draw_zoomed_capability_tab(at, icon, poi->capabilities, poi->action_active,
                                       poi->quest_active, show_stats, poi->stats_sum,
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
        draw_pixel_outline(ring, 3, fade_c(IMAP_PLAYER, fade * 0.85f));
        Vector2 mp = cell_to_card(card, n,
                                  g_game_state.player.base.interp_pos.x,
                                  g_game_state.player.base.interp_pos.y);
        uint8_t self_status = 0 != g_game_state.player.base.status_icon
            ? g_game_state.player.base.status_icon : STATUS_ICON_PLAYER;
        const char* self_ic = presentation_runtime_status_icon(self_status);
        if (zoomed) {
            draw_zoomed_capability_tab(mp, self_ic, 0, false, false, true,
                                       g_game_state.player.base.stats_sum,
                                       IMAP_PLAYER, IMAP_PLAYER, fade, t);
        } else {
            float shell = floorf(icon_size) + 6.0f;
            Rectangle marker = { floorf(mp.x - shell * 0.5f), floorf(mp.y - shell * 0.5f),
                                 shell, shell };
            draw_pixel_active_pulse(marker, IMAP_PLAYER, fade, t);
            draw_presence_marker(mp, self_ic, IMAP_PLAYER, icon_size, fade);
        }
    }
    draw_node_name(n, card, selected, fade);
}

/* Packed cards share grid edges, so declaration order is the stable painter order. */
static void sorted_node_order(int* order, int n) {
    for (int i = 0; i < n; ++i) order[i] = i;
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

    /* Count lines to compute dynamic height. */
    int lines = 1; /* name */
    lines++;        /* sector line */

    char quest_buf[96];
    snprintf(quest_buf, sizeof(quest_buf), "%d active / %d",
             instance_map_data_node_active_quests(s_m.selected_node), n->quest_provider_count);
    if (n->quest_provider_count > 0) lines++; /* quest row */

    char action_buf[96];
    snprintf(action_buf, sizeof(action_buf), "%d active / %d",
             instance_map_data_node_active_actions(s_m.selected_node), n->action_provider_count);
    if (n->action_provider_count > 0) lines++; /* action row */

    if (n->portal_count > 0) lines++; /* portal row */

    float content_h = pad * 2.0f + (float)lines * line_h;
    float panel_h = content_h < 90.0f ? 90.0f : content_h;
    float rx = s_m.panel.x + 14.0f;
    float ry = s_m.panel.y + s_m.panel.height - panel_h - 14.0f;
    Rectangle r = { rx, ry, panel_w, panel_h };

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
        int max_name_w = (int)(panel_w - pad * 2.0f - 6.0f - 8.0f);
        char name_buf[IMAP_NAME_MAX + 4];
        strncpy(name_buf, n->name, sizeof(name_buf) - 1);
        name_buf[sizeof(name_buf) - 1] = '\0';
        while ((int)strlen(name_buf) > 3 && MeasureText(name_buf, name_font) > max_name_w) {
            name_buf[strlen(name_buf) - 1] = '\0';
        }
        int nlen = (int)strlen(name_buf);
        if (nlen < (int)strlen(n->name) && nlen >= 3) {
            name_buf[nlen - 3] = '.';
            name_buf[nlen - 2] = '.';
            name_buf[nlen - 1] = '.';
            name_buf[nlen] = '\0';
        }
        shadow_label(name_buf, x, y, name_font, fade_c(IMAP_SELECTED, fade));
    }
    y += line_h;

    /* ── Sector code + grid dimensions ── */
    {
        char line[96];
        snprintf(line, sizeof(line), "SECTOR %s  ·  %dx%d", n->map_code, n->grid_x, n->grid_y);
        shadow_label(line, x, y, sub_font, fade_c(IMAP_TEXT_DIM, fade));
    }
    y += line_h;

    /* ── Quest providers ── */
    if (n->quest_provider_count > 0) {
        ui_icon_draw_ex("quest", (float)x + (float)icon_sz * 0.5f,
                        (float)y + (float)line_h * 0.5f, (float)icon_sz, 0.0f,
                        fade_c(IMAP_QUEST, fade));
        shadow_label(quest_buf, x + icon_sz + 6, y, stat_font, fade_c(IMAP_QUEST, fade));
        y += line_h;
    }

    /* ── Action providers ── */
    if (n->action_provider_count > 0) {
        const char* action_icon = presentation_runtime_status_icon(STATUS_ICON_ACTION_PROVIDER);
        if (action_icon && '\0' != action_icon[0]) {
            ui_icon_draw_ex(action_icon, (float)x + (float)icon_sz * 0.5f,
                            (float)y + (float)line_h * 0.5f, (float)icon_sz, 0.0f,
                            fade_c(IMAP_ACTION, fade));
        }
        shadow_label(action_buf, x + icon_sz + 6, y, stat_font, fade_c(IMAP_ACTION, fade));
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

    /* Grid rotation controls keep all map cards on the same packed tile plane. */
    float ab = 30.0f;
    float rotate_x = s_m.panel.x + s_m.panel.width - 2.0f * ab - 16.0f;
    float rotate_y = s_m.panel.y + s_m.panel.height * 0.5f - ab * 0.5f;
    s_rotate_left_btn  = (Rectangle){ rotate_x, rotate_y, ab, ab };
    s_rotate_right_btn = (Rectangle){ rotate_x + ab + 6.0f, rotate_y, ab, ab };
    int mx = GetMouseX();
    int my = GetMouseY();
    bool enabled = !grid_rotation_animating();
    draw_pixel_icon_button(s_rotate_left_btn, "arrow-left", enabled, mx, my, fade);
    draw_pixel_icon_button(s_rotate_right_btn, "arrow-right", enabled, mx, my, fade);
    if (grid_rotation_animating()) {
        int blocks = (int)(grid_rotation_progress() * 5.0f);
        for (int i = 0; i < blocks; i++) {
            DrawRectangle((int)(s_rotate_left_btn.x + 4.0f + i * 4.0f),
                          (int)(s_rotate_left_btn.y + s_rotate_left_btn.height - 5.0f),
                          3, 2, fade_c(IMAP_SELECTED, fade));
        }
    }

    const char* hint = "DRAG PAN · WHEEL / PINCH ZOOM · ROTATE GRID · TAP NODE";
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
