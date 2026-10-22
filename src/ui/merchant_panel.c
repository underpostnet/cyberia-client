#include "merchant_panel.h"

#include "item_slot.h"
#include "modal.h"
#include "modal_notification.h"
#include "ol_as_animated_ico.h"
#include "sim/local_player.h"
#include "text.h"
#include "ui_button.h"
#include "ui_rect.h"
#include "ui_scroll.h"
#include "utils.h"
#include "view/viewport.h"
#include "world/economy.h"
#include "world/object_layers_management.h"

#include <math.h>
#include <raylib.h>
#include <stdio.h>
#include <string.h>

#define MP_CARDS_MAX ACTION_CACHE_TRADE_MAX

static const Color C_CARD      = {  24,  32,  50, 255 };
static const Color C_CARD_EDGE = {  58,  78, 110, 255 };
static const Color C_CARD_DARK = {   8,  12,  22, 255 };
static const Color C_BTN_OFF   = {  58,  62,  76, 255 };
static const Color C_BURN      = { 168,  70,  40, 255 };
static const Color C_PAYOUT    = { 230, 210,  90, 255 };
static const Color C_FEE       = { 230, 170, 110, 255 };

static struct {
    UIScroll  scroll;
    float     content_h;
    Rectangle buttons[MP_CARDS_MAX];
    char      items[MP_CARDS_MAX][MAX_ITEM_ID_LENGTH];
    int       count;
} s_tabs[MERCHANT_MODE_COUNT];

/* The terminal on screen, and the quote the quantity prompt confirms. */
static struct {
    EntityID      terminal;
    MerchantQuote quote;
} s_pending;

static int   font_body(void)   { return viewport_is_mobile() ? 12 : 15; }
static float slot_size(void)   { return viewport_is_mobile() ? 44.0f : 56.0f; }
static float button_h(void)    { return viewport_is_mobile() ? 30.0f : 34.0f; }
static float pad(void)         { return viewport_is_mobile() ? 6.0f : 8.0f; }

/* Whether the policy lets item_id take part in the operation of `mode`: a burn,
 * or a buyback that pays at least one coin. */
static bool policy_allows(const char* item_id, MerchantMode mode) {
    const ItemPolicy* policy = economy_policy(item_id);
    return MERCHANT_BURN == mode ? policy->burn : policy->buyback && 0 < policy->buyback_payout;
}

int merchant_panel_count(const ActionMetadataEntry* action, MerchantMode mode) {
    if (NULL == action) return 0;
    int listed = MERCHANT_BURN == mode ? action->burn_count : action->buyback_count;
    int count = 0;
    for (int i = 0; i < listed; i++) {
        const char* item_id = MERCHANT_BURN == mode ? action->burn_items[i] : action->buyback_items[i];
        count += policy_allows(item_id, mode);
    }
    return count;
}

void merchant_panel_reset(void) {
    for (int m = 0; MERCHANT_MODE_COUNT > m; m++) {
        ui_scroll_reset(&s_tabs[m].scroll);
        s_tabs[m].content_h = 0.0f;
        s_tabs[m].count = 0;
    }
}

/* The bag slot of the largest stack of item_id the player can part with: an
 * unworn one first. -1 when it holds none. */
static int bag_stack_of(const char* item_id) {
    int best = -1;
    for (int i = 0; i < g_local_player.inventory_count; i++) {
        const ObjectLayerState* s = &g_local_player.inventory[i];
        if (0 > s->slot || 0 != strcmp(s->item_id, item_id)) continue;
        if (0 > best) { best = i; continue; }
        const ObjectLayerState* b = &g_local_player.inventory[best];
        if ((b->active && !s->active) || (b->active == s->active && s->quantity > b->quantity)) best = i;
    }
    return best;
}

/* How many units one request may cover: the stack, and the wallet. */
static int max_units(const ObjectLayerState* stack, bool burn, int unit) {
    int max = stack->quantity;
    if (0 < unit) {
        double fit = burn ? (double)local_player_coins() / unit : economy_wallet_room() / unit;
        if (fit < max) max = (int)fit;
    }
    return max;
}

static void confirmed(const char* item_id, int quantity) {
    (void)item_id;
    local_player_confirm_quote(s_pending.quote.burn, s_pending.quote.quote_id, quantity);
}

static void request(EntityID terminal, MerchantMode mode, const char* item_id) {
    int index = bag_stack_of(item_id);
    if (0 <= index) local_player_request_quote(terminal, MERCHANT_BURN == mode, g_local_player.inventory[index].slot);
}

