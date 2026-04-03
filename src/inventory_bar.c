/**
 * @file inventory_bar.c
 * @brief Horizontal inventory bottom bar implementation.
 *
 * Renders all ObjectLayers from g_game_state.full_inventory in a scrollable
 * strip at the bottom of the screen.  Active items get a colored border + glow.
 * Stackable quantities are shown as a small badge.
 *
 * State managed here:
 *   - scroll_offset : first visible full_inventory index (integer, snapped)
 *   - scroll_anim   : floating point for smooth slide animation
 *
 * This module is intentionally stateless with respect to game data — it only
 * reads g_game_state and renders.  No game-state mutations happen here.
 */

#include "inventory_bar.h"
#include "game_state.h"
#include "object_layers_management.h"
#include "texture_manager.h"
#include <raylib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

/* ── Module state ─────────────────────────────────────────────────────── */

static ObjectLayersManager* s_ol_manager = NULL;

/* Scroll state: s_scroll_offset is the integer first-visible slot index;
 * s_scroll_anim smoothly interpolates toward it each frame.             */
static int   s_scroll_offset = 0;
static float s_scroll_anim   = 0.0f;   /* current animated offset (slots) */

/* ── Internal colours ──────────────────────────────────────────────────── */

static const Color C_BAR_BG        = {  10,  10,  20, INV_BAR_ALPHA };
static const Color C_SLOT_BG       = {  25,  25,  40, 200 };
static const Color C_SLOT_BORDER   = {  70,  70, 100, 180 };
static const Color C_ACTIVE_GLOW   = { 100, 200, 255, 240 }; /* cyan glow */
static const Color C_QTY_BG        = {   0,   0,   0, 190 };
static const Color C_QTY_TEXT      = { 255, 230,  80, 255 }; /* bright gold */
static const Color C_SCROLL_ARROW  = { 180, 180, 200, 220 };

/* ── Public API ─────────────────────────────────────────────────────────── */

void inventory_bar_init(ObjectLayersManager* ol_manager) {
    s_ol_manager    = ol_manager;
    s_scroll_offset = 0;
    s_scroll_anim   = 0.0f;
}

void inventory_bar_update(float dt) {
    /* Smoothly slide animated scroll toward the integer target. */
    float target = (float)s_scroll_offset;
    float diff   = target - s_scroll_anim;
    if (fabsf(diff) < 0.01f) {
        s_scroll_anim = target;
    } else {
        /* Exponential ease-out: reaches target in ~12 frames at 60 Hz. */
        s_scroll_anim += diff * (1.0f - powf(0.2f, dt * 10.0f));
    }
}

/* ── Internal helpers ─────────────────────────────────────────────────── */

/* visible_slot_count returns how many slots fit horizontally at full size. */
static int visible_slot_count(int screen_w) {
    int usable    = screen_w - (INV_SLOT_SIZE + INV_SLOT_GAP) * 2; /* arrow lanes */
    int per_slot  = INV_SLOT_SIZE + INV_SLOT_GAP;
    int n         = usable / per_slot;
    return (n < 1) ? 1 : n;
}

/* slot_rect returns the screen rectangle for the i-th visible slot,
 * applying the smooth animated scroll offset.                            */
static Rectangle slot_rect(int vis_idx, int screen_w, int screen_h) {
    /* Content area starts after the left arrow lane. */
    float arrow_w  = (float)(INV_SLOT_SIZE + INV_SLOT_GAP);
    float per_slot = (float)(INV_SLOT_SIZE + INV_SLOT_GAP);
    float bar_top  = (float)(screen_h - INV_BAR_HEIGHT);
    float slot_top = bar_top + (INV_BAR_HEIGHT - INV_SLOT_SIZE) * 0.5f;

    float x = arrow_w + (float)vis_idx * per_slot
            - (s_scroll_anim - (float)s_scroll_offset) * per_slot;

    return (Rectangle){ x, slot_top, (float)INV_SLOT_SIZE, (float)INV_SLOT_SIZE };
}

