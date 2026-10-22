#include "market_panel.h"

#include "content/stat_contract_generated.h"
#include "item_slot.h"
#include "modal.h"
#include "modal_notification.h"
#include "net/client_event.h"
#include "ol_as_animated_ico.h"
#include "serial.h"
#include "session_gate.h"
#include "sim/local_player.h"
#include "text.h"
#include "ui_button.h"
#include "ui_rect.h"
#include "ui_scroll.h"
#include "utils.h"
#include "view/viewport.h"
#include "world/economy.h"
#include "world/market_terms.h"
#include "world/object_layers_management.h"

#include <math.h>
#include <raylib.h>
#include <stdio.h>
#include <string.h>

#define MK_ROWS CYBERIA_MARKET_PAGE

static const Color C_CARD      = {  24,  32,  50, 255 };
static const Color C_CARD_EDGE = {  58,  78, 110, 255 };
static const Color C_CARD_DARK = {   8,  12,  22, 255 };
static const Color C_BTN_OFF   = {  58,  62,  76, 255 };
static const Color C_FIXED     = { 230, 210,  90, 255 };
static const Color C_AUCTION   = { 120, 180, 240, 255 };
static const Color C_MARKET    = {  60, 130, 110, 255 };
static const Color C_WARN      = { 230, 170, 110, 255 };

typedef enum { MK_BUY, MK_AUCTIONS, MK_SELL, MK_MINE, MK_CLAIMS, MK_VIEW_COUNT } MarketPage;
static const char* MK_VIEW_LABEL[MK_VIEW_COUNT] = { "Buy", "Auctions", "Sell", "Mine", "Claims" };
/* The listings each page asks for: one mode, or every mode. */
static const char* MK_VIEW_MODE[MK_VIEW_COUNT] = { "fixed", "auction", "", "", "" };

typedef struct {
    char   listing_id[CLIENT_EVENT_ID_BYTES];
    char   seller[32];
    char   item_id[MAX_ITEM_ID_LENGTH];
    int    quantity;
    bool   auction, mine, leading;
    int    price, bid, bids, min_bid;
    double ends_at;   /* GetTime() clock */
} MarketRow;

typedef struct {
    char reason[16];
    int  coins;
    char item_id[MAX_ITEM_ID_LENGTH];
    int  quantity;
} MarketClaimRow;

static struct {
    EntityID       terminal;
    bool           requested, loaded;
    char           closed[160];   /* why the market did not open; empty while it opens */
    MarketTerms    terms;
    char           after[CLIENT_EVENT_ID_BYTES], next[CLIENT_EVENT_ID_BYTES];
    MarketRow      listings[MK_ROWS];
    int            listing_count;
    MarketRow      mine[MK_ROWS];
    int            mine_count;
    MarketClaimRow claims[MK_ROWS];
    int            claim_count;
    MarketPage     page;
    bool           auction;   /* the mode of a new listing */
    int            hours;     /* index into terms.hours */
    UIScroll       scroll;
    float          content_h;
    /* Tap targets of the last draw. */
    Rectangle      views[MK_VIEW_COUNT], modes[2], hour_chips[CYBERIA_MARKET_HOURS_MAX];
    Rectangle      actions[MK_ROWS * 2];
    int            action_rows[MK_ROWS * 2];
    int            action_count;
    Rectangle      more, first, collect;
} s_m;

/* The listing or the stack a confirmation asks about. */
static struct {
    MarketRow row;
    char      item_id[MAX_ITEM_ID_LENGTH];
    int       slot, quantity;
} s_pending;

static int   font_body(void) { return viewport_is_mobile() ? 12 : 15; }
static float slot_size(void) { return viewport_is_mobile() ? 44.0f : 56.0f; }
static float button_h(void)  { return viewport_is_mobile() ? 30.0f : 34.0f; }
static float pad(void)       { return viewport_is_mobile() ? 6.0f : 8.0f; }

/* The page switch stays above the scrolled pages. */
static float header_h(void) { return button_h() + pad(); }
static Rectangle scroll_view(Rectangle content) {
    return (Rectangle){ content.x, content.y + header_h(), content.width, content.height - header_h() };
}

