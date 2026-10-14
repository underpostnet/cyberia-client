#ifndef CYBERIA_UI_PLAYER_PANEL_H
#define CYBERIA_UI_PLAYER_PANEL_H

#include <raylib.h>
#include <stdbool.h>

#include "world/world_types.h"

/* player_panel — the player tabs of modal_interact.
 *
 *   Profile  who a player is. Another player shows only public data from the
 *            snapshot, and only while it is in view, with the party actions
 *            the server allows. The own profile adds the private block: XP,
 *            the wallet, and the fill of the bag and the personal storage.
 *   Party    the own party: the members, the invites that wait for an
 *            answer, and leave, remove and disband.
 *
 * Every action is a request; the next snapshot shows its result. */

typedef enum {
    PLAYER_VIEW_PROFILE,
    PLAYER_VIEW_PARTY,
    PLAYER_VIEW_COUNT,
} PlayerView;

/* A new session starts every view at its top. */
void  player_panel_reset(void);

/* Scroll, and answer a tap once `ready` (the content has settled). */
void  player_panel_update(PlayerView view, Rectangle content, bool ready, float dt);

/* Draw the view of player `player_id`; returns the content height. */
float player_panel_draw(PlayerView view, Rectangle content, EntityID player_id);

void  player_panel_press(PlayerView view, Rectangle content, int mx, int my);
bool  player_panel_wheel(PlayerView view, Rectangle content, float wheel_delta);

#endif /* CYBERIA_UI_PLAYER_PANEL_H */
