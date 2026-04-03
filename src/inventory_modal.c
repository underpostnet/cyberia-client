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
 *   - All animations (fade-in, pop) are cosmetic and driven by age timers.
 */

#include "inventory_modal.h"
#include "inventory_bar.h"
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
static bool s_open          = false;
static int  s_inv_idx       = -1;    /* index into g_game_state.full_inventory */
static float s_age          = 0.0f;  /* seconds since open (for animations)   */

/* ── Layout constants ───────────────────────────────────────────────────── */

#define MODAL_CARD_W      380     /* width of the central card in pixels  */
#define MODAL_CARD_H      520     /* height of the central card           */
#define MODAL_SPRITE_SIZE 128     /* rendered item preview size (pixels)  */
#define MODAL_FONT_TITLE  18
#define MODAL_FONT_BODY   13
#define MODAL_FONT_STAT   12
#define MODAL_BTN_W       140
#define MODAL_BTN_H        42
#define MODAL_CLOSE_SIZE   32

/* ── Internal colours ────────────────────────────────────────────────── */

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

/* ── Helpers ─────────────────────────────────────────────────────────── */

/* card_rect returns the central modal card rectangle. */
static Rectangle card_rect(int screen_w, int screen_h, float scale) {
    float cx = screen_w * 0.5f;
    float cy = screen_h * 0.5f;
    float w  = MODAL_CARD_W * scale;
    float h  = MODAL_CARD_H * scale;
    return (Rectangle){ cx - w * 0.5f, cy - h * 0.5f, w, h };
}

/* send_activation sends a JSON item_activation intent to the server. */
static void send_activation(const char* item_id, bool active) {
    char buf[256];
    snprintf(buf, sizeof(buf),
        "{\"type\":\"item_activation\","
        "\"payload\":{\"itemId\":\"%s\",\"active\":%s}}",
        item_id, active ? "true" : "false");
    client_send(buf);
}

/* draw_btn draws a simple button and returns true if it was just clicked. */
static bool draw_btn(Rectangle r, const char* label, Color bg, bool clicked,
                     int mx, int my) {
    bool hovered = ((float)mx >= r.x && (float)mx < r.x + r.width &&
                    (float)my >= r.y && (float)my < r.y + r.height);
    Color bg_draw = hovered ? (Color){ (uint8_t)((int)bg.r + 20),
                                       (uint8_t)((int)bg.g + 20),
                                       (uint8_t)((int)bg.b + 20), bg.a } : bg;
    DrawRectangleRec(r, bg_draw);
    DrawRectangleLinesEx(r, 1.5f, (Color){220, 220, 220, 100});
    int fs = MODAL_FONT_BODY + 1;
    int tw = MeasureText(label, fs);
    DrawText(label, (int)(r.x + (r.width - tw) * 0.5f),
             (int)(r.y + (r.height - fs) * 0.5f), fs, C_BTN_TEXT);
    return hovered && clicked;
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
}

void inventory_modal_close(void) {
    s_open    = false;
    s_inv_idx = -1;
}

bool inventory_modal_is_open(void) {
    return s_open;
}

void inventory_modal_update(float dt) {
    if (!s_open) return;
    s_age += dt;
}