bool market_panel_visible(const ActionMetadataEntry* action) {
    return NULL != action && '\0' != action->market_code[0];
}

void market_panel_reset(void) {
    ui_scroll_reset(&s_m.scroll);
    s_m.terminal = 0;
    s_m.requested = s_m.loaded = false;
    s_m.closed[0] = '\0';
    s_m.listing_count = s_m.mine_count = s_m.claim_count = 0;
    s_m.after[0] = s_m.next[0] = '\0';
    s_m.page = MK_BUY;
    s_m.content_h = 0.0f;
    s_m.action_count = 0;
}

static void read_rows(const cJSON* list, MarketRow* out, int* count) {
    *count = 0;
    double now = GetTime();
    const cJSON* row = NULL;
    cJSON_ArrayForEach(row, list) {
        if (MK_ROWS <= *count) break;
        MarketRow* r = &out[(*count)++];
        memset(r, 0, sizeof *r);
        char mode[16] = {0};
        serial_get_string(row, "listingId", r->listing_id, sizeof r->listing_id);
        serial_get_string(row, "sellerName", r->seller, sizeof r->seller);
        serial_get_string(row, "itemId", r->item_id, sizeof r->item_id);
        serial_get_string(row, "mode", mode, sizeof mode);
        r->auction  = 0 == strcmp(mode, "auction");
        r->quantity = serial_get_int_default(row, "quantity", 0);
        r->price    = serial_get_int_default(row, "price", 0);
        r->bid      = serial_get_int_default(row, "bid", 0);
        r->bids     = serial_get_int_default(row, "bids", 0);
        r->min_bid  = serial_get_int_default(row, "minBid", 0);
        r->mine     = serial_get_bool_default(row, "mine", false);
        r->leading  = serial_get_bool_default(row, "leading", false);
        r->ends_at  = now + serial_get_double_default(row, "endsInMs", 0.0) / 1000.0;
    }
}

void market_panel_view(const cJSON* payload) {
    if (s_m.terminal != serial_get_u32_default(payload, "entityId", 0)) return;
    s_m.loaded = true;
    s_m.closed[0] = '\0';
    market_terms_read(payload, &s_m.terms);
    if (s_m.hours >= s_m.terms.hour_count) s_m.hours = 0;
    if (!s_m.terms.fixed) s_m.auction = s_m.terms.auction;
    if (!s_m.terms.auction) s_m.auction = false;
    serial_get_string(payload, "after", s_m.after, sizeof s_m.after);
    serial_get_string(payload, "next", s_m.next, sizeof s_m.next);
    read_rows(serial_get_array(payload, "listings"), s_m.listings, &s_m.listing_count);
    read_rows(serial_get_array(payload, "mine"), s_m.mine, &s_m.mine_count);
    s_m.claim_count = 0;
    const cJSON* claim = NULL;
    cJSON_ArrayForEach(claim, serial_get_array(payload, "claims")) {
        if (MK_ROWS <= s_m.claim_count) break;
        MarketClaimRow* c = &s_m.claims[s_m.claim_count++];
        memset(c, 0, sizeof *c);
        serial_get_string(claim, "reason", c->reason, sizeof c->reason);
        serial_get_string(claim, "itemId", c->item_id, sizeof c->item_id);
        c->coins    = serial_get_int_default(claim, "coins", 0);
        c->quantity = serial_get_int_default(claim, "quantity", 0);
    }
}

void market_panel_refused(const char* text) {
    if (0 == s_m.terminal) return;
    s_m.loaded = true;
    copy_str(s_m.closed, sizeof s_m.closed, text);
}

static void time_left(double ends_at, char* out, size_t cap) {
    int left = (int)(ends_at - GetTime());
    if (0 >= left)        snprintf(out, cap, "Ending");
    else if (3600 <= left) snprintf(out, cap, "%dh %02dm", left / 3600, left % 3600 / 60);
    else                  snprintf(out, cap, "%dm %02ds", left / 60, left % 60);
}

