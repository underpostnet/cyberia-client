#include "modal_map.h"
#include "modal_instance_map.h"
#include "text.h"
#include "ui_button.h"

#include "game_render.h"
#include "network/game_client.h"
#include "game_state.h"

#include <assert.h>
#include <math.h>
#include <raylib.h>
#include <stdio.h>
#include <string.h>

/* Container expand/retract transition length, seconds. */
#define MODAL_MAP_EXPAND_DURATION 0.28f

/* Global instance */
ModalMap g_modal_map = {0};

/* ── Initialisation ───────────────────────────────────────────────────── */

int modal_map_init(void) {
    memset(&g_modal_map, 0, sizeof(ModalMap));
    g_modal_map.show_connection = true;
    g_modal_map.show_map        = true;
    g_modal_map.show_position   = true;
    g_modal_map.show_fps        = true;
    g_modal_map.cached_fps      = 60.0f;
    g_modal_map.last_fps_update = 0.0;
    return 0;
}

void modal_map_cleanup(void) {
    memset(&g_modal_map, 0, sizeof(ModalMap));
}

/* ── Update ───────────────────────────────────────────────────────────── */

void modal_map_update(float delta_time) {
    g_modal_map.age += delta_time;

    double t = GetTime();
    if (t - g_modal_map.last_fps_update >= 0.5) {
        g_modal_map.cached_fps      = (float)GetFPS();
        g_modal_map.last_fps_update = t;
    }

    /* Advance the container morph toward its mode. */
    float step = delta_time / MODAL_MAP_EXPAND_DURATION;
    if (g_modal_map.expanded) {
        g_modal_map.expand_t += step;
        if (g_modal_map.expand_t > 1.0f) g_modal_map.expand_t = 1.0f;
    } else {
        g_modal_map.expand_t -= step;
        if (g_modal_map.expand_t < 0.0f) g_modal_map.expand_t = 0.0f;
    }
}

void modal_map_set_expanded(bool expanded) {
    g_modal_map.expanded = expanded;
}

bool modal_map_is_expanded(void) {
    return g_modal_map.expanded;
}

float modal_map_expand_progress(void) {
    float t = g_modal_map.expand_t;
    return t * t * (3.0f - 2.0f * t); /* smoothstep */
}

/* ── Draw helpers ─────────────────────────────────────────────────────── */

/* shadow_text renders `text` with a subtle 1-pixel black shadow. */
static void shadow_text(const char* text, int x, int y, int fs, Color c) {
    DrawText(text, x + 1, y + 1, fs, (Color){ 0, 0, 0, (unsigned char)(160.0f * (c.a / 255.0f)) });
    DrawText(text, x,     y,     fs, c);
}

/* ── Draw ─────────────────────────────────────────────────────────────── */

void modal_map_draw(int screen_width, int screen_height) {
    const char* map = g_game_state.player.map_code;
    float px        = g_game_state.player.base.interp_pos.x;
    float py        = g_game_state.player.base.interp_pos.y;
    int   fps       = (int)roundf(g_modal_map.cached_fps);
    bool  init      = g_game_state.init_received;

    /* ── Build two compact lines ──────────────────────────────────────── */

    /* Line 1:  [dot space] <map>   (dot drawn separately) */
    char line1[64];
    snprintf(line1, sizeof(line1), "  %s",
             (init && map && map[0]) ? map : "--");

    /* Line 2:  (<x>, <y>)  <fps>fps */
    char line2[64];
    if (init)
        snprintf(line2, sizeof(line2), "(%.0f,%.0f) %dfps", px, py, fps);
    else
        snprintf(line2, sizeof(line2), "--  %dfps", fps);

    /* ── Measure for background rect ─────────────────────────────────── */
    int fs  = 11;          /* font size */
    int pad = 5;           /* inner padding */
    int lsp = fs + 4;      /* line spacing */
    int w1  = MeasureText(line1, fs);
    int w2  = MeasureText(line2, fs);
    int box_w  = (w1 > w2 ? w1 : w2) + pad * 2 + 10; /* +10 for dot */
    int box_h  = lsp * 2 + pad * 2 - 2;
    int margin = 10;
    /* Shifted left of its natural corner position to leave room for the Map
     * toggle + fullscreen buttons pinned in the top-right corner. */
    int bx = screen_width - box_w - margin
             - 2 * (FULLSCREEN_BTN_SIZE + FULLSCREEN_BTN_MARGIN);
    int by = margin;

    g_modal_map.bounds = (Rectangle){ (float)bx, (float)by, (float)box_w, (float)box_h };

    /* The compact readout fades out while the container expands into the
     * Instance Map (modal_instance_map draws the expanded content). */
    float fade = modal_pop_alpha(g_modal_map.age) * (1.0f - modal_map_expand_progress());
    if (fade > 0.01f) {
        /* ── Background: rounded rect, no border, shared fade-in ──────── */
        DrawRectangleRounded(
            g_modal_map.bounds,
            0.35f, 8, (Color){ 0, 0, 0, (unsigned char)(130.0f * fade) });

        /* ── Status dot (connection indicator) ────────────────────────── */
        /* Always draw — shows disconnected state too. */
        bool connected = connection_is_open();
        Color dot_c = connected ? (Color){ 60, 220, 80, 255 }
                                : (Color){ 220, 60, 60, 255 };
        dot_c.a = (unsigned char)((float)dot_c.a * fade);
        DrawCircle(bx + pad + 4, by + pad + fs / 2 + 1, 3.0f, dot_c);

        /* ── Line 1: map code ──────────────────────────────────────────── */
        Color line1_c = { 240, 215, 100, 230 };
        line1_c.a = (unsigned char)((float)line1_c.a * fade);
        shadow_text(line1, bx + pad, by + pad, fs, line1_c);

        /* ── Line 2: position + fps ────────────────────────────────────── */
        Color line2_c = { 180, 195, 220, 210 };
        line2_c.a = (unsigned char)((float)line2_c.a * fade);
        shadow_text(line2, bx + pad, by + pad + lsp, fs, line2_c);
    }

    /* ── Map toggle button: beside the fullscreen button, same chrome ─── */
    g_modal_map.map_btn_bounds = (Rectangle){
        (float)(screen_width - 2 * (FULLSCREEN_BTN_SIZE + FULLSCREEN_BTN_MARGIN)),
        (float)FULLSCREEN_BTN_MARGIN,
        (float)FULLSCREEN_BTN_SIZE, (float)FULLSCREEN_BTN_SIZE,
    };
    /* The Map button becomes the Close button while the container is
     * expanded — one slot, one action, mirroring the fullscreen toggle. */
    bool expanded = modal_instance_map_is_open();
    UIButtonStyle style = {
        .icon_id    = expanded ? "close-yellow" : "map",
        .icon_size  = FULLSCREEN_BTN_SIZE - 12,
        .bg         = { 20, 20, 35, 200 },
        .bg_hover   = { 50, 50, 70, 220 },
        .border     = { 80, 80, 120, 180 },
        .border_selected = { 90, 210, 250, 220 },
    };
    Vector2 mp = GetMousePosition();
    ui_button_draw(g_modal_map.map_btn_bounds, &style,
                   ui_button_resolve_state(true, expanded,
                                           CheckCollisionPointRec(mp, g_modal_map.map_btn_bounds)));
}

Rectangle modal_map_bounds(void) {
    return g_modal_map.bounds;
}

bool modal_map_handle_expand_click(int mx, int my) {
    if (!ui_button_hit(g_modal_map.map_btn_bounds, mx, my)) return false;
    modal_instance_map_toggle();
    return true;
}
