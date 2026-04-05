/**
 * @file inventory_modal.c
 * @brief Full-screen inventory item detail modal implementation.
 *
 * Architecture notes:
 *   - The modal is purely a UI layer: it reads game state and sends intents.
 *   - Item metadata (description, stats) is fetched lazily via
 *     get_or_fetch_object_layer() which caches results in the OL manager.
 *   - Activation intent is sent via client_send() as a JSON string
 *     matching the server's existing "item_activation" handler in
 *     handlers.go.  The server validates, swaps if needed, and pushes
 *     the updated state back in the next AOI frame.
 *   - The sprite preview uses ol_as_animated_ico so it plays the live
 *     animation for the selected direction/mode pair.
 *   - Direction (up/down/left/right) and mode (idle/walking) buttons let
 *     the player inspect how the item looks in every animation state, just
 *     like ObjectLayerEngineViewer.js in the JS client.
 */

#include "inventory_modal.h"
#include "inventory_bar.h"
#include "ol_as_animated_ico.h"
#include "game_state.h"
#include "client.h"
#include "object_layers_management.h"
#include "object_layer.h"
#include <raylib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

/* ── Module state ─────────────────────────────────────────────────────── */

static ObjectLayersManager* s_ol_manager = NULL;
static bool  s_open       = false;
static int   s_inv_idx    = -1;
static float s_age        = 0.0f;

/* Direction / mode viewer state */
static char  s_dir[16]     = "down";
static char  s_mode[16]    = "idle";
static char  s_dir_str[32] = "down_idle";

/* Cached button rects (updated every draw, used in handle_click) */
#define DIR_BTN_COUNT 5          /* 4 directions + 1 mode toggle */
static Rectangle s_dir_btn_rects[DIR_BTN_COUNT];
static bool      s_dir_btn_enabled[DIR_BTN_COUNT]; /* false = no frames */

/* ── Layout constants ───────────────────────────────────────────────────── */

#define MODAL_CARD_W      380
#define MODAL_CARD_H      540
#define MODAL_SPRITE_SIZE 112     /* animated item preview */
#define MODAL_FONT_TITLE  18
#define MODAL_FONT_BODY   13
#define MODAL_FONT_STAT   12
#define MODAL_BTN_W       140
#define MODAL_BTN_H        40
#define MODAL_CLOSE_SIZE   30

#define DIR_BTN_W  52
#define DIR_BTN_H  24
#define DIR_BTN_GAP 4
#define MODE_BTN_W 76

/* ── Colours ──────────────────────────────────────────────────────────── */

static const Color C_OVERLAY_BG    = {   0,   0,   0, 170 };
static const Color C_CARD_BG       = {  18,  18,  30, 240 };
static const Color C_CARD_BORDER   = {  80,  80, 130, 220 };
static const Color C_TITLE         = { 220, 220, 255, 255 };
static const Color C_BODY          = { 180, 180, 200, 220 };
static const Color C_STAT_LABEL    = { 140, 160, 200, 220 };
static const Color C_STAT_VAL      = { 100, 220, 140, 255 };
static const Color C_BTN_ACTIVATE  = {  30, 120,  60, 240 };
static const Color C_BTN_DEACT     = { 120,  40,  40, 240 };
static const Color C_BTN_TEXT      = { 230, 230, 230, 255 };
static const Color C_CLOSE_BTN     = {  80,  80, 110, 220 };
static const Color C_CLOSE_X       = { 210, 210, 230, 255 };
static const Color C_QTY           = { 255, 215,   0, 255 };
static const Color C_DIR_BTN_SEL   = {  50,  80, 160, 240 };   /* selected dir */
static const Color C_DIR_BTN_DEF   = {  35,  38,  60, 220 };   /* unselected  */
static const Color C_DIR_BTN_DIS   = {  22,  22,  30, 180 };   /* disabled    */
static const Color C_MODE_IDLE     = {  40,  90,  40, 230 };
static const Color C_MODE_WALK     = {  90,  50,  10, 230 };