/* A coin icon with a label and an amount; returns its width. */
static float coin_amount(const char* label, int amount, int x, int y, int font, Color color) {
    char text[64];
    snprintf(text, sizeof text, "%s %d", label, amount);
    float icon = (float)font + 4.0f;
    ol_as_ico_draw(obj_layers_mgr_get(), economy_coin_item(), x, y - 2, (int)icon, OL_ICO_DEFAULT_DIR, 0, WHITE);
    DrawText(text, (int)(x + icon + 4.0f), y, font, color);
    return icon + 4.0f + (float)MeasureText(text, font);
}

/* One action button; `row` is what a tap on it acts on. */
static void action_button(Rectangle r, const char* label, const char* icon, Color bg, bool enabled, int row,
                          int mx, int my) {
    UIButtonPixelRetroStyle st = {
        .bg = enabled ? bg : C_BTN_OFF, .icon_id = icon, .label = label, .font_size = font_body(),
        .text_color = enabled ? MODAL_TEXT : MODAL_LABEL, .enabled = enabled,
    };
    ui_button_pixel_retro_draw(r, &st, enabled && ui_button_hit(r, mx, my));
    if (enabled && (int)(sizeof s_m.actions / sizeof s_m.actions[0]) > s_m.action_count) {
        s_m.actions[s_m.action_count] = r;
        s_m.action_rows[s_m.action_count++] = row;
    }
}

/* The frame of a card with an item slot; returns where its text starts. */
static float card_frame(Rectangle card, const char* item_id, int quantity, Color accent, int mx, int my) {
    bool hovered = ui_button_hit(card, mx, my);
    Rectangle inner = draw_pixel_bevel(card, 0.0f, 0.0f, hovered ? ColorBrightness(C_CARD, 0.2f) : C_CARD,
                                       hovered ? ColorBrightness(C_CARD_EDGE, 0.2f) : C_CARD_EDGE, C_CARD_DARK);
    DrawRectangle((int)inner.x, (int)inner.y, 3, (int)inner.height, accent);
    ObjectLayerState shown = { .active = true, .quantity = quantity };
    copy_str(shown.item_id, sizeof shown.item_id, item_id);
    float x = card.x + pad() * 1.5f;
    item_slot_draw((Rectangle){ x, card.y + pad(), slot_size(), slot_size() }, &shown, obj_layers_mgr_get());
    return x + slot_size() + pad();
}

static float card_height(int lines) {
    float line_h = (float)text_line_height(font_body()) + 2.0f;
    return pad() * 3.0f + fmaxf(slot_size(), (float)lines * line_h) + button_h();
}

/* A note in the label color; returns its height and a gap. */
static float note(Rectangle c, float y, const char* text) {
    return (float)text_wrap(text, (int)c.x, (int)y, (int)c.width, font_body() - 1, MODAL_LABEL, false, true) + pad();
}

/* One listing: the stack, its seller, its price or bid and its time left. */
static float draw_listing(Rectangle c, float y, const MarketRow* r, int row, bool own, int mx, int my) {
    int font = font_body();
    float line_h = (float)text_line_height(font) + 2.0f;
    float h = card_height(3);
    Rectangle card = { c.x, y, c.width, h };
    float tx = card_frame(card, r->item_id, r->quantity, r->auction ? C_AUCTION : C_FIXED, mx, my);
    int tw = (int)(c.x + c.width - pad() - tx);
    char head[96];
    snprintf(head, sizeof head, "%s  -  %s", r->item_id, r->mine ? "you" : r->seller);
    modal_draw_clipped_text(head, (int)tx, (int)(y + pad()), tw, font, MODAL_TEXT);
    float line = y + pad() + line_h;
    if (!r->auction)        coin_amount("Price", r->price, (int)tx, (int)line, font - 1, C_FIXED);
    else if (0 < r->bids)   coin_amount(r->leading ? "Your bid" : "Bid", r->bid, (int)tx, (int)line, font - 1, C_AUCTION);
    else                    coin_amount("Opens at", r->price, (int)tx, (int)line, font - 1, C_AUCTION);
    line += line_h;
    char left[32], info[64];
    time_left(r->ends_at, left, sizeof left);
    if (r->auction) snprintf(info, sizeof info, "%s  -  %d %s", left, r->bids, 1 == r->bids ? "bid" : "bids");
    else            snprintf(info, sizeof info, "%s", left);
    DrawText(info, (int)tx, (int)line, font - 1, MODAL_LABEL);

    Rectangle btn = { c.x + pad(), y + h - pad() - button_h(), c.width - 2.0f * pad(), button_h() };
    char label[48];
    int coins = local_player_coins();
    if (own) {
        bool can = 0 == r->bids;
        action_button(btn, can ? "Cancel" : "Has bids", "close", C_WARN, can, row, mx, my);
    } else if (r->mine) {
        action_button(btn, "Yours", "bag", C_BTN_OFF, false, row, mx, my);
    } else if (!r->auction) {
        bool can = coins >= r->price;
        action_button(btn, can ? "Buy" : "Need coins", "trade", MODAL_ACCEPT, can, row, mx, my);
    } else if (r->leading) {
        action_button(btn, "Leading", "star", C_BTN_OFF, false, row, mx, my);
    } else {
        snprintf(label, sizeof label, "Bid %d", r->min_bid);
        bool can = coins >= r->min_bid;
        action_button(btn, can ? label : "Need coins", "points", C_AUCTION, can, row, mx, my);
    }
    return h;
}