/* draw_slot renders a single inventory slot. */
static void draw_slot(Rectangle r, const ObjectLayerState* ols,
                      bool selected, ObjectLayersManager* mgr) {
    /* Background */
    DrawRectangleRec(r, C_SLOT_BG);

    bool active = ols->active;

    /* Check activable flag from ObjectLayer metadata (may be NULL while loading). */
    bool activable = true;
    if (mgr && ols->item_id[0] != '\0') {
        ObjectLayer* ol_data = get_or_fetch_object_layer(mgr, ols->item_id);
        if (ol_data) activable = ol_data->data.item.activable;
    }

    /* Active border + glow (only for activable items) */
    if (active && activable) {
        /* Outer glow — draw a slightly larger filled rect with low alpha */
        Rectangle glow = { r.x - 4, r.y - 4, r.width + 8, r.height + 8 };
        DrawRectangleRec(glow, (Color){ C_ACTIVE_GLOW.r, C_ACTIVE_GLOW.g,
                                        C_ACTIVE_GLOW.b, 60 });
        DrawRectangleLinesEx(r, (float)INV_ACTIVE_BORDER, C_ACTIVE_GLOW);
    } else {
        DrawRectangleLinesEx(r, 1.0f, C_SLOT_BORDER);
    }

    /* Selection highlight (tapped slot) */
    if (selected) {
        DrawRectangleLinesEx(r, 2.0f, (Color){255, 255, 255, 180});
    }

    /* Item sprite — first frame of "down_idle" from atlas. */
    if (mgr && ols->item_id[0] != '\0') {
        AtlasSpriteSheetData* atlas = get_or_fetch_atlas_data(mgr, ols->item_id);
        if (atlas) {
            Texture2D tex = get_atlas_texture(mgr, atlas->file_id);
            if (tex.id > 0) {
                /* Pick the first "down_idle" frame; fall back to default_idle. */
                const DirectionFrameData* frames = &atlas->down_idle;
                if (frames->count == 0) frames = &atlas->default_idle;
                if (frames->count > 0) {
                    const FrameMetadata* fm = &frames->frames[0];
                    Rectangle src  = { (float)fm->x, (float)fm->y,
                                       (float)fm->width, (float)fm->height };
                    Rectangle dst  = { r.x + INV_SLOT_PADDING,
                                       r.y + INV_SLOT_PADDING,
                                       r.width  - INV_SLOT_PADDING * 2,
                                       r.height - INV_SLOT_PADDING * 2 };
                    /* Desaturate inactive items or non-activable items. */
                    Color tint = (active && activable) ? WHITE : (Color){180, 180, 180, 160};
                    DrawTexturePro(tex, src, dst, (Vector2){0, 0}, 0.0f, tint);
                }
            }
        }

        /* Quantity badge — bottom-right corner (only if qty > 1) */
        if (ols->quantity > 1) {
            char qty_buf[16];
            if (ols->quantity >= 1000)
                snprintf(qty_buf, sizeof(qty_buf), "%dk", ols->quantity / 1000);
            else
                snprintf(qty_buf, sizeof(qty_buf), "%d", ols->quantity);

            int fs  = INV_QTY_FONT_SIZE;
            int tw  = MeasureText(qty_buf, fs);
            int bx  = (int)(r.x + r.width - tw - 3);
            int by  = (int)(r.y + r.height - fs - 2);

            /* Badge background */
            DrawRectangle(bx - 1, by - 1, tw + 2, fs + 2, C_QTY_BG);
            DrawText(qty_buf, bx, by, fs, C_QTY_TEXT);
        }

        /* Lock indicator — top-left corner for non-activable items (e.g. coins).
         * A small orange "—" badge signals "cannot be equipped / activated". */
        if (!activable) {
            int lfs = INV_QTY_FONT_SIZE;
            int lx  = (int)(r.x + 3);
            int ly  = (int)(r.y + 3);
            DrawRectangle(lx - 1, ly - 1, lfs + 2, lfs + 2, (Color){0, 0, 0, 160});
            DrawText("-", lx + 1, ly, lfs, (Color){255, 165, 0, 220});
        }
    } else {
        /* Empty slot placeholder — dim dot in center */
        DrawCircle((int)(r.x + r.width * 0.5f), (int)(r.y + r.height * 0.5f),
                   3.0f, (Color){80, 80, 100, 120});
    }
}

/* draw_arrow draws a left (<) or right (>) navigation arrow button. */
static void draw_arrow(int x, int y, int w, int h, bool left, bool enabled) {
    Color c = enabled ? C_SCROLL_ARROW : (Color){60, 60, 80, 100};
    DrawRectangle(x, y, w, h, (Color){20, 20, 35, 180});
    const char* label = left ? "<" : ">";
    int fs = 18;
    int tw = MeasureText(label, fs);
    DrawText(label, x + (w - tw) / 2, y + (h - fs) / 2, fs, c);
}

