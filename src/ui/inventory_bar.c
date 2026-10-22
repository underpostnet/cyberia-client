/**
 * @file inventory_bar.c
 * @brief Horizontal inventory bottom bar implementation.
 *
 * Layout (left → right):
 *   [ drag-scrollable bag slots ... ]  [coin slot]
 *
 * Each strip cell is a bag slot, and a stack shows in the cell of its slot, so
 * a stack that leaves leaves its cell empty and the others stay. The strip
 * shows every slot the inventory capacity can open; the slots the level has
 * not opened yet show the lock icon and the level that opens them. A tap on a
 * locked slot says so.
 *
 * A press on a stack, or on the coin slot, lifts it once the pointer travels:
 * the release drops it on a strip cell or on a cell of the open container
 * through ui/stack_move, on the own offer of an open trade (ui/trade_panel),
 * or on the ground under the pointer when no UI covers it (ui/world_drop).
 * A press that never travelled is a tap, so a drag never opens a modal —
 * activation is deferred to release via inventory_bar_take_tap(). A stack that
 * lands slides into its cell from where it came.
 *
 * When the cells overrun the strip, a scroll track runs over it. Only the
 * track and the wheel scroll the strip: a drag of the thumb pans it, and a
 * press on the track brings the thumb under the pointer.
 *
 * The coin slot is always the rightmost slot, pinned outside the scrollable
 * region.  Coins cannot be activated (non-activable), so they never get the
 * active glow border — a small lock badge is shown instead.
 *
 * All item sprites are rendered as looping animated icons via ol_as_animated_ico
 * (down_idle by default) so the bar shows "live" sprites consistently with the
 * in-world entity render.
 *
 * Above a slot whose skills summon entities, a row shows each summoned entity
 * with the units the player holds of it, and one cooldown bar: the consume
 * cooldown for a held skill, else the shared action cooldown. An inactive slot
 * shows only its held skills.
 */

#include "inventory_bar.h"
#include "input/input.h"
#include "modal_notification.h"

#include "fx/fx_shapes.h"
#include "world/object_layer.h"
#include "utils.h"
#include "view/viewport.h"
#include "fx/fx_inventory_bar_qty.h"
#include "sim/local_player.h"
#include "sim/game_state.h"
#include "sim/container_ops.h"
#include "stack_move.h"
#include "container_panel.h"
#include "trade_panel.h"
#include "ui_dispatch.h"
#include "world_drop.h"
#include "item_slot_grid.h"
#include "ease.h"
#include "item_slot.h"
#include "modal.h"
#include "world/object_layers_management.h"
#include "ol_as_animated_ico.h"
#include "ol_stack_ico.h"
#include "skill_preview.h"
#include "ui_button.h"
#include "ui_icon.h"
#include "ui_rect.h"
#include "ui_toggle.h"
#include "utils.h"

#include <assert.h>
#include <math.h>
#include <raylib.h>
#include <stdio.h>
#include <string.h>

/* ── Layout constants (screen pixels) ────────────────────────────────── */

#define INV_SLOT_SIZE       60      /* square */
#define INV_SLOT_GAP        6
#define INV_SLOT_PADDING    4       /* around the sprite, inside the slot */
#define INV_BAR_MARGIN      6       /* above and below each row of the bar */
#define INV_TRACK_H         14      /* the scroll track over the strip */
#define INV_BAR_ALPHA       210     /* backing rectangle, 0-255 */
#define INV_QTY_FONT_SIZE   10

/* ── Module state ─────────────────────────────────────────────────────── */

static ObjectLayersManager* s_ol_manager = NULL;

static UIToggle s_bar_toggle;
static bool     s_bar_toggle_init = false;

/* A stack that lands in a cell slides in from where it came: the screen point
 * it starts from and its age, per bag slot. */
static Vector2 s_settle_from[INVENTORY_LAYERS_MAX];
static float   s_settle_age[INVENTORY_LAYERS_MAX];

/* The stack a press landed on: its bag slot, the wallet for the coin slot. */
static bool    s_lift_armed = false;
static int     s_lift_slot  = 0;
static bool    s_lifting    = false;     /* the press passed the slop */
static Vector2 s_lift_press;

/* Scroll offset of the strip in px. */
static float s_scroll_px = 0.0f;
/* A press on the track drags the thumb, held `s_track_grab` px from its left edge. */
static bool  s_track_held = false;
static float s_track_grab = 0.0f;
/* A press on a cell without a stack: a tap once it lifts without travel. */
static bool    s_press_armed = false;
static Vector2 s_press;
static bool  s_tap_pending = false;      /* clean release awaiting activation */
static int   s_tap_x = 0;
static int   s_tap_y = 0;