/* The listings of one mode, with the page switch of the view. */
static float draw_listings(Rectangle c, float y, bool auction, int mx, int my) {
    float top = y;
    int shown = 0;
    for (int i = 0; i < s_m.listing_count; i++) {
        if (s_m.listings[i].auction != auction) continue;
        y += draw_listing(c, y, &s_m.listings[i], i, false, mx, my) + pad();
        shown++;
    }
    if (0 == shown) y += note(c, y, auction ? "No auction runs here now." : "Nothing is for sale at a fixed price now.");
    float w = (c.width - pad()) * 0.5f;
    if ('\0' != s_m.after[0]) {
        s_m.first = (Rectangle){ c.x, y, w, button_h() };
        action_button(s_m.first, "First page", "arrow-left", C_CARD, true, -1, mx, my);
    }
    if ('\0' != s_m.next[0]) {
        s_m.more = (Rectangle){ c.x + w + pad(), y, w, button_h() };
        action_button(s_m.more, "Next page", "arrow-right", C_CARD, true, -1, mx, my);
    }
    if ('\0' != s_m.after[0] || '\0' != s_m.next[0]) y += button_h() + pad();
    return y - top;
}

/* One stack this terminal lists. */
static float draw_sellable(Rectangle c, float y, const ObjectLayerState* stack, int row, int mx, int my) {
    int font = font_body();
    float line_h = (float)text_line_height(font) + 2.0f;
    float h = card_height(2);
    Rectangle card = { c.x, y, c.width, h };
    float tx = card_frame(card, stack->item_id, stack->quantity, C_MARKET, mx, my);
    modal_draw_clipped_text(stack->item_id, (int)tx, (int)(y + pad()), (int)(c.x + c.width - pad() - tx), font, MODAL_TEXT);
    char hold[48];
    snprintf(hold, sizeof hold, "You hold %d%s", stack->quantity, stack->active ? "  -  worn" : "");
    DrawText(hold, (int)tx, (int)(y + pad() + line_h), font - 1, MODAL_LABEL);
    Rectangle btn = { c.x + pad(), y + h - pad() - button_h(), c.width - 2.0f * pad(), button_h() };
    bool can = local_player_coins() >= s_m.terms.listing_fee && !local_player_unsaved();
    action_button(btn, can ? (s_m.auction ? "Auction" : "List") : "Need coins", "hand", C_MARKET, can, row, mx, my);
    return h;
}

/* One claim: the coins or the stack the market owes, and why. */
static float draw_claim(Rectangle c, float y, const MarketClaimRow* claim, int mx, int my) {
    int font = font_body();
    float h = pad() * 2.0f + slot_size();
    Rectangle card = { c.x, y, c.width, h };
    bool stack = 0 < claim->quantity;
    float tx = card_frame(card, stack ? claim->item_id : economy_coin_item(), stack ? claim->quantity : claim->coins,
                          C_MARKET, mx, my);
    const char* why = 0 == strcmp(claim->reason, "sale")   ? "Sold: the price, less the sale fee"
                    : 0 == strcmp(claim->reason, "won")    ? "Won at auction"
                    : 0 == strcmp(claim->reason, "outbid") ? "Outbid: your bid returns"
                                                           : "Ended unsold: it returns";
    text_wrap(why, (int)tx, (int)(y + pad()), (int)(c.x + c.width - pad() - tx), font - 1, MODAL_LABEL, false, true);
    return h;
}

