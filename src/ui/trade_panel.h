#ifndef CYBERIA_UI_TRADE_PANEL_H
#define CYBERIA_UI_TRADE_PANEL_H

#include <raylib.h>
#include <stdbool.h>

#include "world/world_types.h"

/* trade_panel — the trade of the player: the Trade tab of modal_interact on
 * the other player, and the prompt that asks the player to answer a request.
 *
 *   no trade    Request trade.
 *   requested   The answer waits; Cancel.
 *   incoming    Accept or Decline. A prompt also asks when the request arrives.
 *   open        Your offer: bag stacks and coins. Drag a bag stack onto it; tap
 *               an offered stack to take it back. Their offer reads only. Each
 *               side confirms the offers it sees; a change clears both.
 *   committing  The server saves both sides at once; nothing moves until it
 *               answers.
 *
 * Every action is a request; the snapshot shows its result. */

/* Each frame: ask about a new trade request. */
void  trade_panel_update(void);

/* A new modal session or another tab starts the tab at its top and hides its
 * drop target. */
void  trade_panel_reset(void);

/* True when the Trade tab leads for player `id`: a trade with it exists. */
bool  trade_panel_with(EntityID id);

/* Scroll, and answer a tap once `ready` (the content has settled). */
void  trade_panel_tab_update(Rectangle content, bool ready, float dt);

/* Draw the trade with player `partner`; returns the content height. */
float trade_panel_draw(Rectangle content, EntityID partner);

void  trade_panel_press(Rectangle content, int mx, int my);
bool  trade_panel_wheel(Rectangle content, float wheel_delta);

/* True when a point is over the own offer of an open trade on screen: a bag
 * stack dragged there joins the offer. */
bool  trade_panel_offer_at(int mx, int my);

/* The own offer grid on screen, where a dragged stack lands. */
Rectangle trade_panel_offer_rect(void);

/* Ask how many units of the bag stack in `bag_slot` to offer, then offer them. */
void  trade_panel_offer_bag_stack(int bag_slot);

#endif /* CYBERIA_UI_TRADE_PANEL_H */
