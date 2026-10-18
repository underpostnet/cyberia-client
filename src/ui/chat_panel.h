#ifndef CYBERIA_UI_CHAT_PANEL_H
#define CYBERIA_UI_CHAT_PANEL_H

#include <stdbool.h>

#include "world/world_types.h"

/* chat_panel — the HUD chat, the one place chat shows and is written: the
 * world and party channels and the direct conversations, in the whole
 * right-hand column (ui/hud_side_stack.h). A conversation stays open when its
 * peer leaves view. It opens no modal, so the player keeps playing beside it.
 * Its text input shows only while no modal covers the world, and keys typed
 * into it never reach the game. Hidden by default; the toolbar's chat button
 * toggles it. */

void  chat_panel_toggle(void);
bool  chat_panel_is_visible(void);

void  chat_panel_update(float dt);
void  chat_panel_draw(void);
bool  chat_panel_handle_click(int mx, int my);
bool  chat_panel_handle_wheel(float wheel_delta);
bool  chat_panel_covers_point(int mx, int my);

/* Shows the direct conversation with a player on this server, named and
 * drawn by its identity icon. The social panel gives way to it. */
void  chat_panel_open_direct(EntityID peer, const char* name);

/* Unread lines of every conversation. */
int   chat_panel_unread(void);

#endif /* CYBERIA_UI_CHAT_PANEL_H */
