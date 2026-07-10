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
#include "text.h"

#include "domain/viewport.h"
#include "fx_inventory_bar_qty.h"
#include "game_state.h"
#include "item_slot.h"
#include "object_layers_management.h"
#include "ol_as_animated_ico.h"
#include "ui_button.h"
#include "ui_toggle.h"

#include <assert.h>
#include <math.h>
#include <raylib.h>
#include <stdio.h>
#include <string.h>

/* ── Module state ─────────────────────────────────────────────────────── */

static ObjectLayersManager* s_ol_manager = NULL;

static int   s_scroll_offset = 0;
static float s_scroll_anim   = 0.0f;
static UIToggle s_bar_toggle;
static bool     s_bar_toggle_init = false;

/* ── Internal colours ──────────────────────────────────────────────────── */

static const Color C_BAR_BG        = {  10,  10,  20, INV_BAR_ALPHA };
static const Color C_COIN_BORDER   = { 230, 190,  60, 200 };  /* gold border for coin slot */
static const Color C_COIN_QTY_TEXT = { 255, 215,   0, 255 };
static const Color C_SCROLL_ARROW  = { 180, 180, 200, 220 };

/* ── Internal helpers ─────────────────────────────────────────────────── */

static int bar_height(void)       { return viewport_is_mobile() ? 60 : INV_BAR_HEIGHT; }
static int bar_slot_size(void)    { return viewport_is_mobile() ? 50 : INV_SLOT_SIZE; }
static int bar_slot_gap(void)     { return viewport_is_mobile() ? 4 : INV_SLOT_GAP; }
static int bar_slot_padding(void) { return viewport_is_mobile() ? 3 : INV_SLOT_PADDING; }
static int bar_qty_font(void)     { return viewport_is_mobile() ? 9 : INV_QTY_FONT_SIZE; }
static int bar_toggle_size(void)  { return viewport_is_mobile() ? 28 : 32; }
static int bar_toggle_pad(void)   { return viewport_is_mobile() ? 5 : 6; }

static float bar_top(int screen_h) {
    return (float)screen_h - bar_height() * s_bar_toggle.anim_t;
}

static Rectangle bar_toggle_anchor(int screen_h) {
    float size = (float)bar_toggle_size();
    float pad = (float)bar_toggle_pad();
    float x = pad;
    float y = bar_top(screen_h) - size - pad;
    float max_y = (float)screen_h - size;
    if (y < 0.0f) y = 0.0f;
    if (y > max_y) y = max_y;
    return (Rectangle){ x, y, size, size };
}

static void ensure_bar_toggle(void) {
    int screen_h = GetScreenHeight();
    if (!s_bar_toggle_init) {
        ui_toggle_init(&s_bar_toggle, (Rectangle){ 0 }, true, UI_TOGGLE_CHEVRON_DOWN);
        s_bar_toggle_init = true;
    }
    ui_toggle_set_anchor(&s_bar_toggle, bar_toggle_anchor(screen_h));
}

static bool bar_slots_settled(void) {
    return s_bar_toggle.expanded && s_bar_toggle.anim_t >= 0.98f;
}

/* coin_item_key returns a representative coin item ID from entity_defaults
 * (the world-drop pickup bot's default skin), or NULL. This is ONLY a
 * fallback icon for the pinned slot before any coin entry has arrived in
 * full_inventory (e.g. a fresh session with a zero balance) — it must never
 * be used for matching, see is_coin_item_id(). */
static const char* coin_item_key(void) {
    const EntityTypeDefault* def = game_state_get_entity_default("coin");
    if (def && def->live_item_id_count > 0) return def->live_item_ids[0];
    return NULL;
}

