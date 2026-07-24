/**
 * @file inventory_bar.c
 * @brief Horizontal inventory bottom bar implementation.
 *
 * Layout (left → right):
 *   [ drag-scrollable items ... ]  [coin slot]
 *
 * The strip scrolls by a continuous pixel offset: press and slide left/right
 * (finger or mouse) moves it 1:1 and releases into an inertial glide. A press
 * only activates a slot when it never became a drag, so scrolling never opens
 * a modal — activation is deferred to release via inventory_bar_take_tap().
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

static UIToggle s_bar_toggle;
static bool     s_bar_toggle_init = false;

/* Horizontal drag scroll: offset in px, plus gesture + inertia tracking. */
static float s_scroll_px  = 0.0f;
static float s_scroll_vel = 0.0f;
static bool  s_press_armed = false;      /* press landed on the bar          */
static bool  s_dragging    = false;      /* passed the slop — suppress a tap */
static bool  s_pointer_was_down = false;
static float s_press_x = 0.0f;
static float s_press_y = 0.0f;
static float s_last_x  = 0.0f;
static bool  s_tap_pending = false;      /* clean release awaiting activation */
static int   s_tap_x = 0;
static int   s_tap_y = 0;

/* Pointer travel that turns a press into a scroll instead of a tap. */
#define INV_DRAG_SLOP_PX 6.0f
/* Glide below this speed (px/s) is not worth animating. */
#define INV_MIN_GLIDE_PX 8.0f

/* ── Internal colours ──────────────────────────────────────────────────── */

static const Color C_BAR_BG        = {  10,  10,  20, INV_BAR_ALPHA };
static const Color C_COIN_BORDER   = { 230, 190,  60, 200 };  /* gold border for coin slot */
static const Color C_COIN_QTY_TEXT = { 255, 215,   0, 255 };

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

static int slot_pitch(void) { return bar_slot_size() + bar_slot_gap(); }

/* The coin slot is pinned to the right edge, outside the scroll strip. */
static float coin_slot_x(int screen_w) {
    return (float)(screen_w - bar_slot_size() - bar_slot_gap());
}

/* Scrollable strip bounds — everything left of the pinned coin slot. */
static float strip_left(void)             { return (float)bar_slot_gap(); }
static float strip_right(int screen_w)    { return coin_slot_x(screen_w) - (float)bar_slot_gap(); }

/* visible_slot_count returns how many scrollable slots fit in the strip. */
static int visible_slot_count(int screen_w) {
    int usable = (int)(strip_right(screen_w) - strip_left());
    int n = usable / slot_pitch();
    return (n < 1) ? 1 : n;
}

/* Furthest the strip may slide before its last slot reaches the right edge. */
static float max_scroll_px(int screen_w, int scroll_count) {
    float content = scroll_count > 0 ? (float)(scroll_count * slot_pitch() - bar_slot_gap()) : 0.0f;
    float max = content - (strip_right(screen_w) - strip_left());
    return max > 0.0f ? max : 0.0f;
}

/* slot_rect returns the screen rect for the si-th scrollable slot. */
static Rectangle slot_rect(int si, float bar_top) {
    float slot_top = bar_top + (bar_height() - bar_slot_size()) * 0.5f;
    float x = strip_left() + (float)si * (float)slot_pitch() - s_scroll_px;
    return (Rectangle){ x, slot_top, (float)bar_slot_size(), (float)bar_slot_size() };
}

/* coin_slot_rect returns the rect for the pinned coin slot (right of scrollable area). */
static Rectangle coin_slot_rect(int screen_w, float bar_top) {
    float slot_top = bar_top + (bar_height() - bar_slot_size()) * 0.5f;
    return (Rectangle){ coin_slot_x(screen_w), slot_top,
                        (float)bar_slot_size(), (float)bar_slot_size() };
}

/* Hold the offset inside range, killing any glide that runs into an edge. */
static void clamp_scroll(void) {
    int map[MAX_OBJECT_LAYERS];
    int count = build_scroll_map(find_coin_slot(), map, false);
    float max = max_scroll_px(GetScreenWidth(), count);
    if (s_scroll_px < 0.0f) { s_scroll_px = 0.0f; s_scroll_vel = 0.0f; }
    if (s_scroll_px > max)  { s_scroll_px = max;  s_scroll_vel = 0.0f; }
}

