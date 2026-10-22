#include "trade_panel.h"

#include "content/stat_contract_generated.h"
#include "item_slot.h"
#include "modal.h"
#include "modal_interact.h"
#include "modal_notification.h"
#include "ol_as_animated_ico.h"
#include "sim/game_state.h"
#include "sim/local_player.h"
#include "text.h"
#include "ui_button.h"
#include "ui_icon.h"
#include "ui_scroll.h"
#include "utils.h"
#include "view/viewport.h"
#include "world/economy.h"
#include "world/object_layers_management.h"

#include <math.h>
#include <raylib.h>
#include <stdio.h>
#include <string.h>

#define TP_COLUMNS     4
#define TP_BUTTONS_MAX (CYBERIA_TRADE_OFFER_MAX_STACKS + 8)

typedef enum { TP_REQUEST, TP_CANCEL, TP_ACCEPT, TP_DECLINE, TP_CONFIRM, TP_COINS, TP_CLEAR_COINS, TP_TAKE_BACK } TradeAction;

typedef struct {
    Rectangle   rect;
    TradeAction action;
    int         line;   /* TP_TAKE_BACK: the offered line */
} TradeButton;

static struct {
    UIScroll    scroll;
    float       content_h;
    TradeButton buttons[TP_BUTTONS_MAX];
    int         button_count;
    EntityID    partner;          /* the player the tab shows */
    Rectangle   offer_area;       /* the own offer grid, while an open trade shows */
    bool        offer_shown;
    EntityID    prompted;         /* the request the prompt asked about */
    char        prompted_name[PLAYER_NAME_BYTES];
    int         pending_slot;     /* the bag slot the quantity picker asks about */
} s_tp;

static const Color C_TRADE     = {  90, 170, 230, 255 };
static const Color C_DISABLED  = {  44,  46,  58, 255 };
static const Color C_DANGER    = { 130,  44,  44, 255 };
static const Color C_CONFIRMED = { 110, 210, 130, 255 };
static const Color C_WAITING   = { 230, 170, 110, 255 };
static const Color C_BORDER    = {  80, 160, 220, 240 };

static int   font_body(void) { return viewport_is_mobile() ? 12 : 15; }
static float button_h(void)  { return viewport_is_mobile() ? 32.0f : 36.0f; }
static float gap(void)       { return viewport_is_mobile() ? 6.0f : 8.0f; }
static float max_cell(void)  { return viewport_is_mobile() ? 48.0f : 60.0f; }

void trade_panel_reset(void) {
    ui_scroll_reset(&s_tp.scroll);
    s_tp.content_h = 0.0f;
    s_tp.button_count = 0;
    s_tp.offer_shown = false;
}

bool trade_panel_with(EntityID id) {
    return TRADE_NONE != g_local_player.trade.state && g_local_player.trade.partner_id == id;
}

/* ── Requests ─────────────────────────────────────────────────────────── */

static void accept_request(void) {
    local_player_request_trade_reply(s_tp.prompted, true);
    modal_interact_open(s_tp.prompted, s_tp.prompted_name, "", false, C_BORDER);
}

static void decline_request(void) { local_player_request_trade_reply(s_tp.prompted, false); }

void trade_panel_update(void) {
    const Trade* t = &g_local_player.trade;
    if (TRADE_INCOMING != t->state) {
        s_tp.prompted = 0;
        return;
    }
    if (s_tp.prompted == t->partner_id) return;
    s_tp.prompted = t->partner_id;
    copy_str(s_tp.prompted_name, sizeof s_tp.prompted_name, t->partner_name);
    char message[96];
    snprintf(message, sizeof message, "%s wants to trade with you.", t->partner_name);
    modal_notification_show_prompt("Trade request", message, C_TRADE, "Accept", "trade", t->expires_at,
                                   accept_request, decline_request);
}

/* The own offer with one line changed: `qty` units of bag slot `slot`, or the
 * line taken out when qty is 0. */
static void offer_slot(int slot, int qty) {
    TradeOffer offer = g_local_player.trade.mine;
    int at = -1;
    for (int i = 0; i < offer.count; i++) if (offer.items[i].slot == slot) at = i;
    if (0 <= at && 0 == qty) {
        offer.items[at] = offer.items[--offer.count];
    } else if (0 <= at) {
        offer.items[at].quantity = qty;
    } else if (0 < qty && CYBERIA_TRADE_OFFER_MAX_STACKS > offer.count) {
        offer.items[offer.count++] = (TradeLine){ .slot = slot, .quantity = qty };
    }
    local_player_request_trade_offer(&offer);
}