void merchant_panel_quoted(const MerchantQuote* quote) {
    if (1 > quote->max_quantity) return;
    s_pending.quote = *quote;
    int unit = quote->unit;
    const char* coins = 1 == unit ? "coin" : "coins";
    char message[96];
    if (quote->burn) snprintf(message, sizeof message, "Burn for %d %s each. It cannot come back.", unit, coins);
    else             snprintf(message, sizeof message, "The merchant pays %d %s each.", unit, coins);
    modal_notification_show_split_priced(quote->burn ? "Burn" : "Sell", message, quote->burn ? C_FEE : C_PAYOUT,
                                         quote->item_id, quote->max_quantity, economy_coin_item(), unit,
                                         quote->burn ? "Burn" : "Sell", quote->burn ? "burn" : "buyback", confirmed);
}

/* A coin icon with an amount; returns its width. */
static float coin_amount(const char* label, int amount, int x, int y, int font, Color color) {
    char text[48];
    snprintf(text, sizeof text, "%s %d", label, amount);
    float icon = (float)font + 4.0f;
    ol_as_ico_draw(obj_layers_mgr_get(), economy_coin_item(), x, y - 2, (int)icon, OL_ICO_DEFAULT_DIR, 0, WHITE);
    DrawText(text, (int)(x + icon + 4.0f), y, font, color);
    return icon + 4.0f + (float)MeasureText(text, font);
}

/* One card: the item, what the operation pays or charges, what a burn salvages,
 * how many the player holds, and the action. Returns its height. */
static float draw_card(MerchantMode mode, Rectangle c, float y, const char* item_id, int index, int mx, int my) {
    bool burn = MERCHANT_BURN == mode;
    const ItemPolicy* policy = economy_policy(item_id);
    int unit = burn ? policy->burn_coin_fee : policy->buyback_payout;
    int held_index = bag_stack_of(item_id);
    const ObjectLayerState* stack = 0 <= held_index ? &g_local_player.inventory[held_index] : NULL;
    int held = local_player_item_quantity(item_id);
    int font = font_body();
    float slot = slot_size();
    float salvage_h = burn && 0 < policy->salvage_count ? slot * 0.6f + pad() : 0.0f;
    float line_h = (float)text_line_height(font) + 2.0f;
    float head = fmaxf(slot, 3.0f * line_h);
    float h = pad() * 2.0f + head + salvage_h + pad() + button_h();
    Rectangle card = { c.x, y, c.width, h };
    bool hovered = ui_button_hit(card, mx, my);
    Rectangle inner = draw_pixel_bevel(card, 0.0f, 0.0f, hovered ? ColorBrightness(C_CARD, 0.2f) : C_CARD,
                                       hovered ? ColorBrightness(C_CARD_EDGE, 0.2f) : C_CARD_EDGE, C_CARD_DARK);
    DrawRectangle((int)inner.x, (int)inner.y, 3, (int)inner.height, burn ? C_BURN : C_PAYOUT);

    ObjectLayerState shown = { .active = true, .quantity = held };
    copy_str(shown.item_id, sizeof shown.item_id, item_id);
    float x = c.x + pad() * 1.5f;
    item_slot_draw((Rectangle){ x, y + pad(), slot, slot }, &shown, obj_layers_mgr_get());
    float tx = x + slot + pad();
    int tw = (int)(c.x + c.width - pad() - tx);
    modal_draw_clipped_text(item_id, (int)tx, (int)(y + pad()), tw, font, MODAL_TEXT);
    float line = y + pad() + line_h;
    coin_amount(burn ? "Fee each" : "Pays each", unit, (int)tx, (int)line, font - 1, burn ? C_FEE : C_PAYOUT);
    line += line_h;
    char hold[48];
    snprintf(hold, sizeof hold, "You hold %d", held);
    DrawText(hold, (int)tx, (int)line, font - 1, 0 < held ? MODAL_LABEL : C_FEE);

    float row = y + pad() + head + pad();
    if (0 < salvage_h) {
        DrawText("Salvage", (int)x, (int)(row + (slot * 0.6f - (float)font) * 0.5f), font - 1, MODAL_LABEL);
        float sx = x + (float)MeasureText("Salvage", font - 1) + pad();
        for (int i = 0; i < policy->salvage_count; i++) {
            ObjectLayerState out = { .active = true, .quantity = policy->salvage[i].qty };
            copy_str(out.item_id, sizeof out.item_id, policy->salvage[i].item_id);
            item_slot_draw((Rectangle){ sx, row, slot * 0.6f, slot * 0.6f }, &out, obj_layers_mgr_get());
            sx += slot * 0.6f + pad() * 0.5f;
        }
        row += salvage_h;
    }

    bool fits = true;
    for (int i = 0; burn && i < policy->salvage_count; i++) fits = fits && local_player_inventory_has_room(policy->salvage[i].item_id);
    bool trading = local_player_trading();
    bool enabled = NULL != stack && 0 < max_units(stack, burn, unit) && fits && !trading && !local_player_unsaved();
    const char* label = NULL == stack            ? "None held"
                      : trading                  ? "Trade open"
                      : local_player_unsaved()   ? "Waiting to save"
                      : !fits                    ? "Bag full"
                      : !enabled                 ? (burn ? "Need coins" : "Wallet full")
                      : burn                     ? "Burn"
                                                 : "Sell";
    Rectangle btn = { c.x + pad(), row, c.width - 2.0f * pad(), button_h() };
    UIButtonPixelRetroStyle st = {
        .bg = enabled ? (burn ? C_BURN : MODAL_ACCEPT) : C_BTN_OFF,
        .icon_id = burn ? "burn" : "buyback",
        .label = label,
        .font_size = font,
        .text_color = enabled ? MODAL_TEXT : MODAL_LABEL,
        .enabled = enabled,
    };
    ui_button_pixel_retro_draw(btn, &st, enabled && ui_button_hit(btn, mx, my));
    s_tabs[mode].buttons[index] = enabled ? btn : (Rectangle){ 0 };
    return h;
}