/* ── Helpers ──────────────────────────────────────────────────────────── */

static void rebuild_dir_str(void) {
    snprintf(s_dir_str, sizeof(s_dir_str), "%s_%s", s_dir, s_mode);
}

static Rectangle card_rect(int sw, int sh, float scale) {
    float cx = sw * 0.5f, cy = sh * 0.5f;
    float w = MODAL_CARD_W * scale, h = MODAL_CARD_H * scale;
    return (Rectangle){ cx - w * 0.5f, cy - h * 0.5f, w, h };
}

static void send_activation(const char* item_id, bool active) {
    char buf[256];
    snprintf(buf, sizeof(buf),
        "{\"type\":\"item_activation\","
        "\"payload\":{\"itemId\":\"%s\",\"active\":%s}}",
        item_id, active ? "true" : "false");
    client_send(buf);
}

/* hit_rect returns true if (mx,my) is inside r. */
static bool hit_rect(int mx, int my, Rectangle r) {
    return ((float)mx >= r.x && (float)mx < r.x + r.width &&
            (float)my >= r.y && (float)my < r.y + r.height);
}

/* draw_small_btn draws direction/mode buttons and returns true if clicked. */
static bool draw_small_btn(Rectangle r, const char* label, Color bg, bool selected,
                            bool enabled, int mx, int my) {
    Color used_bg = !enabled ? C_DIR_BTN_DIS : (selected ? C_DIR_BTN_SEL : bg);
    bool hovered  = enabled && hit_rect(mx, my, r);
    if (hovered) {
        used_bg = (Color){ (uint8_t)((int)used_bg.r + 18),
                           (uint8_t)((int)used_bg.g + 18),
                           (uint8_t)((int)used_bg.b + 18), used_bg.a };
    }
    DrawRectangleRec(r, used_bg);
    if (selected)
        DrawRectangleLinesEx(r, 1.5f, (Color){ 120, 160, 255, 200 });
    else
        DrawRectangleLinesEx(r, 1.0f, (Color){ 60, 65, 90, 160 });

    int fs = MODAL_FONT_STAT;
    int tw = MeasureText(label, fs);
    Color text_c = enabled ? (selected ? (Color){220,230,255,255} : (Color){160,170,190,220})
                           : (Color){60,65,75,140};
    DrawText(label, (int)(r.x + (r.width - tw) * 0.5f),
             (int)(r.y + (r.height - fs) * 0.5f), fs, text_c);
    return false; /* click handled in handle_click */
}

/* dir_has_frames checks if the atlas has any frames for dir+mode combo. */
static bool dir_has_frames(AtlasSpriteSheetData* atlas,
                            const char* dir, const char* mode) {
    if (!atlas) return false;
    char full[48];
    snprintf(full, sizeof(full), "%s_%s", dir, mode);
    const DirectionFrameData* dfd = atlas_get_direction_frames(atlas, full);
    return (dfd && dfd->count > 0);
}

/* ── Public API ──────────────────────────────────────────────────────── */

void inventory_modal_init(ObjectLayersManager* ol_manager) {
    s_ol_manager = ol_manager;
    s_open       = false;
    s_inv_idx    = -1;
    s_age        = 0.0f;
}

void inventory_modal_open(int inv_idx) {
    if (inv_idx < 0 || inv_idx >= g_game_state.full_inventory_count) return;
    s_inv_idx = inv_idx;
    s_age     = 0.0f;
    s_open    = true;
    /* Reset to default direction/mode on every open */
    strncpy(s_dir,  "down", sizeof(s_dir)  - 1);
    strncpy(s_mode, "idle", sizeof(s_mode) - 1);
    rebuild_dir_str();
}

void inventory_modal_close(void) {
    s_open    = false;
    s_inv_idx = -1;
}