static void offer_units(const char* item_id, int qty) { offer_slot(s_tp.pending_slot, qty); }

static void offer_coins(const char* item_id, int qty) {
    TradeOffer offer = g_local_player.trade.mine;
    offer.coins = qty;
    local_player_request_trade_offer(&offer);
}

void trade_panel_offer_bag_stack(int bag_slot) {
    const Trade* t = &g_local_player.trade;
    int index = local_player_bag_stack(bag_slot);
    if (TRADE_OPEN != t->state || 0 > index) return;
    const ObjectLayerState* stack = &g_local_player.inventory[index];
    if (!economy_policy(stack->item_id)->trade) {
        modal_notification_show("Trade", "This item cannot be traded.", C_WAITING);
        return;
    }
    bool offered = false;
    for (int i = 0; i < t->mine.count; i++) offered = offered || t->mine.items[i].slot == bag_slot;
    if (!offered && CYBERIA_TRADE_OFFER_MAX_STACKS <= t->mine.count) {
        char full[64];
        snprintf(full, sizeof full, "An offer holds %d stacks. Take one back first.", CYBERIA_TRADE_OFFER_MAX_STACKS);
        modal_notification_show("Trade", full, C_WAITING);
        return;
    }
    s_tp.pending_slot = bag_slot;
    if (1 == stack->quantity) {
        offer_slot(bag_slot, 1);
        return;
    }
    modal_notification_show_split("Offer", "How many do you offer?", C_TRADE, stack->item_id, stack->quantity,
                                  "Offer", "trade", offer_units, NULL);
}

static void perform(const TradeButton* b) {
    const Trade* t = &g_local_player.trade;
    switch (b->action) {
        case TP_REQUEST:     local_player_request_trade(s_tp.partner); return;
        case TP_CANCEL:      local_player_request_trade_cancel(); return;
        case TP_ACCEPT:      local_player_request_trade_reply(s_tp.partner, true); return;
        case TP_DECLINE:     local_player_request_trade_reply(s_tp.partner, false); return;
        case TP_CONFIRM:     local_player_request_trade_confirm(); return;
        case TP_CLEAR_COINS: offer_coins(NULL, 0); return;
        case TP_TAKE_BACK:   offer_slot(t->mine.items[b->line].slot, 0); return;
        case TP_COINS:
            if (0 < local_player_coins())
                modal_notification_show_split("Offer coins", "How many coins do you offer?", C_TRADE,
                                              economy_coin_item(), local_player_coins(), "Offer", "wallet",
                                              offer_coins, NULL);
            return;
    }
}

/* ── Draw ─────────────────────────────────────────────────────────────── */

static void target(Rectangle r, TradeAction action, int line) {
    if (!CheckCollisionRecs(r, s_tp.scroll.view) || TP_BUTTONS_MAX <= s_tp.button_count) return;
    s_tp.buttons[s_tp.button_count++] = (TradeButton){ .rect = r, .action = action, .line = line };
}

static void button(Rectangle r, const char* icon, const char* label, Color bg, bool enabled, TradeAction action) {
    UIButtonPixelRetroStyle style = {
        .bg = enabled ? bg : C_DISABLED, .icon_id = icon, .label = label,
        .font_size = font_body(), .text_color = enabled ? MODAL_TEXT : MODAL_LABEL, .enabled = enabled,
    };
    ui_button_pixel_retro_draw(r, &style, enabled && ui_button_hit(r, GetMouseX(), GetMouseY()));
    if (enabled) target(r, action, -1);
}

static float message(Rectangle c, float y, const char* text, Color color) {
    return (float)text_wrap(text, (int)c.x, (int)y, (int)c.width, font_body(), color, false, true) + gap();
}

/* One side of the trade: its stacks in a grid, its coins and whether it
 * confirmed. The own side of an open trade takes stacks and coins. */