/* Pointer travel that cancels a tap. */
#define INV_DRAG_SLOP_PX 6.0f

/* ── Internal colours ──────────────────────────────────────────────────── */

static const Color C_BAR_BG        = {  10,  10,  20, INV_BAR_ALPHA };
static const Color C_COIN_BORDER   = { 230, 190,  60, 200 };  /* gold border for coin slot */
static const Color C_COIN_QTY_TEXT = { 255, 215,   0, 255 };
static const Color C_PREVIEW_BG    = {  18,  20,  32, 210 };
static const Color C_DROP_FREE     = { 120, 200, 140, 255 };
static const Color C_DROP_REFUSE   = { 210,  90,  80, 255 };

/* ── Internal helpers ─────────────────────────────────────────────────── */

static int bar_slot_size(void)    { return viewport_is_mobile() ? 50 : INV_SLOT_SIZE; }
static int bar_slot_gap(void)     { return viewport_is_mobile() ? 4 : INV_SLOT_GAP; }
static int bar_slot_padding(void) { return viewport_is_mobile() ? 3 : INV_SLOT_PADDING; }
static int bar_qty_font(void)     { return viewport_is_mobile() ? 9 : INV_QTY_FONT_SIZE; }
static int bar_toggle_size(void)  { return viewport_is_mobile() ? 28 : 32; }
static int bar_toggle_pad(void)   { return viewport_is_mobile() ? 5 : 6; }
static int bar_margin(void)       { return viewport_is_mobile() ? 5 : INV_BAR_MARGIN; }
static int bar_track_h(void)      { return viewport_is_mobile() ? 12 : INV_TRACK_H; }

static int slot_pitch(void) { return bar_slot_size() + bar_slot_gap(); }

/* The coin slot is pinned to the right edge, outside the scroll strip. */
static float coin_slot_x(int screen_w) {
    return (float)(screen_w - bar_slot_size() - bar_slot_gap());
}

/* Scrollable strip bounds — everything left of the pinned coin slot. */
static float strip_left(void)             { return (float)bar_slot_gap(); }
static float strip_right(int screen_w)    { return coin_slot_x(screen_w) - (float)bar_slot_gap(); }

/* Cells of the strip: every slot the capacity can open, and a stack past them. */
static int strip_cells(void) {
    int cells = CYBERIA_INVENTORY_SLOTS_MAX;
    for (int i = 0; i < g_local_player.inventory_count; i++) {
        if (g_local_player.inventory[i].slot >= cells) cells = g_local_player.inventory[i].slot + 1;
    }
    return cells;
}

/* Furthest the strip may slide before its last cell reaches the right edge. */
static float max_scroll_px(int screen_w, int scroll_count) {
    float content = scroll_count > 0 ? (float)(scroll_count * slot_pitch() - bar_slot_gap()) : 0.0f;
    float max = content - (strip_right(screen_w) - strip_left());
    return max > 0.0f ? max : 0.0f;
}

/* The bar holds the track row only while the cells overrun the strip. */
static bool strip_scrolls(void) { return 0.0f < max_scroll_px(GetScreenWidth(), strip_cells()); }

static int bar_height(void) {
    int slots = bar_slot_size() + 2 * bar_margin();
    return strip_scrolls() ? slots + bar_track_h() + bar_margin() : slots;
}

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
        s_bar_toggle.icon_expanded  = "close-yellow";
        s_bar_toggle.icon_collapsed = "bag";
        /* Drag the button in any direction to show or hide the bar. */
        ui_toggle_set_drag(&s_bar_toggle);
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
 * be used for matching, see local_player_is_coin(). */
static const char* coin_item_key(void) {
    const EntityTypeDefault* def = game_state_get_entity_default("coin");
    if (def && def->live_item_id_count > 0) return def->live_item_ids[0];
    return NULL;
}

/* find_coin_slot returns the index of the coin slot in full_inventory, or -1. */
static int find_coin_slot(void) {
    for (int i = 0; i < g_local_player.inventory_count; i++) {
        if (local_player_is_coin(g_local_player.inventory[i].item_id)) return i;
    }
    return -1;
}

/* The inventory index of the stack the strip shows in cell si, or -1. A first
 * copy waits hidden for its pickup flight. */