/* is_coin_item_id classifies an inventory entry as currency via the item's
 * OWN type metadata (ObjectLayer.data.item.type == "coin") — the same
 * authoritative field layer_z_order.c uses to tell "skin" from "weapon".
 * This is deliberately independent of entity_defaults: the "coin" entity
 * type there describes the default sprite for world coin-pickup bots, not
 * the currency item's classification, so matching against it (as this used
 * to) missed real coin entries whenever their id wasn't also the pickup
 * bot's first registered skin — letting the coin fall through into the
 * scrollable list as an ordinary item. */
static bool is_coin_item_id(const char* item_id) {
    if (!item_id || item_id[0] == '\0') return false;
    ObjectLayer* ol = lookup_cached_layer(item_id);
    return ol && 0 == strcmp(ol->data.item.type, "coin");
}

/* find_coin_slot returns the index of the coin slot in full_inventory, or -1. */
static int find_coin_slot(void) {
    for (int i = 0; i < g_game_state.full_inventory_count; i++) {
        if (is_coin_item_id(g_game_state.full_inventory[i].item_id)) return i;
    }
    return -1;
}

/* build_scroll_map fills `map` (vis index → full_inventory index) with the
 * scrollable slots: everything except the pinned coin slot and — unless
 * `include_hidden` — first-copy slots still awaiting their pickup flight.
 * Delivery targeting passes include_hidden=true so the flight aims at the
 * cell where the slot will reveal. Returns the slot count. */
static int build_scroll_map(int coin_idx, int map[MAX_OBJECT_LAYERS], bool include_hidden) {
    int n = 0;
    for (int i = 0; i < g_game_state.full_inventory_count && n < MAX_OBJECT_LAYERS; i++) {
        if (i == coin_idx) continue;
        if (!include_hidden &&
            !fx_inventory_bar_qty_slot_visible(g_game_state.full_inventory[i].item_id)) {
            continue;
        }
        map[n++] = i;
    }
    return n;
}

/* scale_rect inflates `r` around its centre — the slot pulse transform. */
static Rectangle scale_rect(Rectangle r, float s) {
    if (1.0f == s) return r;
    float w = r.width * s;
    float h = r.height * s;
    return (Rectangle){ r.x - (w - r.width) * 0.5f, r.y - (h - r.height) * 0.5f, w, h };
}

/* visible_slot_count returns how many scrollable-slots fit in the usable width.
 * The coin slot and both arrows each occupy one slot-width on the right/sides. */
static int visible_slot_count(int screen_w) {
    int arrow_w  = bar_slot_size() + bar_slot_gap();
    int coin_w   = bar_slot_size() + bar_slot_gap();
    int usable   = screen_w - arrow_w * 2 - coin_w;
    int per_slot = bar_slot_size() + bar_slot_gap();
    int n = usable / per_slot;
    return (n < 1) ? 1 : n;
}

/* slot_rect returns the screen rect for the vis_idx-th scrollable slot. */
static Rectangle slot_rect(int vis_idx, float bar_top) {
    float arrow_w  = (float)(bar_slot_size() + bar_slot_gap());
    float per_slot = (float)(bar_slot_size() + bar_slot_gap());
    float slot_top = bar_top + (bar_height() - bar_slot_size()) * 0.5f;

    float x = arrow_w + (float)vis_idx * per_slot
            - (s_scroll_anim - (float)s_scroll_offset) * per_slot;

    return (Rectangle){ x, slot_top, (float)bar_slot_size(), (float)bar_slot_size() };
}

/* coin_slot_rect returns the rect for the pinned coin slot (right of scrollable area). */
static Rectangle coin_slot_rect(int screen_w, float bar_top) {
    float arrow_w  = (float)(bar_slot_size() + bar_slot_gap());
    float slot_top = bar_top + (bar_height() - bar_slot_size()) * 0.5f;
    float cx       = (float)(screen_w - arrow_w - bar_slot_size() - bar_slot_gap() + bar_slot_gap() / 2);
    return (Rectangle){ cx, slot_top, (float)bar_slot_size(), (float)bar_slot_size() };
}