static float draw_offer(Rectangle c, float y, const char* title, const TradeOffer* offer, bool own, bool open) {
    float top = y;
    int font = font_body();
    const char* state = offer->confirmed ? "Confirmed" : "Not confirmed";
    int sw = MeasureText(state, font);
    modal_draw_clipped_text(title, (int)c.x, (int)y, (int)(c.width - (float)sw - 2.0f * (float)font), font, MODAL_LABEL);
    if (offer->confirmed) ui_icon_draw("check", c.x + c.width - (float)sw - (float)font, y + (float)font * 0.5f, font, false, 0.0f);
    DrawText(state, (int)(c.x + c.width - (float)sw), (int)y, font, offer->confirmed ? C_CONFIRMED : C_WAITING);
    y += (float)text_line_height(font) + gap() * 0.5f;

    float cell = fminf(max_cell(), (c.width - (float)(TP_COLUMNS - 1) * gap()) / (float)TP_COLUMNS);
    int rows = (CYBERIA_TRADE_OFFER_MAX_STACKS + TP_COLUMNS - 1) / TP_COLUMNS;
    Rectangle grid = { c.x, y, (float)TP_COLUMNS * cell + (float)(TP_COLUMNS - 1) * gap(),
                       (float)rows * cell + (float)(rows - 1) * gap() };
    for (int i = 0; CYBERIA_TRADE_OFFER_MAX_STACKS > i; i++) {
        Rectangle r = { c.x + (float)(i % TP_COLUMNS) * (cell + gap()), y + (float)(i / TP_COLUMNS) * (cell + gap()),
                        cell, cell };
        if (i >= offer->count) {
            item_slot_draw_empty(r);
            continue;
        }
        ObjectLayerState shown = { .active = true, .quantity = offer->items[i].quantity };
        copy_str(shown.item_id, sizeof shown.item_id, offer->items[i].item_id);
        item_slot_draw(r, &shown, obj_layers_mgr_get());
        if (own && open) target(r, TP_TAKE_BACK, i);
    }
    if (own && open) {
        s_tp.offer_area = grid;
        s_tp.offer_shown = true;
    }
    y += grid.height + gap();

    float icon = (float)font + 6.0f;
    ol_as_ico_draw(obj_layers_mgr_get(), economy_coin_item(), (int)c.x, (int)y, (int)icon, OL_ICO_DEFAULT_DIR, 0, WHITE);
    char coins[32];
    snprintf(coins, sizeof coins, "%d coins", offer->coins);
    DrawText(coins, (int)(c.x + icon + 6.0f), (int)(y + (icon - (float)font) * 0.5f), font, MODAL_TEXT);
    if (own && open) {
        float bw = viewport_is_mobile() ? 78.0f : 96.0f;
        Rectangle set = { c.x + c.width - bw, y - 4.0f, bw, icon + 8.0f };
        button(set, "wallet", "Coins", MODAL_BUTTON, 0 < local_player_coins(), TP_COINS);
        if (0 < offer->coins)
            button((Rectangle){ set.x - bw - gap(), set.y, bw, set.height }, "close", "Remove", MODAL_BUTTON, true,
                   TP_CLEAR_COINS);
    }
    y += icon + gap() * 1.5f;
    return y - top;
}

