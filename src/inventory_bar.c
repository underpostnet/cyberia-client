/**
 * @file inventory_bar.c
 * @brief Horizontal inventory bottom bar implementation.
 *
 * Layout (left → right):
 *   [<]  [ scrollable items ... ]  [coin slot]  [>]
 *
 * The coin slot is always the rightmost slot, pinned outside the scrollable
 * region.  Coins cannot be activated (non-activable), so they never get the
 * active glow border — a small lock badge is shown instead.
 *
 * All item sprites are rendered as looping animated icons via ol_as_animated_ico
 * (down_idle by default) so the bar shows "live" sprites consistently with the
 * in-world entity render.
 */

#include "inventory_bar.h"
#include "ol_as_animated_ico.h"
#include "game_state.h"
#include "object_layers_management.h"
#include <raylib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <assert.h>

/* ── Module state ─────────────────────────────────────────────────────── */

static ObjectLayersManager* s_ol_manager = NULL;

static int   s_scroll_offset = 0;
static float s_scroll_anim   = 0.0f;

/* ── Internal colours ──────────────────────────────────────────────────── */

static const Color C_BAR_BG        = {  10,  10,  20, INV_BAR_ALPHA };
static const Color C_SLOT_BG       = {  25,  25,  40, 200 };
static const Color C_SLOT_BORDER   = {  70,  70, 100, 180 };
static const Color C_ACTIVE_GLOW   = { 100, 200, 255, 240 };
static const Color C_COIN_BORDER   = { 230, 190,  60, 200 };  /* gold border for coin slot */
static const Color C_QTY_BG        = {   0,   0,   0, 190 };
static const Color C_QTY_TEXT      = { 255, 230,  80, 255 };
static const Color C_COIN_QTY_TEXT = { 255, 215,   0, 255 };
static const Color C_SCROLL_ARROW  = { 180, 180, 200, 220 };

/* ── Internal helpers ─────────────────────────────────────────────────── */

/* coin_item_key returns the coin item ID from entity_defaults, or NULL. */
static const char* coin_item_key(void) {
    const EntityTypeDefault* def = game_state_get_entity_default("coin");
    if (def && def->live_item_id_count > 0) return def->live_item_ids[0];
    return NULL;
}

/* find_coin_slot returns the index of the coin slot in full_inventory, or -1. */
static int find_coin_slot(void) {
    const char* ck = coin_item_key();
    if (!ck) return -1;
    for (int i = 0; i < g_game_state.full_inventory_count; i++) {
        if (strcmp(g_game_state.full_inventory[i].item_id, ck) == 0) return i;
    }
    return -1;
}

/* visible_slot_count returns how many scrollable-slots fit in the usable width.
 * The coin slot and both arrows each occupy one slot-width on the right/sides. */
static int visible_slot_count(int screen_w) {
    int arrow_w  = INV_SLOT_SIZE + INV_SLOT_GAP;
    int coin_w   = INV_SLOT_SIZE + INV_SLOT_GAP;
    int usable   = screen_w - arrow_w * 2 - coin_w;
    int per_slot = INV_SLOT_SIZE + INV_SLOT_GAP;
    int n = usable / per_slot;
    return (n < 1) ? 1 : n;
}

/* slot_rect returns the screen rect for the vis_idx-th scrollable slot. */
static Rectangle slot_rect(int vis_idx, int screen_w, int screen_h) {
    float arrow_w  = (float)(INV_SLOT_SIZE + INV_SLOT_GAP);
    float per_slot = (float)(INV_SLOT_SIZE + INV_SLOT_GAP);
    float bar_top  = (float)(screen_h - INV_BAR_HEIGHT);
    float slot_top = bar_top + (INV_BAR_HEIGHT - INV_SLOT_SIZE) * 0.5f;

    float x = arrow_w + (float)vis_idx * per_slot
            - (s_scroll_anim - (float)s_scroll_offset) * per_slot;

    return (Rectangle){ x, slot_top, (float)INV_SLOT_SIZE, (float)INV_SLOT_SIZE };
}

