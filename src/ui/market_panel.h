#ifndef CYBERIA_UI_MARKET_PANEL_H
#define CYBERIA_UI_MARKET_PANEL_H

#include <cJSON.h>
#include <raylib.h>
#include <stdbool.h>

#include "data/action_cache.h"
#include "world/world_types.h"

/* market_panel — the Market tab of modal_interact at a market terminal.
 *
 *   Buy       the fixed-price listings of the shared market, soonest end first.
 *   Auctions  its auctions: Bid the lowest bid each takes.
 *   Sell      what this terminal lists: its items or every item the economy
 *             allows, in its modes, at the durations and prices of the market.
 *   Mine      the open listings of the player there: Cancel one without bids.
 *   Claims    what the market owes the player: Collect takes it all.
 *
 * The server sends the view (a market message) with the terms of the market
 * and the rules of this terminal; a page switch asks for it again, and a fresh
 * view follows each change. Each action names a listing of that view. Nothing
 * in the bag or the wallet changes until the snapshot shows it. */

/* Whether the action opens a market. */
bool  market_panel_visible(const ActionMetadataEntry* action);

/* A new modal session starts the tab empty, at Buy. */
void  market_panel_reset(void);

/* Take a market message: the view of a terminal. */
void  market_panel_view(const cJSON* payload);

/* The market of the terminal did not open: show `text` in place of the view. */
void  market_panel_refused(const char* text);

/* Asks for the first page once, scrolls, and answers a tap once `ready`. */
void  market_panel_update(Rectangle content, EntityID terminal, bool ready, float dt);

/* Draw the tab; returns the content height. */
float market_panel_draw(Rectangle content);

void  market_panel_press(Rectangle content, int mx, int my);
bool  market_panel_wheel(Rectangle content, float wheel_delta);

#endif /* CYBERIA_UI_MARKET_PANEL_H */