void inventory_bar_draw(void) {
    int screen_w = GetScreenWidth();
    int screen_h = GetScreenHeight();
    int n_inv    = g_game_state.full_inventory_count;

    /* Bar background */
    DrawRectangle(0, screen_h - INV_BAR_HEIGHT, screen_w, INV_BAR_HEIGHT, C_BAR_BG);
    /* Top separator line */
    DrawLine(0, screen_h - INV_BAR_HEIGHT, screen_w, screen_h - INV_BAR_HEIGHT,
             (Color){80, 80, 120, 160});

    int vis = visible_slot_count(screen_w);
    int arrow_w = INV_SLOT_SIZE + INV_SLOT_GAP;
    int bar_top = screen_h - INV_BAR_HEIGHT;

    /* Left arrow */
    draw_arrow(0, bar_top, arrow_w, INV_BAR_HEIGHT, true,  s_scroll_offset > 0);
    /* Right arrow */
    draw_arrow(screen_w - arrow_w, bar_top, arrow_w, INV_BAR_HEIGHT, false,
               s_scroll_offset + vis < n_inv);

    /* Slots */
    for (int vi = 0; vi < vis; vi++) {
        int inv_idx = s_scroll_offset + vi;
        if (inv_idx >= n_inv) break;

        Rectangle r = slot_rect(vi, screen_w, screen_h);
        /* Clip: skip slots outside the usable content area. */
        if (r.x + r.width < (float)arrow_w) continue;
        if (r.x > (float)(screen_w - arrow_w)) continue;

        draw_slot(r, &g_game_state.full_inventory[inv_idx],
                  false /* selection handled by modal */, s_ol_manager);
    }

    /* Scroll indicator dots */
    if (n_inv > vis) {
        int dot_count = n_inv;
        int dot_r     = 3;
        int dot_gap   = 4;
        int total_w   = dot_count * (dot_r * 2 + dot_gap) - dot_gap;
        int dot_x     = (screen_w - total_w) / 2;
        int dot_y     = screen_h - 8;
        for (int i = 0; i < dot_count && i < 40; i++) {
            bool active_dot = (i >= s_scroll_offset && i < s_scroll_offset + vis);
            Color dc = active_dot ? (Color){180, 200, 255, 220}
                                  : (Color){ 60,  60,  90, 140};
            DrawCircle(dot_x + i * (dot_r * 2 + dot_gap) + dot_r,
                       dot_y, (float)dot_r, dc);
        }
    }
}

int inventory_bar_get_tapped_slot(int mx, int my) {
    int screen_w = GetScreenWidth();
    int screen_h = GetScreenHeight();
    int n_inv    = g_game_state.full_inventory_count;
    int vis      = visible_slot_count(screen_w);
    int arrow_w  = INV_SLOT_SIZE + INV_SLOT_GAP;

    /* Arrow zones */
    int bar_top  = screen_h - INV_BAR_HEIGHT;
    if (my < bar_top || my > screen_h) return -1;

    /* Left arrow */
    if (mx < arrow_w) {
        if (s_scroll_offset > 0) inventory_bar_scroll(-1);
        return -1;
    }
    /* Right arrow */
    if (mx > screen_w - arrow_w) {
        if (s_scroll_offset + vis < n_inv) inventory_bar_scroll(1);
        return -1;
    }

    /* Slot hit-test */
    for (int vi = 0; vi < vis; vi++) {
        int inv_idx = s_scroll_offset + vi;
        if (inv_idx >= n_inv) break;
        Rectangle r = slot_rect(vi, screen_w, screen_h);
        if ((float)mx >= r.x && (float)mx < r.x + r.width &&
            (float)my >= r.y && (float)my < r.y + r.height) {
            return inv_idx;
        }
    }
    return -1;
}

void inventory_bar_scroll(int delta) {
    int screen_w = GetScreenWidth();
    int n_inv    = g_game_state.full_inventory_count;
    int vis      = visible_slot_count(screen_w);
    int max_off  = n_inv - vis;
    if (max_off < 0) max_off = 0;
    s_scroll_offset += delta;
    if (s_scroll_offset < 0)       s_scroll_offset = 0;
    if (s_scroll_offset > max_off) s_scroll_offset = max_off;
}