/* coin_slot_rect returns the rect for the pinned coin slot (right of scrollable area). */
static Rectangle coin_slot_rect(int screen_w, int screen_h) {
    float arrow_w  = (float)(INV_SLOT_SIZE + INV_SLOT_GAP);
    float bar_top  = (float)(screen_h - INV_BAR_HEIGHT);
    float slot_top = bar_top + (INV_BAR_HEIGHT - INV_SLOT_SIZE) * 0.5f;
    float cx       = (float)(screen_w - arrow_w - INV_SLOT_SIZE - INV_SLOT_GAP + INV_SLOT_GAP / 2);
    return (Rectangle){ cx, slot_top, (float)INV_SLOT_SIZE, (float)INV_SLOT_SIZE };
}

/* draw_slot renders a single scrollable inventory slot. */
static void draw_slot(Rectangle r, const ObjectLayerState* ols, ObjectLayersManager* mgr) {
    DrawRectangleRec(r, C_SLOT_BG);

    bool active = ols->active;

    bool activable = true;
    if (mgr && ols->item_id[0] != '\0') {
        ObjectLayer* ol_data = get_or_fetch_object_layer(mgr, ols->item_id);
        if (ol_data) activable = ol_data->data.item.activable;
    }

    /* Border: always consistent 2px thickness — color varies by state */
    if (active && activable) {
        DrawRectangleLinesEx(r, 2.0f, C_ACTIVE_GLOW);
    } else if (!activable) {
        DrawRectangleLinesEx(r, 2.0f, C_SLOT_BORDER);
    } else {
        DrawRectangleLinesEx(r, 2.0f, C_SLOT_BORDER);
    }

    /* Sprite via ol_as_animated_ico */
    if (ols->item_id[0] != '\0') {
        Color tint = (active && activable) ? WHITE : (Color){180, 180, 180, 160};
        int inner  = INV_SLOT_SIZE - INV_SLOT_PADDING * 2;
        ol_as_ico_draw(mgr, ols->item_id,
                       (int)(r.x + INV_SLOT_PADDING),
                       (int)(r.y + INV_SLOT_PADDING),
                       inner, OL_ICO_DEFAULT_DIR, 0, tint);

        /* Quantity badge (bottom-right, only if > 1) */
        if (ols->quantity > 1) {
            char buf[16];
            if (ols->quantity >= 1000)
                snprintf(buf, sizeof(buf), "%dk", ols->quantity / 1000);
            else
                snprintf(buf, sizeof(buf), "%d", ols->quantity);
            int fs = INV_QTY_FONT_SIZE;
            int tw = MeasureText(buf, fs);
            int bx = (int)(r.x + r.width - tw - 3);
            int by = (int)(r.y + r.height - fs - 2);
            DrawRectangle(bx - 1, by - 1, tw + 2, fs + 2, C_QTY_BG);
            DrawText(buf, bx, by, fs, C_QTY_TEXT);
        }

        /* Lock badge (top-left) for non-activable items */
        if (!activable) {
            int lfs = INV_QTY_FONT_SIZE;
            int lx  = (int)(r.x + 3);
            int ly  = (int)(r.y + 3);
            DrawRectangle(lx - 1, ly - 1, lfs + 2, lfs + 2, (Color){0, 0, 0, 160});
            DrawText("-", lx + 1, ly, lfs, (Color){255, 165, 0, 220});
        }
    } else {
        /* Empty placeholder */
        DrawCircle((int)(r.x + r.width * 0.5f), (int)(r.y + r.height * 0.5f),
                   3.0f, (Color){80, 80, 100, 120});
    }
}