static int shown_stack(int si) {
    int i = local_player_bag_stack(si);
    return 0 <= i && fx_inventory_bar_qty_slot_visible(g_local_player.inventory[i].item_id) ? i : -1;
}

/* The slots sit in the bottom row of the bar. */
static float slot_top(float bar_top) { return bar_top + (float)(bar_height() - bar_margin() - bar_slot_size()); }

/* slot_rect returns the screen rect of strip cell si. */
static Rectangle slot_rect(int si, float bar_top) {
    float x = strip_left() + (float)si * (float)slot_pitch() - s_scroll_px;
    return (Rectangle){ x, slot_top(bar_top), (float)bar_slot_size(), (float)bar_slot_size() };
}

/* coin_slot_rect returns the rect for the pinned coin slot (right of scrollable area). */
static Rectangle coin_slot_rect(int screen_w, float bar_top) {
    return (Rectangle){ coin_slot_x(screen_w), slot_top(bar_top), (float)bar_slot_size(), (float)bar_slot_size() };
}

static void clamp_scroll(void) {
    s_scroll_px = fminf(fmaxf(s_scroll_px, 0.0f), max_scroll_px(GetScreenWidth(), strip_cells()));
}

/* The scroll track spans the strip in the top row of the bar. */
static Rectangle track_rect(float bar_top) {
    return (Rectangle){ strip_left(), bar_top + (float)bar_margin(), strip_right(GetScreenWidth()) - strip_left(),
                        (float)bar_track_h() };
}

/* The track and its margins: the press target of the track. */
static Rectangle track_band(float bar_top) {
    return (Rectangle){ strip_left(), bar_top, strip_right(GetScreenWidth()) - strip_left(),
                        slot_top(bar_top) - bar_top };
}

/* The thumb: as wide as the share of the cells in view, placed at the scroll offset. */
static Rectangle thumb_rect(Rectangle track) {
    float max = max_scroll_px(GetScreenWidth(), strip_cells());
    float view = strip_right(GetScreenWidth()) - strip_left();
    float width = fmaxf(2.0f * track.height, track.width * view / (view + max));
    float at = 0.0f < max ? s_scroll_px / max : 0.0f;
    return (Rectangle){ track.x + (track.width - width) * at, track.y, width, track.height };
}

/* Scroll so the thumb's grab point follows the pointer x. */
static void drag_thumb(float x) {
    Rectangle track = track_rect(bar_top(GetScreenHeight()));
    float travel = track.width - thumb_rect(track).width;
    if (0.0f < travel)
        s_scroll_px = (x - s_track_grab - track.x) / travel * max_scroll_px(GetScreenWidth(), strip_cells());
    clamp_scroll();
}

static void draw_track(float bar_top) {
    Rectangle track = track_rect(bar_top);
    DrawRectangleRec(track, MODAL_PANEL_BG);
    DrawRectangleLinesEx(track, 1.0f, MODAL_PANEL_BORDER);
    bool hot = s_track_held || CheckCollisionPointRec(GetMousePosition(), track_band(bar_top));
    Color fill = hot ? ColorBrightness(MODAL_SELECTED, 0.14f) : MODAL_SELECTED;
    Color light = ColorBrightness(fill, 0.45f);
    Rectangle thumb = thumb_rect(track);
    Rectangle inner = draw_pixel_bevel(thumb, 0.0f, 2.0f, fill, light, ColorBrightness(fill, -0.45f));
    /* Grip: three ridges at the thumb centre. */
    for (int i = -1; 1 >= i; i++)
        DrawRectangle((int)(inner.x + inner.width * 0.5f) + 3 * i, (int)inner.y + 2, 1, (int)inner.height - 4, light);
}

/* draw_coin_slot renders the pinned right coin slot. `coin_idx` is the
 * matched full_inventory entry (from find_coin_slot()), so the sprite
 * reflects the player's actual coin skin variant when one is owned. */