/* A row of chips; returns their height. `selected` is the chosen one. */
static float chips(Rectangle c, float y, const char* const* labels, int count, int selected, Rectangle* out,
                   int mx, int my) {
    float w = (c.width - (float)(count - 1) * pad()) / (float)count;
    for (int i = 0; i < count; i++) {
        Rectangle r = { c.x + (float)i * (w + pad()), y, w, button_h() };
        UIButtonPixelRetroStyle st = {
            .bg = i == selected ? C_MARKET : C_CARD, .label = labels[i], .font_size = font_body() - 1,
            .text_color = MODAL_TEXT, .selected = i == selected, .enabled = true,
        };
        ui_button_pixel_retro_draw(r, &st, ui_button_hit(r, mx, my));
        out[i] = r;
    }
    return button_h();
}

/* What this terminal lists: every item the economy allows, or its items. */
static void listed_items(const MarketTerms* t, char* out, size_t cap) {
    if (t->by_policy) {
        snprintf(out, cap, "Lists here: every item the economy allows.");
        return;
    }
    snprintf(out, cap, "Lists here:");
    for (int i = 0; i < t->item_count; i++) {
        size_t used = strlen(out);
        snprintf(out + used, cap - used, "%s %s", 0 == i ? "" : ",", t->item_ids[i]);
    }
    if (0 == t->item_count) snprintf(out + strlen(out), cap - strlen(out), " no item.");
}

static float draw_sell(Rectangle c, float y, int mx, int my) {
    float top = y;
    const MarketTerms* t = &s_m.terms;
    if (!t->active) return note(c, y, "This market takes no new listing. What is listed runs to its end.");
    if (!market_terms_takes_listings(t)) return note(c, y, "This terminal takes no listing. Buy, bid and collect here.");
    char line[256];
    listed_items(t, line, sizeof line);
    y += note(c, y, line);
    const char* modes[2] = { "Fixed price", "Auction" };
    if (t->fixed && t->auction) y += chips(c, y, modes, 2, s_m.auction ? 1 : 0, s_m.modes, mx, my) + pad();
    else {
        s_m.modes[0] = s_m.modes[1] = (Rectangle){ 0 };
        y += note(c, y, t->fixed ? "Fixed price only." : "Auctions only.");
    }
    char hours[CYBERIA_MARKET_HOURS_MAX][16];
    const char* hour_labels[CYBERIA_MARKET_HOURS_MAX];
    for (int i = 0; i < t->hour_count; i++) {
        snprintf(hours[i], sizeof hours[i], "%dh", t->hours[i]);
        hour_labels[i] = hours[i];
    }
    y += chips(c, y, hour_labels, t->hour_count, s_m.hours, s_m.hour_chips, mx, my) + pad();
    snprintf(line, sizeof line, "Listing fee %d  -  sale fee %d  -  prices %d to %d", t->listing_fee, t->sale_fee,
             t->price_min, t->price_max);
    y += note(c, y, line);
    int shown = 0;
    for (int i = 0; i < g_local_player.inventory_count && MK_ROWS > shown; i++) {
        const ObjectLayerState* stack = &g_local_player.inventory[i];
        if (0 > stack->slot || 0 >= stack->quantity || !market_terms_lists(t, stack->item_id)) continue;
        y += draw_sellable(c, y, stack, i, mx, my) + pad();
        shown++;
    }
    if (0 == shown) y += note(c, y, "Nothing you carry can be listed here.");
    return y - top;
}

/* The name of the market and who shares it. */
static float draw_identity(Rectangle c, float y) {
    if ('\0' == s_m.terms.code[0]) return 0.0f;
    char line[160];
    if (0 == strcmp(s_m.terms.scope, "universe"))
        snprintf(line, sizeof line, "Market %s  -  shared across the universe %s", s_m.terms.code, s_m.terms.scope_key);
    else
        snprintf(line, sizeof line, "Market %s  -  this world only", s_m.terms.code);
    return note(c, y, line);
}