/* draw_coin_slot renders the pinned right coin slot. */
static void draw_coin_slot(Rectangle r, int coin_idx, ObjectLayersManager* mgr) {
    /* Gold-tinted background to distinguish from normal slots */
    DrawRectangleRec(r, (Color){ 35, 28, 10, 210 });

    /* Gold border — always visible, slightly thicker */
    DrawRectangleLinesEx(r, 2.0f, C_COIN_BORDER);

    /* Coin sprite (animated, down_idle) */
    const char* ck = coin_item_key();
    if (ck && ck[0] != '\0') {
        int inner = INV_SLOT_SIZE - INV_SLOT_PADDING * 2;
        ol_as_ico_draw(mgr, ck,
                       (int)(r.x + INV_SLOT_PADDING),
                       (int)(r.y + INV_SLOT_PADDING),
                       inner, OL_ICO_DEFAULT_DIR, 0, WHITE);
    }

    /* Coin balance from fast flat field */
    int balance = game_state_get_player_coins();
    char buf[24];
    if (balance >= 1000000)
        snprintf(buf, sizeof(buf), "%.1fM", balance / 1000000.0f);
    else if (balance >= 1000)
        snprintf(buf, sizeof(buf), "%.1fk", balance / 1000.0f);
    else
        snprintf(buf, sizeof(buf), "%d", balance);

    int fs = INV_QTY_FONT_SIZE;
    int tw = MeasureText(buf, fs);
    int bx = (int)(r.x + r.width - tw - 3);
    int by = (int)(r.y + r.height - fs - 2);
    DrawRectangle(bx - 1, by - 1, tw + 2, fs + 2, (Color){0, 0, 0, 200});
    DrawText(buf, bx, by, fs, C_COIN_QTY_TEXT);

    /* Small "lock" — coins are non-activable */
    int lfs = INV_QTY_FONT_SIZE - 1;
    DrawRectangle((int)r.x + 2, (int)r.y + 2, lfs + 2, lfs + 2, (Color){0, 0, 0, 140});
    DrawText("-", (int)r.x + 3, (int)r.y + 2, lfs, (Color){255, 165, 0, 200});

    (void)coin_idx; /* reserved for modal open logic */
}

/* draw_arrow draws a scroll arrow button. */
static void draw_arrow(int x, int y, int w, int h, bool left, bool enabled) {
    Color c = enabled ? C_SCROLL_ARROW : (Color){60, 60, 80, 100};
    DrawRectangle(x, y, w, h, (Color){20, 20, 35, 180});
    const char* label = left ? "<" : ">";
    int fs = 18;
    int tw = MeasureText(label, fs);
    DrawText(label, x + (w - tw) / 2, y + (h - fs) / 2, fs, c);
}

/* ── Public API ─────────────────────────────────────────────────────────── */

void inventory_bar_init(ObjectLayersManager* ol_manager) {
    s_ol_manager    = ol_manager;
    s_scroll_offset = 0;
    s_scroll_anim   = 0.0f;
}

void inventory_bar_update(float dt) {
    float target = (float)s_scroll_offset;
    float diff   = target - s_scroll_anim;
    if (fabsf(diff) < 0.01f) {
        s_scroll_anim = target;
    } else {
        s_scroll_anim += diff * (1.0f - powf(0.2f, dt * 10.0f));
    }
}

void inventory_bar_draw(void) {
    int screen_w = GetScreenWidth();
    int screen_h = GetScreenHeight();
    int n_inv    = g_game_state.full_inventory_count;

    /* Bar background */
    DrawRectangle(0, screen_h - INV_BAR_HEIGHT, screen_w, INV_BAR_HEIGHT, C_BAR_BG);
    DrawLine(0, screen_h - INV_BAR_HEIGHT, screen_w, screen_h - INV_BAR_HEIGHT,
             (Color){80, 80, 120, 160});

    int coin_idx = find_coin_slot();
    int vis      = visible_slot_count(screen_w);
    int arrow_w  = INV_SLOT_SIZE + INV_SLOT_GAP;
    int bar_top  = screen_h - INV_BAR_HEIGHT;

    /* Count scrollable slots (all except the coin slot) */
    int scroll_count = 0;
    int scroll_map[MAX_OBJECT_LAYERS]; /* maps vis index → full_inventory index */
    for (int i = 0; i < n_inv && scroll_count < MAX_OBJECT_LAYERS; i++) {
        if (i != coin_idx) scroll_map[scroll_count++] = i;
    }

    /* Left / right arrows */
    draw_arrow(0, bar_top, arrow_w, INV_BAR_HEIGHT, true,  s_scroll_offset > 0);
    draw_arrow(screen_w - arrow_w, bar_top, arrow_w, INV_BAR_HEIGHT, false,
               s_scroll_offset + vis < scroll_count);

    /* Scrollable slots */
    for (int vi = 0; vi < vis; vi++) {
        int si = s_scroll_offset + vi;
        if (si >= scroll_count) break;

        int inv_idx = scroll_map[si];
        Rectangle r = slot_rect(vi, screen_w, screen_h);
        /* Clip to scrollable content lane (between the two arrow lanes, before coin) */
        float coin_left = coin_slot_rect(screen_w, screen_h).x;
        if (r.x + r.width <= (float)arrow_w) continue;
        if (r.x >= coin_left)                continue;

        draw_slot(r, &g_game_state.full_inventory[inv_idx], s_ol_manager);
    }

    /* Pinned coin slot — right side, just inside the right arrow */
    Rectangle cr = coin_slot_rect(screen_w, screen_h);
    draw_coin_slot(cr, coin_idx, s_ol_manager);

    /* Scroll indicator dots — positioned ABOVE slot area, at the top of the bar */
    if (scroll_count > vis) {
        int dot_count = scroll_count > 40 ? 40 : scroll_count;
        int dot_r     = 3;
        int dot_gap   = 4;
        int total_w   = dot_count * (dot_r * 2 + dot_gap) - dot_gap;
        int dot_x     = (screen_w / 2 - total_w / 2);
        int dot_y     = bar_top + 4;
        for (int i = 0; i < dot_count; i++) {
            bool on = (i >= s_scroll_offset && i < s_scroll_offset + vis);
            Color dc = on ? (Color){180, 200, 255, 220} : (Color){60, 60, 90, 140};
            DrawCircle(dot_x + i * (dot_r * 2 + dot_gap) + dot_r, dot_y, (float)dot_r, dc);
        }
    }
}