void inventory_modal_draw(void) {
    if (!s_open || s_inv_idx < 0) return;
    if (s_inv_idx >= g_game_state.full_inventory_count) {
        s_open = false;
        return;
    }

    int screen_w = GetScreenWidth();
    int screen_h = GetScreenHeight();
    const ObjectLayerState* ols = &g_game_state.full_inventory[s_inv_idx];

    /* ── Pop-in animation (first 0.15 s = card grows from 80% → 100%) ── */
    float pop_dur = 0.15f;
    float scale;
    if (s_age < pop_dur) {
        float t = s_age / pop_dur;
        /* Ease-out cubic */
        scale = 0.80f + 0.20f * (1.0f - (1.0f - t) * (1.0f - t) * (1.0f - t));
    } else {
        scale = 1.0f;
    }

    /* Fade-in overlay alpha */
    float fade = s_age / 0.20f;
    if (fade > 1.0f) fade = 1.0f;
    unsigned char overlay_a = (unsigned char)(C_OVERLAY_BG.a * fade);

    /* ── 1. Semi-transparent backdrop ──────────────────────────────── */
    DrawRectangle(0, 0, screen_w, screen_h,
                  (Color){ C_OVERLAY_BG.r, C_OVERLAY_BG.g, C_OVERLAY_BG.b, overlay_a });

    /* ── 2. Card ──────────────────────────────────────────────────── */
    Rectangle card = card_rect(screen_w, screen_h, scale);
    DrawRectangleRec(card, C_CARD_BG);
    DrawRectangleLinesEx(card, 2.0f, C_CARD_BORDER);

    float cx  = card.x;
    float cy  = card.y;
    float cw  = card.width;
    int   pad = 16;

    /* ── 3. Close button (top-right of card) ─────────────────────── */
    Rectangle close_r = { cx + cw - MODAL_CLOSE_SIZE - 8, cy + 8,
                           MODAL_CLOSE_SIZE, MODAL_CLOSE_SIZE };
    DrawRectangleRec(close_r, C_CLOSE_BTN);
    DrawRectangleLinesEx(close_r, 1.0f, C_CARD_BORDER);
    int xfs = MODAL_FONT_TITLE;
    int xtw = MeasureText("X", xfs);
    DrawText("X", (int)(close_r.x + (close_r.width - xtw) * 0.5f),
             (int)(close_r.y + (close_r.height - xfs) * 0.5f), xfs, C_CLOSE_X);

    /* ── 4. Item sprite ──────────────────────────────────────────── */
    float sprite_x = cx + cw * 0.5f - MODAL_SPRITE_SIZE * 0.5f;
    float sprite_y = cy + 16;
    Rectangle sprite_dst = { sprite_x, sprite_y,
                              MODAL_SPRITE_SIZE, MODAL_SPRITE_SIZE };

    if (s_ol_manager && ols->item_id[0] != '\0') {
        AtlasSpriteSheetData* atlas = get_or_fetch_atlas_data(s_ol_manager, ols->item_id);
        if (atlas) {
            Texture2D tex = get_atlas_texture(s_ol_manager, atlas->file_id);
            if (tex.id > 0) {
                const DirectionFrameData* frames = &atlas->down_idle;
                if (frames->count == 0) frames = &atlas->default_idle;
                if (frames->count > 0) {
                    const FrameMetadata* fm = &frames->frames[0];
                    Rectangle src = { (float)fm->x, (float)fm->y,
                                      (float)fm->width, (float)fm->height };
                    DrawTexturePro(tex, src, sprite_dst,
                                   (Vector2){0, 0}, 0.0f, WHITE);
                }
            }
        }
    }
    /* Sprite placeholder frame */
    DrawRectangleLinesEx(sprite_dst, 1.0f, C_CARD_BORDER);

    float y_cursor = cy + 16 + MODAL_SPRITE_SIZE + 10;

    /* ── 5. Item name + type ─────────────────────────────────────── */
    const char* item_name = ols->item_id; /* fallback: show item_id key */
    const char* item_type = "";
    const char* item_desc = "";
    int stats_effect = 0, stats_resist = 0, stats_agility = 0;
    int stats_range  = 0, stats_intel  = 0, stats_utility = 0;
    bool activable = true;

    if (s_ol_manager) {
        ObjectLayer* ol_data = get_or_fetch_object_layer(s_ol_manager, ols->item_id);
        if (ol_data) {
            if (ol_data->data.item.id[0] != '\0') item_name = ol_data->data.item.id;
            item_type    = ol_data->data.item.type;
            item_desc    = ol_data->data.item.description;
            activable    = ol_data->data.item.activable;
            stats_effect  = ol_data->data.stats.effect;
            stats_resist  = ol_data->data.stats.resistance;
            stats_agility = ol_data->data.stats.agility;
            stats_range   = ol_data->data.stats.range;
            stats_intel   = ol_data->data.stats.intelligence;
            stats_utility = ol_data->data.stats.utility;
        }
    }

    /* Title */
    int tfs = MODAL_FONT_TITLE;
    int ttw = MeasureText(item_name, tfs);
    DrawText(item_name, (int)(cx + (cw - ttw) * 0.5f), (int)y_cursor, tfs, C_TITLE);
    y_cursor += tfs + 4;

    /* Type badge */
    if (item_type && item_type[0] != '\0') {
        char type_buf[64];
        snprintf(type_buf, sizeof(type_buf), "[%s]", item_type);
        int tbf = MODAL_FONT_BODY - 1;
        int tbw = MeasureText(type_buf, tbf);
        DrawText(type_buf, (int)(cx + (cw - tbw) * 0.5f), (int)y_cursor,
                 tbf, (Color){120, 180, 255, 200});
        y_cursor += tbf + 8;
    }

    /* Separator */
    DrawLine((int)(cx + pad), (int)y_cursor, (int)(cx + cw - pad), (int)y_cursor,
             (Color){70, 70, 100, 180});
    y_cursor += 8;

    /* Description  */
    if (item_desc && item_desc[0] != '\0') {
        /* Simple word-wrap: split at spaces, add newline when overflows. */
        char desc_copy[MAX_DESCRIPTION_LENGTH];
        strncpy(desc_copy, item_desc, sizeof(desc_copy) - 1);
        desc_copy[sizeof(desc_copy) - 1] = '\0';

        int fs_d = MODAL_FONT_BODY;
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
                y_cursor += fs_d + 3;
                snprintf(line_buf, sizeof(line_buf), "%s", tok);
            } else {
                snprintf(line_buf, sizeof(line_buf), "%s", test);
            }
            tok = strtok(NULL, " ");
        }
        if (line_buf[0] != '\0') {
            DrawText(line_buf, (int)(cx + pad), (int)y_cursor, fs_d, C_BODY);
            y_cursor += fs_d + 8;
        }
    }

    /* ── 6. Stats grid ───────────────────────────────────────────── */
    {
        int fs_s  = MODAL_FONT_STAT;
        int col_w = (int)(cw * 0.5f) - pad;
        struct { const char* label; int val; } stat_rows[6] = {
            { "Effect",       stats_effect  },
            { "Resistance",   stats_resist  },
            { "Agility",      stats_agility },
            { "Range",        stats_range   },
            { "Intelligence", stats_intel   },
            { "Utility",      stats_utility },
        };
        DrawText("Stats", (int)(cx + pad), (int)y_cursor, fs_s,
                 (Color){160, 180, 255, 220});
        y_cursor += fs_s + 4;
        for (int r = 0; r < 3; r++) {
            for (int col = 0; col < 2; col++) {
                int si   = r * 2 + col;
                int sx   = (int)(cx + pad + col * col_w);
                int sy   = (int)y_cursor;
                DrawText(stat_rows[si].label, sx, sy, fs_s, C_STAT_LABEL);
                char val_buf[16];
                snprintf(val_buf, sizeof(val_buf), "%+d", stat_rows[si].val);
                int vw = MeasureText(val_buf, fs_s);
                DrawText(val_buf, sx + col_w - vw - 4, sy, fs_s,
                         stat_rows[si].val > 0 ? C_STAT_VAL
                                               : (Color){200, 80, 80, 220});
            }
            y_cursor += fs_s + 5;
        }
        y_cursor += 8;
    }

    /* ── 7. Quantity (if stackable / qty > 0) ────────────────────── */
    if (ols->quantity > 0) {
        char qty_buf[48];
        snprintf(qty_buf, sizeof(qty_buf), "Quantity: %d", ols->quantity);
        int fs_q = MODAL_FONT_BODY;
        DrawText(qty_buf, (int)(cx + pad), (int)y_cursor, fs_q, C_QTY);
        y_cursor += fs_q + 10;
    }

    /* ── 8. Activate / Deactivate button ─────────────────────────── */
    if (activable) {
        bool currently_active = ols->active;
        const char* btn_label = currently_active ? "Deactivate" : "Activate";
        Color btn_color       = currently_active ? C_BTN_DEACT : C_BTN_ACTIVATE;

        float btn_x = cx + (cw - MODAL_BTN_W) * 0.5f;
        float btn_y = card.y + card.height - MODAL_BTN_H - 16;
        Rectangle btn_r = { btn_x, btn_y, (float)MODAL_BTN_W, (float)MODAL_BTN_H };

        /* Note: drawing button here is cosmetic; actual click handled in
         * inventory_modal_handle_click() which stores btn_r bounds. */
        DrawRectangleRec(btn_r, btn_color);
        DrawRectangleLinesEx(btn_r, 1.5f, (Color){220, 220, 220, 120});
        int bfs = MODAL_FONT_BODY + 2;
        int btw = MeasureText(btn_label, bfs);
        DrawText(btn_label, (int)(btn_r.x + (btn_r.width - btw) * 0.5f),
                 (int)(btn_r.y + (btn_r.height - bfs) * 0.5f), bfs, C_BTN_TEXT);
    }
}