float market_panel_draw(Rectangle content) {
    int mx = GetMouseX(), my = GetMouseY();
    chips(content, content.y, MK_VIEW_LABEL, MK_VIEW_COUNT, s_m.page, s_m.views, mx, my);
    float top = content.y + header_h() - ui_scroll_offset(&s_m.scroll);
    float y = top;
    s_m.action_count = 0;
    s_m.more = s_m.first = s_m.collect = (Rectangle){ 0 };
    s_m.modes[0] = s_m.modes[1] = (Rectangle){ 0 };
    memset(s_m.hour_chips, 0, sizeof s_m.hour_chips);
    ui_scroll_begin(&s_m.scroll);
    if (!session_gate_account()) {
        y += note(content, y, "A market keeps what you list in your account. Sign in to buy and sell here.");
    } else if ('\0' != s_m.closed[0]) {
        y += note(content, y, s_m.closed);
    } else if (!s_m.loaded) {
        y += note(content, y, "Opening the market...");
    } else {
        y += draw_identity(content, y);
        if (MK_BUY == s_m.page || MK_AUCTIONS == s_m.page) {
            y += draw_listings(content, y, MK_AUCTIONS == s_m.page, mx, my);
        } else if (MK_SELL == s_m.page) {
            y += draw_sell(content, y, mx, my);
        } else if (MK_MINE == s_m.page) {
            for (int i = 0; i < s_m.mine_count; i++) y += draw_listing(content, y, &s_m.mine[i], i, true, mx, my) + pad();
            if (0 == s_m.mine_count) y += note(content, y, "You have no listing here.");
        } else {
            int coins = 0;
            for (int i = 0; i < s_m.claim_count; i++) {
                y += draw_claim(content, y, &s_m.claims[i], mx, my) + pad();
                coins += s_m.claims[i].coins;
            }
            if (0 == s_m.claim_count) {
                y += note(content, y, "The market owes you nothing.");
            } else {
                bool room = (double)coins <= economy_wallet_room() && !local_player_unsaved();
                s_m.collect = (Rectangle){ content.x, y, content.width, button_h() };
                action_button(s_m.collect, room ? "Collect all" : "Wallet full", "check", MODAL_ACCEPT, room, -1, mx, my);
                if (!room) s_m.collect = (Rectangle){ 0 };
                y += button_h();
            }
        }
    }
    ui_scroll_end(&s_m.scroll);
    s_m.content_h = y - top;
    return header_h() + s_m.content_h;
}

static void bought(void)    { local_player_market_buy(s_m.terminal, s_pending.row.listing_id); }
static void bid(void)       { local_player_market_bid(s_m.terminal, s_pending.row.listing_id, s_pending.row.min_bid); }
static void cancelled(void) { local_player_market_cancel(s_m.terminal, s_pending.row.listing_id); }

static void priced(const char* item_id, int price) {
    (void)item_id;
    if (s_m.hours >= s_m.terms.hour_count) return;
    local_player_market_list(s_m.terminal, s_pending.slot, s_pending.quantity, s_m.auction, price,
                             s_m.terms.hours[s_m.hours]);
}

/* The price a listing starts from: what the merchant pays for the units, within the prices of the market. */
static int suggested_price(const char* item_id, int quantity) {
    int each = economy_policy(item_id)->buyback_payout;
    int price = (0 < each ? each : 1) * quantity;
    if (price < s_m.terms.price_min) price = s_m.terms.price_min;
    return price > s_m.terms.price_max ? s_m.terms.price_max : price;
}

static void counted(const char* item_id, int quantity) {
    (void)item_id;
    s_pending.quantity = quantity;
    char message[200];
    snprintf(message, sizeof message, "%s for %d x %s. The listing fee is %d; the sale fee is %d.",
             s_m.auction ? "Opening bid" : "Price", quantity, s_pending.item_id, s_m.terms.listing_fee,
             s_m.terms.sale_fee);
    modal_notification_show_amount(s_m.auction ? "Auction" : "List", message, C_MARKET, economy_coin_item(),
                                   suggested_price(s_pending.item_id, quantity), s_m.terms.price_max,
                                   s_m.auction ? "Auction" : "List", "hand", priced);
}