static void draw_coin_slot(Rectangle r, int coin_idx, ObjectLayersManager* mgr) {
    /* Pixel-retro coin slot: black outer border, gold inner fill with
     * highlight/shadow edges, gold border overlay, hover brightens fill. */
    bool hovered = CheckCollisionPointRec(GetMousePosition(), r);
    Color gold_fill = hovered ? (Color){ 55, 44, 20, 230 } : (Color){ 35, 28, 10, 210 };
    Color gold_highlight = ColorBrightness(gold_fill, 0.45f);
    Color gold_shadow = ColorBrightness(gold_fill, -0.45f);

    draw_pixel_bevel(r, 0.0f, 4.0f, gold_fill, gold_highlight, gold_shadow);
    /* Gold border overlay */
    DrawRectangleLinesEx(r, 2.0f, C_COIN_BORDER);

    /* Coin sprite (animated, down_idle) */
    const char* ck = (coin_idx >= 0) ? g_local_player.inventory[coin_idx].item_id : coin_item_key();
    if (ck && ck[0] != '\0') {
        int inner = bar_slot_size() - bar_slot_padding() * 2;
        ol_as_ico_draw(mgr, ck,
                       (int)(r.x + bar_slot_padding()),
                       (int)(r.y + bar_slot_padding()),
                       inner, OL_ICO_DEFAULT_DIR, 0, WHITE);
    }

    /* Coin balance from fast flat field */
    int balance = local_player_coins();
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

/* The entities the skills of `ols` summon, in a row above its slot, each with
 * the units the player holds of it, and the cooldown of the row under them.
 * The row takes no input. */
static void draw_skill_previews(Rectangle slot, const ObjectLayerState* ols, float floor_y, double now) {
    SkillPreview previews[SKILL_PREVIEW_MAX];
    bool held;
    int n = skill_preview_summons(ols, previews, &held);
    if (0 == n) return;

    Rectangle icons[SKILL_PREVIEW_MAX];
    Rectangle bar;
    skill_preview_layout(slot, floor_y, s_bar_toggle.anchor, n, icons, &bar);
    for (int i = 0; n > i; i++) {
        Rectangle inner = draw_pixel_bevel(icons[i], 0.0f, 2.0f, C_PREVIEW_BG,
                                           ColorBrightness(C_PREVIEW_BG, 0.45f),
                                           ColorBrightness(C_PREVIEW_BG, -0.45f));
        Rectangle art = ui_rect_inset(inner, 1.0f);
        ObjectLayerState stack = { .active = true, .quantity = 1 };
        copy_str(stack.item_id, sizeof stack.item_id, previews[i].item_id);
        ol_stack_ico_draw(s_ol_manager, &stack, 1, (int)art.x, (int)art.y, (int)art.width,
                          OL_ICO_DEFAULT_DIR, 0, WHITE);
        item_slot_draw_quantity(icons[i], previews[i].quantity);
    }
    fx_shape_cooldown_bar(bar, held ? local_player_consume_charge(now) : local_player_action_charge(now));
}

/* The thumb follows a held track; a press off the stacks taps on a clean release. */
static void update_press(void) {
    if (!bar_slots_settled()) s_track_held = s_press_armed = false;
    bool down = input_pointer_down();
    Vector2 p = input_pointer_position();
    if (s_track_held) {
        s_track_held = down;
        if (down) drag_thumb(p.x);
    }
    if (!s_press_armed) return;
    if (INV_DRAG_SLOP_PX < hypotf(p.x - s_press.x, p.y - s_press.y)) s_press_armed = false;
    else if (!down) {
        s_tap_pending = true;
        s_tap_x = (int)s_press.x;
        s_tap_y = (int)s_press.y;
        s_press_armed = false;
    }
}

/* ── Lifted stacks ────────────────────────────────────────────────────── */

/* Pixels per second the strip scrolls while a lifted stack waits at its edge. */
#define INV_EDGE_SCROLL_PX 360.0f

/* Strip cell index under the screen point, or -1 off the strip. */
static int strip_cell_at(int mx, int my) {
    if (!s_bar_toggle.expanded) return -1;
    float current_bar_top = bar_top(GetScreenHeight());
    if ((float)my < current_bar_top || (float)mx < strip_left() || (float)mx >= strip_right(GetScreenWidth()))
        return -1;
    int cells = strip_cells();
    for (int si = 0; si < cells; si++) {
        if (ui_button_hit(slot_rect(si, current_bar_top), mx, my)) return si;
    }
    return -1;
}

/* The ghost of the lifted stack under the pointer. */
static Rectangle lift_ghost(Vector2 p) {
    float size = (float)bar_slot_size();
    return (Rectangle){ p.x - size * 0.5f, p.y - size * 0.5f, size, size };
}

/* Where a lifted stack would land. */
typedef enum { LIFT_NONE, LIFT_CELL, LIFT_OFFER, LIFT_WORLD } LiftTarget;

/* Where a lifted stack would land: a strip cell, a cell of the open container,
 * the own offer of an open trade, or the ground under the pointer. */
static LiftTarget lift_target(Vector2 p, Box* box, int* slot) {
    int cell = strip_cell_at((int)p.x, (int)p.y);
    if (0 <= cell) {
        *box = BOX_BAG;
        *slot = cell;
        return LIFT_CELL;
    }
    if (container_panel_cell_at((int)p.x, (int)p.y, box, slot)) return LIFT_CELL;
    if (BAG_WALLET_SLOT == s_lift_slot) return LIFT_NONE;
    if (trade_panel_offer_at((int)p.x, (int)p.y)) return LIFT_OFFER;
    return ui_dispatch_covers_point((int)p.x, (int)p.y) ? LIFT_NONE : LIFT_WORLD;
}

/* Carry a lifted stack and drop it on release. Off the strip, or where the
 * move is refused, the stack stays where it was. A press that never travelled
 * is a tap. */
static void update_lift(float dt) {
    if (!s_lift_armed) return;
    Vector2 p = input_pointer_position();
    if (!s_lifting && INV_DRAG_SLOP_PX < hypotf(p.x - s_lift_press.x, p.y - s_lift_press.y)) s_lifting = true;
    if (s_lifting && p.y >= bar_top(GetScreenHeight())) {
        float edge = (float)slot_pitch();
        if (p.x < strip_left() + edge)                      s_scroll_px -= INV_EDGE_SCROLL_PX * dt;
        else if (p.x > strip_right(GetScreenWidth()) - edge) s_scroll_px += INV_EDGE_SCROLL_PX * dt;
        clamp_scroll();
    }
    if (input_pointer_down()) return;
    Box box = BOX_BAG;
    int to = 0;
    if (!s_lifting) {
        s_tap_pending = true;
        s_tap_x = (int)s_lift_press.x;
        s_tap_y = (int)s_lift_press.y;
    } else {
        const ObjectLayerState* stack = container_ops_stack(BOX_BAG, s_lift_slot);
        switch (lift_target(p, &box, &to)) {
            case LIFT_CELL:  stack_move_drop(BOX_BAG, s_lift_slot, box, to, lift_ghost(p)); break;
            case LIFT_OFFER: trade_panel_offer_bag_stack(s_lift_slot); break;
            case LIFT_WORLD:
                if (NULL != stack) world_drop_ask(BOX_BAG, s_lift_slot, stack->item_id, stack->quantity, world_drop_cell(p));
                break;
            case LIFT_NONE: break;
        }
    }
    s_lift_armed = false;
    s_lifting = false;
}

static void draw_lift(void) {
    if (!s_lifting) return;
    const ObjectLayerState* stack = container_ops_stack(BOX_BAG, s_lift_slot);
    if (NULL == stack) return;
    Vector2 p = input_pointer_position();
    Box box = BOX_BAG;
    int to = 0;
    const char* reason = NULL;
    switch (lift_target(p, &box, &to)) {
        case LIFT_CELL:
            if (BOX_BAG == box && to == s_lift_slot) break;
            {
                bool lands = MOVE_REFUSED != container_ops_check(BOX_BAG, s_lift_slot, box, to, stack->quantity);
                Rectangle cell = BOX_BAG == box ? slot_rect(to, bar_top(GetScreenHeight())) : container_panel_cell_rect(to);
                DrawRectangleLinesEx(cell, 3.0f, lands ? C_DROP_FREE : C_DROP_REFUSE);
            }
            break;
        case LIFT_OFFER: DrawRectangleLinesEx(trade_panel_offer_rect(), 3.0f, C_DROP_FREE); break;
        case LIFT_WORLD: world_drop_draw_target(world_drop_cell(p), world_drop_allowed(stack->item_id, &reason)); break;
        case LIFT_NONE: break;
    }
    item_slot_draw_ex(lift_ghost(p), stack, s_ol_manager, C_DROP_FREE, 0.25f, true);
}

static bool settling(int si) { return ITEM_SLOT_GRID_SETTLE_DUR > s_settle_age[si]; }

/* Where a sliding stack is drawn: eased from where it came toward its cell. */
static Rectangle settled_rect(int si, Rectangle cell) {
    float e = ease_out_cubic(s_settle_age[si] / ITEM_SLOT_GRID_SETTLE_DUR);
    return (Rectangle){ ease_approach(s_settle_from[si].x, cell.x, e), ease_approach(s_settle_from[si].y, cell.y, e),
                        cell.width, cell.height };
}

/* ── Public API ─────────────────────────────────────────────────────────── */

void inventory_bar_init(ObjectLayersManager* ol_manager) {
    s_ol_manager    = ol_manager;
    s_scroll_px     = 0.0f;
    s_track_held    = false;
    s_press_armed   = false;
    s_tap_pending   = false;
    s_lift_armed    = false;
    s_lifting       = false;
    for (int i = 0; i < INVENTORY_LAYERS_MAX; i++) s_settle_age[i] = ITEM_SLOT_GRID_SETTLE_DUR;
    s_bar_toggle_init = false;
    ensure_bar_toggle();
    fx_inventory_bar_qty_init();
}

void inventory_bar_update(float dt) {
    ensure_bar_toggle();
    ui_toggle_update(&s_bar_toggle, dt);
    ensure_bar_toggle();
    fx_inventory_bar_qty_update(dt);
    for (int i = 0; i < INVENTORY_LAYERS_MAX; i++) {
        if (ITEM_SLOT_GRID_SETTLE_DUR > s_settle_age[i]) s_settle_age[i] += dt;
    }
    update_lift(dt);
    update_press();
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

bool inventory_bar_handle_wheel(float wheel_delta) {
    ensure_bar_toggle();
    Vector2 p = input_pointer_position();
    if (!bar_slots_settled() || p.y < bar_top(GetScreenHeight())) return false;
    s_scroll_px -= wheel_delta * (float)slot_pitch();
    clamp_scroll();
    return true;
}

/* A stack or the coins in the own offer of an open trade show the trade icon. */
static void draw_offered(Rectangle cell) {
    float size = cell.width * 0.34f;
    Rectangle badge = { cell.x + cell.width - size - 2.0f, cell.y + 2.0f, size, size };
    DrawRectangleRec(badge, (Color){ 8, 12, 22, 220 });
    ui_icon_draw("trade", badge.x + size * 0.5f, badge.y + size * 0.5f, (int)size - 2, false, 0.0f);
}

void inventory_bar_draw(void) {
    ensure_bar_toggle();
    int screen_w = GetScreenWidth();
    int screen_h = GetScreenHeight();
    float current_bar_top = bar_top(screen_h);
    /* Skill previews follow the bar top to the screen edge. */
    bool  shown    = 0.0f < s_bar_toggle.anim_t;
    /* Stasis hides skill previews over modals. */
    bool  previews = !local_player_in_stasis();
    double now     = GetTime();

    if (shown) {
        DrawRectangle(0, (int)current_bar_top, screen_w, bar_height(), C_BAR_BG);
        DrawLine(0, (int)current_bar_top, screen_w, (int)current_bar_top,
                 (Color){80, 80, 120, 160});
    }

    int coin_idx = find_coin_slot();
    const TradeOffer* offer = &g_local_player.trade.mine;
    bool trading = local_player_trading();
    int cells = strip_cells();
    int open = local_player_open_slots(CYBERIA_INVENTORY_CAPACITY);
    float left  = strip_left();
    float right = strip_right(screen_w);
    int moving[INVENTORY_LAYERS_MAX];
    int moving_count = 0;

    for (int si = 0; si < cells; si++) {
        Rectangle r = slot_rect(si, current_bar_top);
        if (left >= r.x + r.width) continue;
        if (right <= r.x)          break;

        int inv_idx = shown_stack(si);
        if (0 > inv_idx || (s_lifting && si == s_lift_slot)) {
            if (shown && si < open) item_slot_draw_empty(r);
            else if (shown)         item_slot_draw_locked(r, local_player_unlock_level(CYBERIA_INVENTORY_CAPACITY, si));
            continue;
        }
        ObjectLayerState ol = g_local_player.inventory[inv_idx];
        if (shown && settling(si)) {
            item_slot_draw_empty(r);
            moving[moving_count++] = si;
        } else if (shown) {
            ol.quantity = fx_inventory_bar_qty_display(ol.item_id, si, ol.quantity);
            /* During the arrival pulse the sprite renders in full colour. */
            item_slot_draw_ex(ui_rect_scale(r, fx_inventory_bar_qty_slot_scale(ol.item_id, si)),
                              &ol, s_ol_manager, WHITE, 0.0f,
                              fx_inventory_bar_qty_slot_pulsing(ol.item_id, si));
            if (bar_slots_settled()) fx_inventory_bar_qty_draw(r, ol.item_id, si);
            for (int i = 0; trading && i < offer->count; i++) {
                if (offer->items[i].slot == si) draw_offered(r);
            }
        }
        if (previews) draw_skill_previews(r, &ol, current_bar_top, now);
    }

    /* Sliding stacks ride above the settled cells. */
    for (int m = 0; m < moving_count; m++) {
        int si = moving[m];
        ObjectLayerState ol = g_local_player.inventory[local_player_bag_stack(si)];
        ol.quantity = fx_inventory_bar_qty_display(ol.item_id, si, ol.quantity);
        item_slot_draw_ex(settled_rect(si, slot_rect(si, current_bar_top)), &ol, s_ol_manager, WHITE, 0.0f, true);
    }

    if (shown) {
        Rectangle cr = coin_slot_rect(screen_w, current_bar_top);
        const char* coin_key = (coin_idx >= 0) ? g_local_player.inventory[coin_idx].item_id : coin_item_key();
        draw_coin_slot(ui_rect_scale(cr, fx_inventory_bar_qty_slot_scale(coin_key, BAG_WALLET_SLOT)), coin_idx,
                       s_ol_manager);
        if (trading && 0 < offer->coins) draw_offered(cr);
        if (bar_slots_settled()) fx_inventory_bar_qty_draw(cr, coin_key, BAG_WALLET_SLOT);
        if (strip_scrolls()) draw_track(current_bar_top);
    }

    if (!bar_slots_settled()) fx_inventory_bar_qty_draw_bottom(s_bar_toggle.anchor);
    ui_toggle_draw(&s_bar_toggle);
    draw_lift();
}

bool inventory_bar_handle_click(int mx, int my, int* out_slot) {
    ensure_bar_toggle();
    /* Activation is deferred to a clean release — see inventory_bar_take_tap. */
    if (out_slot) *out_slot = -1;
    if (inventory_bar_handle_toggle_click(mx, my)) return true;

    float current_bar_top = bar_top(GetScreenHeight());
    if ((float)my < current_bar_top || my >= GetScreenHeight()) return false;
    if (!s_bar_toggle.expanded) return s_bar_toggle.anim_t > 0.0f;

    if (strip_scrolls() && ui_button_hit(track_band(current_bar_top), mx, my)) {
        Rectangle thumb = thumb_rect(track_rect(current_bar_top));
        bool on_thumb = thumb.x <= (float)mx && (float)mx < thumb.x + thumb.width;
        s_track_held = true;
        s_track_grab = on_thumb ? (float)mx - thumb.x : thumb.width * 0.5f;
        drag_thumb((float)mx);
        return true;
    }

    int cell = strip_cell_at(mx, my);
    bool coin = ui_button_hit(coin_slot_rect(GetScreenWidth(), current_bar_top), mx, my) && 0 <= find_coin_slot();
    if (coin || (0 <= cell && 0 <= shown_stack(cell))) {
        s_lift_armed = true;
        s_lift_slot  = coin ? BAG_WALLET_SLOT : cell;
        s_lifting    = false;
        s_lift_press = (Vector2){ (float)mx, (float)my };
        return true;
    }

    s_press_armed = true;
    s_press       = (Vector2){ (float)mx, (float)my };
    return true;
}

bool inventory_bar_take_tap(int* out_slot) {
    if (!s_tap_pending) return false;
    s_tap_pending = false;
    int slot = inventory_bar_get_tapped_slot(s_tap_x, s_tap_y);
    if (out_slot) *out_slot = slot;
    int cell = strip_cell_at(s_tap_x, s_tap_y);
    if (0 > slot && cell >= local_player_open_slots(CYBERIA_INVENTORY_CAPACITY)) {
        int level = local_player_unlock_level(CYBERIA_INVENTORY_CAPACITY, cell);
        char body[96];
        if (0 < level) snprintf(body, sizeof(body), "This bag slot opens at level %d.", level);
        else           copy_str(body, sizeof(body), "This bag slot does not open in this world.");
        modal_notification_show("Locked Slot", body, (Color){ 230, 170, 110, 255 });
    }
    return true;
}

bool inventory_bar_point_covered(int mx, int my) {
    ensure_bar_toggle();
    if (ui_button_hit(s_bar_toggle.anchor, mx, my)) return true;
    if (s_bar_toggle.anim_t <= 0.0f) return false;
    return (float)my >= bar_top(GetScreenHeight()) && my < GetScreenHeight();
}

static int inventory_bar_slot_at(int mx, int my, Rectangle* out_rect) {
    ensure_bar_toggle();
    if (!s_bar_toggle.expanded) return -1;
    int screen_w = GetScreenWidth();
    int screen_h = GetScreenHeight();
    float current_bar_top = bar_top(screen_h);

    if ((float)my < current_bar_top || my > screen_h) return -1;

    int coin_idx = find_coin_slot();

    /* Coin slot: return coin idx so modal can show it */
    Rectangle cr = coin_slot_rect(screen_w, current_bar_top);
    if (ui_button_hit(cr, mx, my)) {
        if (0 <= coin_idx && NULL != out_rect) *out_rect = cr;
        return (0 <= coin_idx) ? coin_idx : -1;
    }

    int cell = strip_cell_at(mx, my);
    int inv_idx = 0 <= cell ? shown_stack(cell) : -1;
    if (0 <= inv_idx && NULL != out_rect) *out_rect = slot_rect(cell, current_bar_top);
    return inv_idx;
}

int inventory_bar_get_tapped_slot(int mx, int my) {
    return inventory_bar_slot_at(mx, my, NULL);
}

void inventory_bar_animate_from(int slot, Rectangle origin) {
    if (0 > slot || INVENTORY_LAYERS_MAX <= slot) return;
    s_settle_from[slot] = (Vector2){ origin.x, origin.y };
    s_settle_age[slot] = 0.0f;
}

bool inventory_bar_cell_at(int mx, int my, int* slot) {
    ensure_bar_toggle();
    if (!bar_slots_settled()) return false;
    if (ui_button_hit(coin_slot_rect(GetScreenWidth(), bar_top(GetScreenHeight())), mx, my)) {
        *slot = BAG_WALLET_SLOT;
        return true;
    }
    int cell = strip_cell_at(mx, my);
    if (0 > cell) return false;
    *slot = cell;
    return true;
}

Rectangle inventory_bar_cell_rect(int slot) {
    ensure_bar_toggle();
    float top = bar_top(GetScreenHeight());
    return BAG_WALLET_SLOT == slot ? coin_slot_rect(GetScreenWidth(), top) : slot_rect(slot, top);
}

Vector2 inventory_bar_cell_center(int slot) {
    ensure_bar_toggle();
    Rectangle r = bar_slots_settled() ? inventory_bar_cell_rect(slot) : s_bar_toggle.anchor;
    return (Vector2){ r.x + r.width * 0.5f, r.y + r.height * 0.5f };
}

static bool inventory_bar_item_slot_rect(const char* item_id, bool append_missing,
                                         Rectangle* out) {
    if (!out || !item_id || item_id[0] == '\0') return false;

    ensure_bar_toggle();
    int screen_w = GetScreenWidth();
    int screen_h = GetScreenHeight();
    float current_bar_top = bar_top(screen_h);
    if (local_player_is_coin(item_id)) {
        *out = coin_slot_rect(screen_w, current_bar_top);
        return true;
    }

    /* The first stack of the item, else the slot a new stack takes: the lowest free one. */
    int si = -1;
    for (int i = 0; i < g_local_player.inventory_count; i++) {
        const ObjectLayerState* stack = &g_local_player.inventory[i];
        if (0 == strcmp(stack->item_id, item_id) && (0 > si || stack->slot < si)) si = stack->slot;
    }
    if (0 > si && append_missing) {
        for (si = 0; 0 <= local_player_bag_stack(si); si++) {}
    }
    if (0 > si) return false;

    Rectangle slot = slot_rect(si, current_bar_top);
    float left  = strip_left();
    float right = strip_right(screen_w);
    if (slot.x < left) slot.x = left;
    if (slot.x + slot.width > right) slot.x = right - slot.width;
    *out = slot;
    return true;
}

bool inventory_bar_predicted_item_slot_rect(const char* item_id, Rectangle* out) {
    return inventory_bar_item_slot_rect(item_id, true, out);
}

bool inventory_bar_item_slot_center(const char* item_id, Vector2* out) {
    if (!out || !item_id || item_id[0] == '\0') return false;

    ensure_bar_toggle();
    if (!bar_slots_settled()) {
        *out = (Vector2){ s_bar_toggle.anchor.x + s_bar_toggle.anchor.width * 0.5f,
                          s_bar_toggle.anchor.y + s_bar_toggle.anchor.height * 0.5f };
        return true;
    }

    Rectangle slot;
    if (inventory_bar_item_slot_rect(item_id, false, &slot)) {
        *out = (Vector2){ slot.x + slot.width * 0.5f, slot.y + slot.height * 0.5f };
        return true;
    }

    /* Unknown item — aim at the bar center. */
    int screen_w = GetScreenWidth();
    int screen_h = GetScreenHeight();
    float current_bar_top = bar_top(screen_h);
    *out = (Vector2){ screen_w * 0.5f, current_bar_top + bar_height() * 0.5f };
    return true;
}