float merchant_panel_draw(MerchantMode mode, Rectangle content, EntityID terminal, const ActionMetadataEntry* action) {
    s_pending.terminal = terminal;
    int mx = GetMouseX(), my = GetMouseY();
    float top = content.y - ui_scroll_offset(&s_tabs[mode].scroll);
    float y = top;
    ui_scroll_begin(&s_tabs[mode].scroll);
    y += (float)text_wrap(MERCHANT_BURN == mode ? "A burn destroys the units for good and returns what they salvage."
                                                 : "The merchant pays coins for these items.",
                          (int)content.x, (int)y, (int)content.width, font_body() - 1, MODAL_LABEL, false, true) + pad();
    s_tabs[mode].count = 0;
    int listed = NULL == action ? 0 : MERCHANT_BURN == mode ? action->burn_count : action->buyback_count;
    for (int i = 0; i < listed && MP_CARDS_MAX > s_tabs[mode].count; i++) {
        const char* item_id = MERCHANT_BURN == mode ? action->burn_items[i] : action->buyback_items[i];
        if (!policy_allows(item_id, mode)) continue;
        int index = s_tabs[mode].count++;
        copy_str(s_tabs[mode].items[index], MAX_ITEM_ID_LENGTH, item_id);
        y += draw_card(mode, content, y, item_id, index, mx, my) + pad();
    }
    if (local_player_trading())
        y += (float)text_wrap("Finish or cancel your trade first.", (int)content.x, (int)y, (int)content.width,
                              font_body() - 1, C_FEE, false, true);
    ui_scroll_end(&s_tabs[mode].scroll);
    s_tabs[mode].content_h = y - top;
    return s_tabs[mode].content_h;
}

void merchant_panel_update(MerchantMode mode, Rectangle content, bool ready, float dt) {
    ui_scroll_update(&s_tabs[mode].scroll, content, s_tabs[mode].content_h, dt);
    int x, y;
    if (!ui_scroll_take_click(&s_tabs[mode].scroll, &x, &y) || !ready) return;
    for (int i = 0; i < s_tabs[mode].count; i++) {
        if (0.0f < s_tabs[mode].buttons[i].width && ui_button_hit(s_tabs[mode].buttons[i], x, y)) {
            request(s_pending.terminal, mode, s_tabs[mode].items[i]);
            return;
        }
    }
}

void merchant_panel_press(MerchantMode mode, Rectangle content, int mx, int my) {
    if (CheckCollisionPointRec((Vector2){ (float)mx, (float)my }, content)) ui_scroll_on_press(&s_tabs[mode].scroll, mx, my);
}

bool merchant_panel_wheel(MerchantMode mode, Rectangle content, float wheel_delta) {
    return ui_scroll_on_wheel(&s_tabs[mode].scroll, content, s_tabs[mode].content_h, wheel_delta);
}