bool inventory_modal_is_open(void) { return s_open; }

void inventory_modal_update(float dt) {
    if (!s_open) return;
    s_age += dt;
}

void inventory_modal_draw(void) {
    if (!s_open || s_inv_idx < 0) return;
    if (s_inv_idx >= g_game_state.full_inventory_count) { s_open = false; return; }

    int screen_w = GetScreenWidth();
    int screen_h = GetScreenHeight();
    const ObjectLayerState* ols = &g_game_state.full_inventory[s_inv_idx];

    /* Pop-in animation */
    float pop_dur = 0.15f;
    float scale   = (s_age < pop_dur)
        ? (0.80f + 0.20f * (1.0f - powf(1.0f - s_age / pop_dur, 3.0f)))
        : 1.0f;

    float fade    = s_age / 0.20f;
    if (fade > 1.0f) fade = 1.0f;
    unsigned char oa = (unsigned char)(C_OVERLAY_BG.a * fade);

    /* 1. Backdrop */
    DrawRectangle(0, 0, screen_w, screen_h,
                  (Color){ C_OVERLAY_BG.r, C_OVERLAY_BG.g, C_OVERLAY_BG.b, oa });

    /* 2. Card */
    Rectangle card = card_rect(screen_w, screen_h, scale);
    DrawRectangleRec(card, C_CARD_BG);
    DrawRectangleLinesEx(card, 2.0f, C_CARD_BORDER);

    float cx  = card.x;
    float cy  = card.y;
    float cw  = card.width;
    int   pad = 14;

    /* 3. Close button */
    Rectangle close_r = { cx + cw - MODAL_CLOSE_SIZE - 6, cy + 6,
                           MODAL_CLOSE_SIZE, MODAL_CLOSE_SIZE };
    DrawRectangleRec(close_r, C_CLOSE_BTN);
    DrawRectangleLinesEx(close_r, 1.0f, C_CARD_BORDER);
    int xfs = MODAL_FONT_BODY;
    int xtw = MeasureText("X", xfs);
    DrawText("X", (int)(close_r.x + (close_r.width  - xtw) * 0.5f),
                  (int)(close_r.y + (close_r.height - xfs) * 0.5f), xfs, C_CLOSE_X);

    /* ── Fetch atlas for direction button enable state ───────────────── */
    AtlasSpriteSheetData* atlas = NULL;
    if (s_ol_manager && ols->item_id[0] != '\0')
        atlas = get_or_fetch_atlas_data(s_ol_manager, ols->item_id);

    /* 4. Animated sprite via ol_as_animated_ico */
    float sprite_x = cx + cw * 0.5f - MODAL_SPRITE_SIZE * 0.5f;
    float sprite_y = cy + 14.0f;
    Rectangle sprite_dst = { sprite_x, sprite_y,
                              MODAL_SPRITE_SIZE, MODAL_SPRITE_SIZE };

    if (ols->item_id[0] != '\0') {
        ol_as_ico_draw(s_ol_manager, ols->item_id,
                       (int)sprite_x, (int)sprite_y, MODAL_SPRITE_SIZE,
                       s_dir_str, 0, WHITE);
    }
    DrawRectangleLinesEx(sprite_dst, 1.0f, C_CARD_BORDER);

    /* 5. Direction / mode buttons — placed right below the sprite ────── */
    float dir_row_y = sprite_y + MODAL_SPRITE_SIZE + 6.0f;
    {
        /* Compute total row width to centre it */
        int n_dir   = 4;
        int total_w = n_dir * DIR_BTN_W + (n_dir - 1) * DIR_BTN_GAP
                      + DIR_BTN_GAP * 2 + MODE_BTN_W;
        float row_x = cx + (cw - total_w) * 0.5f;
        float row_y = dir_row_y;

        /* mouse position (for hover, not click) */
        int mx = GetMouseX(), my = GetMouseY();

        const char* dirs[4]   = { "up",  "down",  "left",  "right" };
        const char* labels[4] = { "^Up", "vDn",   "<Lf",   "Rt>"  };

        for (int i = 0; i < 4; i++) {
            Rectangle r = { row_x + i * (DIR_BTN_W + DIR_BTN_GAP),
                            row_y, DIR_BTN_W, DIR_BTN_H };
            s_dir_btn_rects[i]   = r;
            s_dir_btn_enabled[i] = dir_has_frames(atlas, dirs[i], s_mode);
            bool sel = (strcmp(s_dir, dirs[i]) == 0);
            draw_small_btn(r, labels[i], C_DIR_BTN_DEF, sel,
                           s_dir_btn_enabled[i], mx, my);
        }

        /* Mode toggle button */
        float mode_x = row_x + 4 * (DIR_BTN_W + DIR_BTN_GAP) + DIR_BTN_GAP;
        Rectangle mr = { mode_x, row_y, MODE_BTN_W, DIR_BTN_H };
        s_dir_btn_rects[4]   = mr;
        s_dir_btn_enabled[4] = true;
        bool walk_mode = (strcmp(s_mode, "walking") == 0);
        Color mode_bg  = walk_mode ? C_MODE_WALK : C_MODE_IDLE;
        draw_small_btn(mr, walk_mode ? "Walking" : "Idle",
                       mode_bg, true, true, mx, my);
    }

    float y_cursor = dir_row_y + DIR_BTN_H + 10.0f;

    /* 6. Fetch item metadata */
    const char* item_name = ols->item_id;
    const char* item_type = "";
    const char* item_desc = "";
    int st_effect = 0, st_resist = 0, st_agility = 0;
    int st_range  = 0, st_intel  = 0, st_utility  = 0;
    bool activable = true;

    if (s_ol_manager) {
        ObjectLayer* ol_data = get_or_fetch_object_layer(s_ol_manager, ols->item_id);
        if (ol_data) {
            if (ol_data->data.item.id[0] != '\0') item_name = ol_data->data.item.id;
            item_type   = ol_data->data.item.type;
            item_desc   = ol_data->data.item.description;
            activable   = ol_data->data.item.activable;
            st_effect   = ol_data->data.stats.effect;
            st_resist   = ol_data->data.stats.resistance;
            st_agility  = ol_data->data.stats.agility;
            st_range    = ol_data->data.stats.range;
            st_intel    = ol_data->data.stats.intelligence;
            st_utility  = ol_data->data.stats.utility;
        }
    }

    /* 7. Item name */
    int tfs = MODAL_FONT_TITLE;
    int ttw = MeasureText(item_name, tfs);
    DrawText(item_name, (int)(cx + (cw - ttw) * 0.5f), (int)y_cursor, tfs, C_TITLE);
    y_cursor += tfs + 4;

    /* Type badge */
    if (item_type && item_type[0] != '\0') {
        char tbuf[64];
        snprintf(tbuf, sizeof(tbuf), "[%s]", item_type);
        int tbf = MODAL_FONT_BODY - 1;
        int tbw = MeasureText(tbuf, tbf);
        DrawText(tbuf, (int)(cx + (cw - tbw) * 0.5f), (int)y_cursor, tbf,
                 (Color){ 120, 180, 255, 200 });
        y_cursor += tbf + 6;
    }

    DrawLine((int)(cx + pad), (int)y_cursor, (int)(cx + cw - pad), (int)y_cursor,
             (Color){ 70, 70, 100, 180 });
    y_cursor += 6;

    /* 8. Description with word-wrap */
    if (item_desc && item_desc[0] != '\0') {
        char desc_copy[MAX_DESCRIPTION_LENGTH];
        strncpy(desc_copy, item_desc, sizeof(desc_copy) - 1);
        desc_copy[sizeof(desc_copy) - 1] = '\0';

        int fs_d  = MODAL_FONT_BODY;
        int max_w = (int)(cw - pad * 2);
        char line_buf[256] = {0};
        char* tok = strtok(desc_copy, " ");
        while (tok) {
            char test[256];
            if (line_buf[0] == '\0')
                snprintf(test, sizeof(test), "%s", tok);
            else
                snprintf(test, sizeof(test), "%s %s", line_buf, tok);

            if (MeasureText(test, fs_d) > max_w && line_buf[0] != '\0') {
                DrawText(line_buf, (int)(cx + pad), (int)y_cursor, fs_d, C_BODY);
                y_cursor += fs_d + 2;
                snprintf(line_buf, sizeof(line_buf), "%s", tok);
            } else {
                snprintf(line_buf, sizeof(line_buf), "%s", test);
            }
            tok = strtok(NULL, " ");
        }
        if (line_buf[0] != '\0') {
            DrawText(line_buf, (int)(cx + pad), (int)y_cursor, fs_d, C_BODY);
            y_cursor += fs_d + 6;
        }
    }

    /* 9. Stats grid (2-column) */
    {
        int fs_s  = MODAL_FONT_STAT;
        int col_w = (int)(cw * 0.5f) - pad;
        struct { const char* label; int val; } rows[6] = {
            { "Effect",       st_effect  },
            { "Resistance",   st_resist  },
            { "Agility",      st_agility },
            { "Range",        st_range   },
            { "Intelligence", st_intel   },
            { "Utility",      st_utility },
        };
        DrawText("Stats", (int)(cx + pad), (int)y_cursor, fs_s,
                 (Color){ 160, 180, 255, 220 });
        y_cursor += fs_s + 3;
        for (int r = 0; r < 3; r++) {
            for (int col = 0; col < 2; col++) {
                int si = r * 2 + col;
                int sx = (int)(cx + pad + col * col_w);
                int sy = (int)y_cursor;
                DrawText(rows[si].label, sx, sy, fs_s, C_STAT_LABEL);
                char vbuf[16];
                snprintf(vbuf, sizeof(vbuf), "%+d", rows[si].val);
                int vw = MeasureText(vbuf, fs_s);
                DrawText(vbuf, sx + col_w - vw - 4, sy, fs_s,
                         rows[si].val > 0 ? C_STAT_VAL : (Color){ 200, 80, 80, 220 });
            }
            y_cursor += fs_s + 4;
        }
        y_cursor += 6;
    }

    /* 10. Quantity */
    if (ols->quantity > 0) {
        char qbuf[48];
        snprintf(qbuf, sizeof(qbuf), "Quantity: %d", ols->quantity);
        DrawText(qbuf, (int)(cx + pad), (int)y_cursor, MODAL_FONT_BODY, C_QTY);
        y_cursor += MODAL_FONT_BODY + 8;
    }

    (void)y_cursor; /* remaining space above activate button */

    /* 11. Activate / Deactivate button (anchored to card bottom) */
    {
        bool currently_active = ols->active;
        const char* btn_label = currently_active ? "Deactivate" : "Activate";

        /* Determine whether the button should be enabled */
        bool btn_enabled = activable;
        if (btn_enabled && item_type[0] != '\0') {
            /* Item type must be in activeItemTypes to be activable */
            if (!game_state_is_active_item_type(item_type))
                btn_enabled = false;
            /* Active skins cannot be deactivated when requireSkin is set */
            if (currently_active && strcmp(item_type, "skin") == 0
                && g_game_state.equipment_rules.require_skin)
                btn_enabled = false;
        }

        Color btn_color = !btn_enabled ? (Color){ 50, 50, 60, 200 }
                        : currently_active ? C_BTN_DEACT : C_BTN_ACTIVATE;
        Color txt_color = !btn_enabled ? (Color){ 100, 100, 110, 160 } : C_BTN_TEXT;

        float btn_x = cx + (cw - MODAL_BTN_W) * 0.5f;
        float btn_y = card.y + card.height - MODAL_BTN_H - 14;
        DrawRectangleRec((Rectangle){ btn_x, btn_y, MODAL_BTN_W, MODAL_BTN_H }, btn_color);
        DrawRectangleLinesEx((Rectangle){ btn_x, btn_y, MODAL_BTN_W, MODAL_BTN_H },
                             1.5f, btn_enabled ? (Color){ 220, 220, 220, 120 }
                                               : (Color){ 60, 60, 70, 100 });
        int bfs = MODAL_FONT_BODY + 2;
        int btw = MeasureText(btn_label, bfs);
        DrawText(btn_label,
                 (int)(btn_x + (MODAL_BTN_W - btw) * 0.5f),
                 (int)(btn_y + (MODAL_BTN_H - bfs) * 0.5f),
                 bfs, txt_color);
    }
}

