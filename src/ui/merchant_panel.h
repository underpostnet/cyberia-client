#ifndef CYBERIA_UI_MERCHANT_PANEL_H
#define CYBERIA_UI_MERCHANT_PANEL_H

#include <raylib.h>
#include <stdbool.h>

#include "data/action_cache.h"
#include "net/client_event.h"
#include "world/world_types.h"

/* merchant_panel — the Buyback and Salvage tabs of modal_interact at a bot
 * whose action buys back or burns items.
 *
 *   Buyback   each item the merchant buys and the item policy lets it buy:
 *             its payout per unit and how many the player holds. Sell asks
 *             how many, up to what the wallet still takes.
 *   Salvage   each item the terminal burns and the item policy lets burn: its
 *             fee per unit and what one burned unit salvages. Burn asks how
 *             many, up to what the wallet pays.
 *
 * A card shows the amounts of the economy the server sent (world/economy.h).
 * Sell and Burn ask the server for a quote first; the quantity prompt shows
 * the quoted price, and the confirm names the quote. A sale or a burn
 * changes nothing until the snapshot shows it. */

typedef enum { MERCHANT_BUYBACK, MERCHANT_BURN, MERCHANT_MODE_COUNT } MerchantMode;

/* The answer of the server to a quote request. */
typedef struct {
    char quote_id[CLIENT_EVENT_ID_BYTES];
    char item_id[MAX_ITEM_ID_LENGTH];
    bool burn;
    int  unit;           /* coins per unit: the payout or the fee */
    int  max_quantity;   /* the stack, and what the wallet takes or pays */
} MerchantQuote;

/* Open the quantity prompt of a quote. */
void  merchant_panel_quoted(const MerchantQuote* quote);

/* The items of the action the policy lets take part; 0 hides the tab. */
int   merchant_panel_count(const ActionMetadataEntry* action, MerchantMode mode);

/* A new modal session starts every tab at its top. */
void  merchant_panel_reset(void);

/* Scroll, and answer a tap once `ready` (the content has settled). */
void  merchant_panel_update(MerchantMode mode, Rectangle content, bool ready, float dt);

/* Draw the tab of the terminal `terminal`; returns the content height. */
float merchant_panel_draw(MerchantMode mode, Rectangle content, EntityID terminal, const ActionMetadataEntry* action);

void  merchant_panel_press(MerchantMode mode, Rectangle content, int mx, int my);
bool  merchant_panel_wheel(MerchantMode mode, Rectangle content, float wheel_delta);

#endif /* CYBERIA_UI_MERCHANT_PANEL_H */