/* draw_coin_slot renders the pinned right coin slot. `coin_idx` is the
 * matched full_inventory entry (from find_coin_slot()), so the sprite
 * reflects the player's actual coin skin variant when one is owned. */
static void draw_coin_slot(Rectangle r, int coin_idx, ObjectLayersManager* mgr) {
    /* Gold-tinted background to distinguish from normal slots */
    DrawRectangleRec(r, (Color){ 35, 28, 10, 210 });

    /* Gold border — always visible, slightly thicker */
    DrawRectangleLinesEx(r, 2.0f, C_COIN_BORDER);

    /* Coin sprite (animated, down_idle) */
    const char* ck = (coin_idx >= 0) ? g_game_state.full_inventory[coin_idx].item_id : coin_item_key();
    if (ck && ck[0] != '\0') {
        int inner = bar_slot_size() - bar_slot_padding() * 2;
        ol_as_ico_draw(mgr, ck,
                       (int)(r.x + bar_slot_padding()),
                       (int)(r.y + bar_slot_padding()),
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

    /* Match the scrollable item slots' badge font size. */
    int fs = (int)(bar_slot_size() * 0.26f);
    if (fs < 13) fs = 13;
    int tw = MeasureText(buf, fs);
    int bx = (int)(r.x + r.width - tw - 3);
    int by = (int)(r.y + r.height - fs - 2);
    DrawRectangle(bx - 1, by - 1, tw + 2, fs + 2, (Color){0, 0, 0, 200});
    DrawText(buf, bx, by, fs, C_COIN_QTY_TEXT);

    /* Small "lock" — coins are non-activable */
    int lfs = bar_qty_font() - 1;
    DrawRectangle((int)r.x + 2, (int)r.y + 2, lfs + 2, lfs + 2, (Color){0, 0, 0, 140});
    DrawText("-", (int)r.x + 3, (int)r.y + 2, lfs, (Color){255, 165, 0, 200});
}

/* draw_arrow draws a scroll arrow button. */
static void draw_arrow(int x, int y, int w, int h, bool left, bool enabled) {
    UIButtonStyle style = {
        .text          = left ? "<" : ">",
        .font_size     = viewport_is_mobile() ? 16 : 18,
        .bg            = { 20, 20, 35, 180 },
        .bg_disabled   = { 20, 20, 35, 180 },
        .text_color    = C_SCROLL_ARROW,
        .text_disabled = { 60, 60, 80, 100 },
    };
    ui_button_draw((Rectangle){ (float)x, (float)y, (float)w, (float)h }, &style,
                   ui_button_resolve_state(enabled, false, false));
}

/* ── Public API ─────────────────────────────────────────────────────────── */

void inventory_bar_init(ObjectLayersManager* ol_manager) {
    s_ol_manager    = ol_manager;
    s_scroll_offset = 0;
    s_scroll_anim   = 0.0f;
    s_bar_toggle_init = false;
    ensure_bar_toggle();
    fx_inventory_bar_qty_init();
}

void inventory_bar_update(float dt) {
    ensure_bar_toggle();
    ui_toggle_update(&s_bar_toggle, dt);
    ui_toggle_set_anchor(&s_bar_toggle,
                         bar_toggle_anchor(GetScreenHeight()));
    fx_inventory_bar_qty_update(dt);

    float target = (float)s_scroll_offset;
    float diff   = target - s_scroll_anim;
    if (fabsf(diff) < 0.01f) {
        s_scroll_anim = target;
    } else {
        s_scroll_anim += diff * (1.0f - powf(0.2f, dt * 10.0f));
    }
}

float inventory_bar_visible_height(void) {
    ensure_bar_toggle();
    return bar_height() * s_bar_toggle.anim_t;
}

float inventory_bar_full_height(void) {
    return bar_height();
}

Rectangle inventory_bar_toggle_bounds(void) {
    ensure_bar_toggle();
    return s_bar_toggle.anchor;
}

bool inventory_bar_handle_toggle_click(int mx, int my) {
    ensure_bar_toggle();
    return ui_toggle_handle_click(&s_bar_toggle, mx, my);
}

void inventory_bar_draw(void) {
    ensure_bar_toggle();
    int screen_w = GetScreenWidth();
    int screen_h = GetScreenHeight();
    float current_bar_top = bar_top(screen_h);

    if (s_bar_toggle.anim_t > 0.0f) {
        DrawRectangle(0, (int)current_bar_top, screen_w, bar_height(), C_BAR_BG);
        DrawLine(0, (int)current_bar_top, screen_w, (int)current_bar_top,
                 (Color){80, 80, 120, 160});

        int coin_idx = find_coin_slot();
        int vis      = visible_slot_count(screen_w);
        int arrow_w  = bar_slot_size() + bar_slot_gap();
        int scroll_map[MAX_OBJECT_LAYERS];
        int scroll_count = build_scroll_map(coin_idx, scroll_map, false);

        draw_arrow(0, (int)current_bar_top, arrow_w, bar_height(), true,
                   s_scroll_offset > 0);
        draw_arrow(screen_w - arrow_w, (int)current_bar_top, arrow_w, bar_height(), false,
                   s_scroll_offset + vis < scroll_count);

        for (int vi = 0; vi < vis; vi++) {
            int si = s_scroll_offset + vi;
            if (si >= scroll_count) break;

            int inv_idx = scroll_map[si];
            Rectangle r = slot_rect(vi, current_bar_top);
            float coin_left = coin_slot_rect(screen_w, current_bar_top).x;
            if (r.x + r.width <= (float)arrow_w) continue;
            if (r.x >= coin_left)                continue;

            ObjectLayerState ol = g_game_state.full_inventory[inv_idx];
            ol.quantity = fx_inventory_bar_qty_display(ol.item_id, ol.quantity);
            item_slot_draw(scale_rect(r, fx_inventory_bar_qty_slot_scale(ol.item_id)), &ol, s_ol_manager);
            if (bar_slots_settled())
                fx_inventory_bar_qty_draw(r, g_game_state.full_inventory[inv_idx].item_id);
        }

        Rectangle cr = coin_slot_rect(screen_w, current_bar_top);
        const char* coin_key = (coin_idx >= 0) ? g_game_state.full_inventory[coin_idx].item_id : coin_item_key();
        draw_coin_slot(scale_rect(cr, fx_inventory_bar_qty_slot_scale(coin_key)), coin_idx, s_ol_manager);
        if (bar_slots_settled()) fx_inventory_bar_qty_draw(cr, coin_key);

        if (scroll_count > vis) {
            int dot_count = scroll_count > 40 ? 40 : scroll_count;
            int dot_r     = 3;
            int dot_gap   = 4;
            int total_w   = dot_count * (dot_r * 2 + dot_gap) - dot_gap;
            int dot_x     = (screen_w / 2 - total_w / 2);
            int dot_y     = (int)current_bar_top + 4;
            for (int i = 0; i < dot_count; i++) {
                bool on = (i >= s_scroll_offset && i < s_scroll_offset + vis);
                Color dc = on ? (Color){180, 200, 255, 220} : (Color){60, 60, 90, 140};
                DrawCircle(dot_x + i * (dot_r * 2 + dot_gap) + dot_r, dot_y, (float)dot_r, dc);
            }
        }
    }

    if (!bar_slots_settled()) fx_inventory_bar_qty_draw_bottom(s_bar_toggle.anchor);
    ui_toggle_draw(&s_bar_toggle);
}

bool inventory_bar_handle_click(int mx, int my, int* out_slot) {
    ensure_bar_toggle();
    if (out_slot) *out_slot = -1;
    if (inventory_bar_handle_toggle_click(mx, my)) return true;

    float current_bar_top = bar_top(GetScreenHeight());
    if ((float)my < current_bar_top || my >= GetScreenHeight()) return false;
    if (!s_bar_toggle.expanded) return s_bar_toggle.anim_t > 0.0f;

    int slot = inventory_bar_get_tapped_slot(mx, my);
    if (out_slot) *out_slot = slot;
    return true;
}

bool inventory_bar_point_covered(int mx, int my) {
    ensure_bar_toggle();
    if (ui_button_hit(s_bar_toggle.anchor, mx, my)) return true;
    if (s_bar_toggle.anim_t <= 0.0f) return false;
    return (float)my >= bar_top(GetScreenHeight()) && my < GetScreenHeight();
}

int inventory_bar_get_tapped_slot(int mx, int my) {
    ensure_bar_toggle();
    if (!s_bar_toggle.expanded) return -1;
    int screen_w = GetScreenWidth();
    int screen_h = GetScreenHeight();
    float current_bar_top = bar_top(screen_h);
    int arrow_w  = bar_slot_size() + bar_slot_gap();

    if ((float)my < current_bar_top || my > screen_h) return -1;

    int coin_idx = find_coin_slot();
    int scroll_map[MAX_OBJECT_LAYERS];
    int scroll_count = build_scroll_map(coin_idx, scroll_map, false);

    int vis = visible_slot_count(screen_w);

    /* Left arrow: scroll back */
    if (mx < arrow_w) {
        if (s_scroll_offset > 0) inventory_bar_scroll(-1);
        return -1;
    }

    /* Coin slot: return coin idx so modal can show it */
    Rectangle cr = coin_slot_rect(screen_w, current_bar_top);
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
        Rectangle r = slot_rect(vi, current_bar_top);
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

bool inventory_bar_item_slot_center(const char* item_id, Vector2* out) {
    if (!out || !item_id || item_id[0] == '\0') return false;

    ensure_bar_toggle();
    if (!bar_slots_settled()) {
        *out = (Vector2){ s_bar_toggle.anchor.x + s_bar_toggle.anchor.width * 0.5f,
                          s_bar_toggle.anchor.y + s_bar_toggle.anchor.height * 0.5f };
        return true;
    }

    int screen_w = GetScreenWidth();
    int screen_h = GetScreenHeight();
    float current_bar_top = bar_top(screen_h);
    int n_inv    = g_game_state.full_inventory_count;
    int coin_idx = find_coin_slot();

    int inv_idx = -1;
    for (int i = 0; i < n_inv; i++) {
        if (0 == strcmp(g_game_state.full_inventory[i].item_id, item_id)) { inv_idx = i; break; }
    }

    /* Coins live in the pinned right slot. */
    if (inv_idx >= 0 && inv_idx == coin_idx) {
        Rectangle r = coin_slot_rect(screen_w, current_bar_top);
        *out = (Vector2){ r.x + r.width * 0.5f, r.y + r.height * 0.5f };
        return true;
    }

    /* Scrollable slot — clamp to the nearest visible column when scrolled off.
     * Hidden first-copy slots are included: the flight aims at the cell where
     * the slot will reveal. */
    if (inv_idx >= 0) {
        int scroll_map[MAX_OBJECT_LAYERS];
        int scroll_count = build_scroll_map(coin_idx, scroll_map, true);
        int si = -1;
        for (int k = 0; k < scroll_count; k++) {
            if (scroll_map[k] == inv_idx) { si = k; break; }
        }
        if (si >= 0) {
            int vis = visible_slot_count(screen_w);
            int vi  = si - s_scroll_offset;
            if (vi < 0)        vi = 0;
            if (vi > vis - 1)  vi = vis - 1;
            Rectangle r = slot_rect(vi, current_bar_top);
            *out = (Vector2){ r.x + r.width * 0.5f, r.y + r.height * 0.5f };
            return true;
        }
    }

    /* Unknown item — aim at the bar center. */
    *out = (Vector2){ screen_w * 0.5f, current_bar_top + bar_height() * 0.5f };
    return true;
}