/* The confirmation of a tap on the action of row. */
static void act(int row) {
    char message[200];
    double deadline = GetTime() + 30.0;
    if (MK_SELL == s_m.page) {
        const ObjectLayerState* stack = &g_local_player.inventory[row];
        copy_str(s_pending.item_id, sizeof s_pending.item_id, stack->item_id);
        s_pending.slot = stack->slot;
        if (1 < stack->quantity)
            modal_notification_show_split("List", "How many units?", C_MARKET, stack->item_id, stack->quantity, "Next",
                                          "hand", counted, NULL);
        else
            counted(stack->item_id, 1);
        return;
    }
    s_pending.row = MK_MINE == s_m.page ? s_m.mine[row] : s_m.listings[row];
    const MarketRow* r = &s_pending.row;
    if (MK_MINE == s_m.page) {
        snprintf(message, sizeof message, "Take %d x %s off the market?", r->quantity, r->item_id);
        modal_notification_show_prompt("Cancel listing", message, C_WARN, "Take back", "close", deadline, cancelled, NULL);
    } else if (!r->auction) {
        snprintf(message, sizeof message, "Buy %d x %s from %s for %d coins?", r->quantity, r->item_id, r->seller, r->price);
        modal_notification_show_prompt("Buy", message, C_FIXED, "Buy", "trade", deadline, bought, NULL);
    } else {
        snprintf(message, sizeof message,
                 "Bid %d coins on %d x %s? The market keeps them until a higher bid returns them.", r->min_bid,
                 r->quantity, r->item_id);
        modal_notification_show_prompt("Bid", message, C_AUCTION, "Bid", "points", deadline, bid, NULL);
    }
}

static bool hit(Rectangle r, int x, int y) { return 0.0f < r.width && ui_button_hit(r, x, y); }

/* Asks for the page of the view at cursor `after`. */
static void request(const char* after) { local_player_market_open(s_m.terminal, MK_VIEW_MODE[s_m.page], after); }

void market_panel_update(Rectangle content, EntityID terminal, bool ready, float dt) {
    if (terminal != s_m.terminal) {
        market_panel_reset();
        s_m.terminal = terminal;
    }
    if (!s_m.requested && session_gate_account()) {
        s_m.requested = true;
        request("");
    }
    ui_scroll_update(&s_m.scroll, scroll_view(content), s_m.content_h, dt);
    int x, y;
    if (!ui_scroll_take_click(&s_m.scroll, &x, &y) || !ready) return;
    if (MK_SELL == s_m.page) {
        for (int i = 0; i < 2; i++) {
            if (hit(s_m.modes[i], x, y)) { s_m.auction = 1 == i; return; }
        }
        for (int i = 0; i < s_m.terms.hour_count; i++) {
            if (hit(s_m.hour_chips[i], x, y)) { s_m.hours = i; return; }
        }
    }
    if (hit(s_m.more, x, y))    { request(s_m.next); return; }
    if (hit(s_m.first, x, y))   { request(""); return; }
    if (hit(s_m.collect, x, y)) { local_player_market_collect(terminal); return; }
    for (int i = 0; i < s_m.action_count; i++) {
        if (0 <= s_m.action_rows[i] && hit(s_m.actions[i], x, y)) {
            act(s_m.action_rows[i]);
            return;
        }
    }
}

void market_panel_press(Rectangle content, int mx, int my) {
    for (int i = 0; i < MK_VIEW_COUNT; i++) {
        if (hit(s_m.views[i], mx, my)) {
            s_m.page = (MarketPage)i;
            ui_scroll_reset(&s_m.scroll);
            if (s_m.requested && '\0' == s_m.closed[0]) request("");
            return;
        }
    }
    Rectangle view = scroll_view(content);
    if (CheckCollisionPointRec((Vector2){ (float)mx, (float)my }, view)) ui_scroll_on_press(&s_m.scroll, mx, my);
}

bool market_panel_wheel(Rectangle content, float wheel_delta) {
    return ui_scroll_on_wheel(&s_m.scroll, scroll_view(content), s_m.content_h, wheel_delta);
}