static float draw_trade(Rectangle c, float y, const Trade* t) {
    float top = y;
    char line[96];
    float half = (c.width - gap()) * 0.5f;
    switch (t->state) {
        case TRADE_REQUESTED:
            snprintf(line, sizeof line, "Waiting for %s to answer.", t->partner_name);
            y += message(c, y, line, C_WAITING);
            button((Rectangle){ c.x, y, c.width, button_h() }, "close", "Cancel request", MODAL_BUTTON, true,
                   TP_CANCEL);
            return y + button_h() + gap() - top;
        case TRADE_INCOMING:
            snprintf(line, sizeof line, "%s wants to trade with you.", t->partner_name);
            y += message(c, y, line, MODAL_TEXT);
            button((Rectangle){ c.x, y, half, button_h() }, "close", "Decline", MODAL_BUTTON, true, TP_DECLINE);
            button((Rectangle){ c.x + half + gap(), y, half, button_h() }, "check", "Accept", MODAL_ACCEPT, true, TP_ACCEPT);
            return y + button_h() + gap() - top;
        default: break;
    }
    bool open = TRADE_OPEN == t->state;
    /* Side by side, split by a divider, when both full grids fit; else one above the other. */
    float column = (c.width - 4.0f * gap()) * 0.5f;
    if (column >= (float)TP_COLUMNS * max_cell() + (float)(TP_COLUMNS - 1) * gap()) {
        Rectangle mine = { c.x, c.y, column, c.height };
        Rectangle theirs = { c.x + column + 4.0f * gap(), c.y, column, c.height };
        float h = fmaxf(draw_offer(mine, y, "Your offer", &t->mine, true, open),
                        draw_offer(theirs, y, "Their offer", &t->theirs, false, open));
        float divider = c.x + column + 2.0f * gap();
        DrawLineEx((Vector2){ divider, y }, (Vector2){ divider, y + h - gap() }, 1.0f, C_DISABLED);
        y += h;
    } else {
        y += draw_offer(c, y, "Your offer", &t->mine, true, open);
        y += draw_offer(c, y, "Their offer", &t->theirs, false, open);
    }
    if (!open) return y + message(c, y, "Saving the trade. Nothing moves until both sides are saved.", C_WAITING) - top;
    y += message(c, y, t->mine.confirmed && !t->theirs.confirmed ? "Waiting for the other player to confirm."
                       : "Drag stacks from your bag onto your offer. Tap a stack to take it back. A change clears both confirmations.",
                 MODAL_LABEL);
    button((Rectangle){ c.x, y, half, button_h() }, "close", "Cancel", C_DANGER, true, TP_CANCEL);
    button((Rectangle){ c.x + half + gap(), y, half, button_h() }, "check", t->mine.confirmed ? "Confirmed" : "Confirm",
           MODAL_ACCEPT, !t->mine.confirmed, TP_CONFIRM);
    return y + button_h() + gap() - top;
}

float trade_panel_draw(Rectangle content, EntityID partner) {
    s_tp.button_count = 0;
    s_tp.offer_shown = false;
    s_tp.partner = partner;
    float top = content.y - ui_scroll_offset(&s_tp.scroll);
    float y = top;
    ui_scroll_begin(&s_tp.scroll);
    const Trade* t = &g_local_player.trade;
    if (trade_panel_with(partner)) {
        y += draw_trade(content, y, t);
    } else if (TRADE_NONE != t->state) {
        char line[96];
        snprintf(line, sizeof line, "You are trading with %s. Finish that trade first.", t->partner_name);
        y += message(content, y, line, C_WAITING);
    } else {
        y += message(content, y, "Trade items and coins. Both of you confirm the same offers before anything moves.",
                     MODAL_LABEL);
        bool here = game_state_has(partner, COMP_PLAYER);
        const char* label = local_player_unsaved() ? "Waiting to save" : here ? "Request trade" : "Out of sight";
        button((Rectangle){ content.x, y, content.width, button_h() }, "trade", label, C_TRADE,
               here && !local_player_unsaved(), TP_REQUEST);
        y += button_h() + gap();
    }
    ui_scroll_end(&s_tp.scroll);
    s_tp.content_h = y - top;
    return s_tp.content_h;
}

void trade_panel_tab_update(Rectangle content, bool ready, float dt) {
    ui_scroll_update(&s_tp.scroll, content, s_tp.content_h, dt);
    int x, y;
    if (!ui_scroll_take_click(&s_tp.scroll, &x, &y) || !ready) return;
    for (int i = 0; i < s_tp.button_count; i++) {
        if (ui_button_hit(s_tp.buttons[i].rect, x, y)) {
            perform(&s_tp.buttons[i]);
            return;
        }
    }
}

void trade_panel_press(Rectangle content, int mx, int my) {
    if (CheckCollisionPointRec((Vector2){ (float)mx, (float)my }, content)) ui_scroll_on_press(&s_tp.scroll, mx, my);
}

bool trade_panel_wheel(Rectangle content, float wheel_delta) {
    return ui_scroll_on_wheel(&s_tp.scroll, content, s_tp.content_h, wheel_delta);
}

Rectangle trade_panel_offer_rect(void) { return s_tp.offer_area; }

bool trade_panel_offer_at(int mx, int my) {
    return modal_interact_is_open() && s_tp.offer_shown && TRADE_OPEN == g_local_player.trade.state &&
           CheckCollisionPointRec((Vector2){ (float)mx, (float)my }, s_tp.offer_area);
}