/* draw_coin_slot renders the pinned right coin slot. `coin_idx` is the
 * matched full_inventory entry (from find_coin_slot()), so the sprite
 * reflects the player's actual coin skin variant when one is owned. */
static void draw_coin_slot(Rectangle r, int coin_idx, ObjectLayersManager* mgr) {
    /* Pixel-retro coin slot: black outer border, gold inner fill with
     * highlight/shadow edges, gold border overlay, hover brightens fill. */
    bool hovered = CheckCollisionPointRec(GetMousePosition(), r);
    Color gold_fill = hovered ? (Color){ 55, 44, 20, 230 } : (Color){ 35, 28, 10, 210 };
    Color gold_highlight = (Color){
        (unsigned char)(gold_fill.r + (255 - gold_fill.r) * 0.45f),
        (unsigned char)(gold_fill.g + (255 - gold_fill.g) * 0.45f),
        (unsigned char)(gold_fill.b + (255 - gold_fill.b) * 0.45f),
        gold_fill.a
    };
    Color gold_shadow = (Color){
        (unsigned char)(gold_fill.r * 0.55f),
        (unsigned char)(gold_fill.g * 0.55f),
        (unsigned char)(gold_fill.b * 0.55f),
        gold_fill.a
    };

    /* Black outer border via rounded rect */
    DrawRectangleRec(r, BLACK);
    /* Inner fill */
    Rectangle inner = { r.x + 2.0f, r.y + 2.0f, r.width - 4.0f, r.height - 4.0f };
    DrawRectangleRec(inner, gold_fill);
    /* Top highlight edge */
    DrawRectangle((int)(inner.x + 4.0f), (int)inner.y, (int)(inner.width - 8.0f), 2, gold_highlight);
    /* Bottom shadow edge */
    DrawRectangle((int)(inner.x + 4.0f), (int)(inner.y + inner.height - 2.0f),
                  (int)(inner.width - 8.0f), 2, gold_shadow);
    /* Gold border overlay */
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

/* Touch drives the gesture when present; mouse otherwise. */
static Vector2 pointer_position(void) {
    if (GetTouchPointCount() > 0) return GetTouchPosition(0);
    return GetMousePosition();
}

static bool pointer_down(void) {
    return GetTouchPointCount() > 0 || IsMouseButtonDown(MOUSE_BUTTON_LEFT);
}

/* Track the press-drag gesture and the inertial glide that follows it. */
static void update_drag(float dt) {
    if (!bar_slots_settled()) {
        s_press_armed = false;
        s_dragging    = false;
        s_scroll_vel  = 0.0f;
        return;
    }

    bool down = pointer_down();
    Vector2 p = pointer_position();

    if (down && s_press_armed) {
        if (!s_dragging && fabsf(p.x - s_press_x) > INV_DRAG_SLOP_PX) s_dragging = true;
        if (s_dragging) {
            float dx = p.x - s_last_x;
            s_scroll_px -= dx;                       /* content follows the pointer 1:1 */
            if (dt > 0.0f) s_scroll_vel = -dx / dt;
        }
        s_last_x = p.x;
    }

    if (!down && s_pointer_was_down) {
        /* A press that never became a drag activates the slot under it. */
        if (s_press_armed && !s_dragging) {
            s_tap_pending = true;
            s_tap_x = (int)s_press_x;
            s_tap_y = (int)s_press_y;
        }
        s_press_armed = false;
        s_dragging    = false;
    }
    s_pointer_was_down = down;

    if (!s_dragging) {
        if (fabsf(s_scroll_vel) > INV_MIN_GLIDE_PX) {
            s_scroll_px  += s_scroll_vel * dt;
            s_scroll_vel *= powf(0.02f, dt);
        } else {
            s_scroll_vel = 0.0f;
        }
    }
    clamp_scroll();
}

/* ── Public API ─────────────────────────────────────────────────────────── */

void inventory_bar_init(ObjectLayersManager* ol_manager) {
    s_ol_manager    = ol_manager;
    s_scroll_px     = 0.0f;
    s_scroll_vel    = 0.0f;
    s_press_armed   = false;
    s_dragging      = false;
    s_pointer_was_down = false;
    s_tap_pending   = false;
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
    update_drag(dt);
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
        int scroll_map[MAX_OBJECT_LAYERS];
        int scroll_count = build_scroll_map(coin_idx, scroll_map, false);
        float left  = strip_left();
        float right = strip_right(screen_w);

        for (int si = 0; si < scroll_count; si++) {
            Rectangle r = slot_rect(si, current_bar_top);
            if (r.x + r.width <= left) continue;
            if (r.x >= right)          break;

            int inv_idx = scroll_map[si];
            ObjectLayerState ol = g_game_state.full_inventory[inv_idx];
            ol.quantity = fx_inventory_bar_qty_display(ol.item_id, ol.quantity);
            /* During the arrival pulse the sprite renders in full colour. */
            item_slot_draw_ex(scale_rect(r, fx_inventory_bar_qty_slot_scale(ol.item_id)),
                              &ol, s_ol_manager, WHITE, 0.0f,
                              fx_inventory_bar_qty_slot_pulsing(ol.item_id));
            if (bar_slots_settled())
                fx_inventory_bar_qty_draw(r, g_game_state.full_inventory[inv_idx].item_id);
        }

        Rectangle cr = coin_slot_rect(screen_w, current_bar_top);
        const char* coin_key = (coin_idx >= 0) ? g_game_state.full_inventory[coin_idx].item_id : coin_item_key();
        draw_coin_slot(scale_rect(cr, fx_inventory_bar_qty_slot_scale(coin_key)), coin_idx, s_ol_manager);
        if (bar_slots_settled()) fx_inventory_bar_qty_draw(cr, coin_key);

        if (scroll_count > vis) {
            int first     = (int)((s_scroll_px + (float)slot_pitch() * 0.5f) / (float)slot_pitch());
            int dot_count = scroll_count > 40 ? 40 : scroll_count;
            int dot_r     = 3;
            int dot_gap   = 4;
            int total_w   = dot_count * (dot_r * 2 + dot_gap) - dot_gap;
            int dot_x     = (screen_w / 2 - total_w / 2);
            int dot_y     = (int)current_bar_top + 4;
            for (int i = 0; i < dot_count; i++) {
                bool on = (i >= first && i < first + vis);
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
    /* Activation is deferred to a clean release — see inventory_bar_take_tap. */
    if (out_slot) *out_slot = -1;
    if (inventory_bar_handle_toggle_click(mx, my)) return true;

    float current_bar_top = bar_top(GetScreenHeight());
    if ((float)my < current_bar_top || my >= GetScreenHeight()) return false;
    if (!s_bar_toggle.expanded) return s_bar_toggle.anim_t > 0.0f;

    s_press_armed = true;
    s_dragging    = false;
    s_press_x     = (float)mx;
    s_press_y     = (float)my;
    s_last_x      = (float)mx;
    s_scroll_vel  = 0.0f;   /* catch a gliding strip */
    return true;
}

bool inventory_bar_take_tap(int* out_slot) {
    if (!s_tap_pending) return false;
    s_tap_pending = false;
    int slot = inventory_bar_get_tapped_slot(s_tap_x, s_tap_y);
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

    if ((float)my < current_bar_top || my > screen_h) return -1;

    int coin_idx = find_coin_slot();

    /* Coin slot: return coin idx so modal can show it */
    Rectangle cr = coin_slot_rect(screen_w, current_bar_top);
    if ((float)mx >= cr.x && (float)mx < cr.x + cr.width &&
        (float)my >= cr.y && (float)my < cr.y + cr.height) {
        return (coin_idx >= 0) ? coin_idx : -1;
    }

    int scroll_map[MAX_OBJECT_LAYERS];
    int scroll_count = build_scroll_map(coin_idx, scroll_map, false);
    float left  = strip_left();
    float right = strip_right(screen_w);

    for (int si = 0; si < scroll_count; si++) {
        Rectangle r = slot_rect(si, current_bar_top);
        if (r.x + r.width <= left) continue;
        if (r.x >= right)          break;
        if ((float)mx >= r.x && (float)mx < r.x + r.width &&
            (float)my >= r.y && (float)my < r.y + r.height) {
            return scroll_map[si];
        }
    }
    return -1;
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
            Rectangle r = slot_rect(si, current_bar_top);
            float left  = strip_left();
            float right = strip_right(screen_w);
            if (r.x < left)                 r.x = left;
            if (r.x + r.width > right)      r.x = right - r.width;
            *out = (Vector2){ r.x + r.width * 0.5f, r.y + r.height * 0.5f };
            return true;
        }
    }

    /* Unknown item — aim at the bar center. */
    *out = (Vector2){ screen_w * 0.5f, current_bar_top + bar_height() * 0.5f };
    return true;
}