bool inventory_modal_handle_click(int mx, int my, bool clicked) {
    if (!s_open) return false;

    int screen_w = GetScreenWidth();
    int screen_h = GetScreenHeight();

    Rectangle card = card_rect(screen_w, screen_h, 1.0f);

    /* During pop-in animation, block all clicks. */
    if (s_age < 0.15f) return true;

    /* Tap outside the card → close */
    if (!clicked) {
        /* Still consume motion events over the overlay. */
        return true; /* modal is open — consume all events */
    }

    bool inside_card = ((float)mx >= card.x && (float)mx < card.x + card.width &&
                        (float)my >= card.y && (float)my < card.y + card.height);

    if (!inside_card) {
        inventory_modal_close();
        return true;
    }

    /* Close button (top-right) */
    Rectangle close_r = { card.x + card.width - MODAL_CLOSE_SIZE - 8,
                           card.y + 8,
                           MODAL_CLOSE_SIZE, MODAL_CLOSE_SIZE };
    if ((float)mx >= close_r.x && (float)mx < close_r.x + close_r.width &&
        (float)my >= close_r.y && (float)my < close_r.y + close_r.height) {
        inventory_modal_close();
        return true;
    }

    /* Activate / Deactivate button */
    if (s_inv_idx >= 0 && s_inv_idx < g_game_state.full_inventory_count) {
        const ObjectLayerState* ols = &g_game_state.full_inventory[s_inv_idx];
        bool activable = true;
        if (s_ol_manager) {
            ObjectLayer* ol_data = get_or_fetch_object_layer(s_ol_manager, ols->item_id);
            if (ol_data) activable = ol_data->data.item.activable;
        }

        if (activable) {
            float btn_x = card.x + (card.width - MODAL_BTN_W) * 0.5f;
            float btn_y = card.y + card.height - MODAL_BTN_H - 16;
            Rectangle btn_r = { btn_x, btn_y, (float)MODAL_BTN_W, (float)MODAL_BTN_H };

            if ((float)mx >= btn_r.x && (float)mx < btn_r.x + btn_r.width &&
                (float)my >= btn_r.y && (float)my < btn_r.y + btn_r.height) {
                /* Send activation intent; server handles swap + validation. */
                send_activation(ols->item_id, !ols->active);
                inventory_modal_close();
                return true;
            }
        }
    }

    return true; /* consume all events while modal is open */
}