int inventory_bar_get_tapped_slot(int mx, int my) {
    int screen_w = GetScreenWidth();
    int screen_h = GetScreenHeight();
    int n_inv    = g_game_state.full_inventory_count;
    int bar_top  = screen_h - INV_BAR_HEIGHT;
    int arrow_w  = INV_SLOT_SIZE + INV_SLOT_GAP;

    if (my < bar_top || my > screen_h) return -1;

    /* Build scroll map (excluding coin slot) */
    int coin_idx = find_coin_slot();
    int scroll_map[MAX_OBJECT_LAYERS];
    int scroll_count = 0;
    for (int i = 0; i < n_inv && scroll_count < MAX_OBJECT_LAYERS; i++) {
        if (i != coin_idx) scroll_map[scroll_count++] = i;
    }

    int vis = visible_slot_count(screen_w);

    /* Left arrow: scroll back */
    if (mx < arrow_w) {
        if (s_scroll_offset > 0) inventory_bar_scroll(-1);
        return -1;
    }

    /* Coin slot: return coin idx so modal can show it */
    Rectangle cr = coin_slot_rect(screen_w, screen_h);
    if ((float)mx >= cr.x && (float)mx < cr.x + cr.width &&
        (float)my >= cr.y && (float)my < cr.y + cr.height) {
        return (coin_idx >= 0) ? coin_idx : -1;
    }

    /* Right arrow: scroll forward */
    if (mx > screen_w - arrow_w) {
        if (s_scroll_offset + vis < scroll_count) inventory_bar_scroll(1);
        return -1;
    }

    /* Scrollable slot hit-test */
    for (int vi = 0; vi < vis; vi++) {
        int si = s_scroll_offset + vi;
        if (si >= scroll_count) break;
        Rectangle r = slot_rect(vi, screen_w, screen_h);
        if ((float)mx >= r.x && (float)mx < r.x + r.width &&
            (float)my >= r.y && (float)my < r.y + r.height) {
            return scroll_map[si];
        }
    }
    return -1;
}

void inventory_bar_scroll(int delta) {
    int screen_w = GetScreenWidth();
    int n_inv    = g_game_state.full_inventory_count;
    int coin_idx = find_coin_slot();
    int scroll_count = n_inv - (coin_idx >= 0 ? 1 : 0);
    int vis      = visible_slot_count(screen_w);
    int max_off  = scroll_count - vis;
    if (max_off < 0) max_off = 0;
    s_scroll_offset += delta;
    if (s_scroll_offset < 0)       s_scroll_offset = 0;
    if (s_scroll_offset > max_off) s_scroll_offset = max_off;
}
