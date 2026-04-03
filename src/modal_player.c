/**
 * @file modal_player.c
 * @brief Compact frameless HUD for player information.
 *
 * Renders a minimal two-line HUD at the top-right corner:
 *   ● <map_code>  (drop shadow)
 *   (<x>, <y>)  <fps>fps
 *
 * No border frame — just a semi-transparent rounded background + text shadows.
 * Coin balance is now visible in the pinned coin slot of the inventory bar, so
 * it is intentionally omitted here.
 */

#include "modal_player.h"
#include "game_state.h"
#include "client.h"
#include <raylib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

/* Global instance */
ModalPlayer g_modal_player = {0};

/* ── Initialisation ───────────────────────────────────────────────────── */

int modal_player_init(void) {
    memset(&g_modal_player, 0, sizeof(ModalPlayer));
    g_modal_player.show_connection  = true;
    g_modal_player.show_map         = true;
    g_modal_player.show_position    = true;
    g_modal_player.show_fps         = true;
    g_modal_player.cached_fps       = 60.0f;
    g_modal_player.last_fps_update  = 0.0;
    return 0;
}

void modal_player_cleanup(void) {
    memset(&g_modal_player, 0, sizeof(ModalPlayer));
}

/* ── Update ───────────────────────────────────────────────────────────── */

void modal_player_update(float delta_time) {
    (void)delta_time;
    double t = GetTime();
    if (t - g_modal_player.last_fps_update >= 0.5) {
        g_modal_player.cached_fps      = (float)GetFPS();
        g_modal_player.last_fps_update = t;
    }
}

/* ── Draw helpers ─────────────────────────────────────────────────────── */

/* shadow_text renders `text` with a subtle 1-pixel black shadow. */
static void shadow_text(const char* text, int x, int y, int fs, Color c) {
    DrawText(text, x + 1, y + 1, fs, (Color){ 0, 0, 0, 160 });
    DrawText(text, x,     y,     fs, c);
}

/* ── Draw ─────────────────────────────────────────────────────────────── */

void modal_player_draw(int screen_width, int screen_height) {
    (void)screen_height;

    /* Always draw — shows disconnected state too. */
    bool connected = client_is_connected();

    const char* map = g_game_state.player.map_code;
    float px        = g_game_state.player.base.interp_pos.x;
    float py        = g_game_state.player.base.interp_pos.y;
    int   fps       = (int)roundf(g_modal_player.cached_fps);
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
    int bx     = screen_width - box_w - margin;
    int by     = margin;

    /* ── Background: rounded rect, no border ─────────────────────────── */
    DrawRectangleRounded(
        (Rectangle){ (float)bx, (float)by, (float)box_w, (float)box_h },
        0.35f, 8, (Color){ 0, 0, 0, 130 });

    /* ── Status dot (connection indicator) ───────────────────────────── */
    Color dot_c = connected ? (Color){ 60, 220, 80, 255 }
                            : (Color){ 220, 60, 60, 255 };
    DrawCircle(bx + pad + 4, by + pad + fs / 2 + 1, 3.0f, dot_c);

    /* ── Line 1: map code ─────────────────────────────────────────────── */
    shadow_text(line1, bx + pad, by + pad, fs, (Color){ 240, 215, 100, 230 });

    /* ── Line 2: position + fps ───────────────────────────────────────── */
    shadow_text(line2, bx + pad, by + pad + lsp, fs, (Color){ 180, 195, 220, 210 });
}