bool inventory_modal_handle_click(int mx, int my, bool clicked) {
    if (!s_open) return false;

    int screen_w = GetScreenWidth();
    int screen_h = GetScreenHeight();
    Rectangle card = card_rect(screen_w, screen_h, 1.0f);

    if (s_age < 0.15f) return true; /* block during pop-in */
    if (!clicked)       return true; /* consume motion events */

    bool inside = hit_rect(mx, my, card);
    if (!inside) { inventory_modal_close(); return true; }

    /* Close button */
    Rectangle close_r = { card.x + card.width - MODAL_CLOSE_SIZE - 6,
                           card.y + 6, MODAL_CLOSE_SIZE, MODAL_CLOSE_SIZE };
    if (hit_rect(mx, my, close_r)) { inventory_modal_close(); return true; }

    /* Direction buttons (0..3) */
    const char* dirs[4] = { "up", "down", "left", "right" };
    for (int i = 0; i < 4; i++) {
        if (s_dir_btn_enabled[i] && hit_rect(mx, my, s_dir_btn_rects[i])) {
            strncpy(s_dir, dirs[i], sizeof(s_dir) - 1);
            rebuild_dir_str();
            return true;
        }
    }

    /* Mode toggle button (index 4) */
    if (s_dir_btn_enabled[4] && hit_rect(mx, my, s_dir_btn_rects[4])) {
        if (strcmp(s_mode, "idle") == 0)
            strncpy(s_mode, "walking", sizeof(s_mode) - 1);
        else
            strncpy(s_mode, "idle",    sizeof(s_mode) - 1);
        rebuild_dir_str();
        return true;
    }

    /* Activate / Deactivate */
    if (s_inv_idx >= 0 && s_inv_idx < g_game_state.full_inventory_count) {
        const ObjectLayerState* ols = &g_game_state.full_inventory[s_inv_idx];
        bool activable = true;
        const char* item_type = "";
        if (s_ol_manager) {
            ObjectLayer* ol_data = get_or_fetch_object_layer(s_ol_manager, ols->item_id);
            if (ol_data) {
                activable = ol_data->data.item.activable;
                item_type = ol_data->data.item.type;
            }
        }
        bool btn_enabled = activable;
        if (btn_enabled && item_type[0] != '\0') {
            if (!game_state_is_active_item_type(item_type))
                btn_enabled = false;
            if (ols->active && strcmp(item_type, "skin") == 0
                && g_game_state.equipment_rules.require_skin)
                btn_enabled = false;
        }
        if (btn_enabled) {
            float btn_x = card.x + (card.width - MODAL_BTN_W) * 0.5f;
            float btn_y = card.y + card.height - MODAL_BTN_H - 14;
            Rectangle btn_r = { btn_x, btn_y, MODAL_BTN_W, MODAL_BTN_H };
            if (hit_rect(mx, my, btn_r)) {
                send_activation(ols->item_id, !ols->active);
                inventory_modal_close();
                return true;
            }
        }
    }

    return true;
}
