#ifndef CYBERIA_UI_SOCIAL_PANEL_H
#define CYBERIA_UI_SOCIAL_PANEL_H

#include <stdbool.h>

/* social_panel — the Social HUD panel: the friends of the account, its friend
 * requests and the party, in the whole right-hand column
 * (ui/hud_side_stack.h). The minimap, the journal and the chat give way while
 * it shows.
 * It opens no modal, so the player keeps playing beside it. Hidden by
 * default; the toolbar's social button toggles it and counts its news. */

/* Opens on the news: friend requests first, then party invites. */
void social_panel_toggle(void);
void social_panel_close(void);
bool social_panel_is_visible(void);

void social_panel_update(float dt);
void social_panel_draw(void);
bool social_panel_handle_click(int mx, int my);
bool social_panel_handle_wheel(float wheel_delta);
bool social_panel_covers_point(int mx, int my);

#endif /* CYBERIA_UI_SOCIAL_PANEL_H */
