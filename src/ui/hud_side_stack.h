#ifndef CYBERIA_UI_HUD_SIDE_STACK_H
#define CYBERIA_UI_HUD_SIDE_STACK_H

#include <raylib.h>

/* Right-hand HUD column between the toolbar and the inventory bar: the
 * minimap on top, the quest journal under it, the chat panel at the bottom.
 * Layout only; the panels draw themselves inside the slots this module hands
 * them. */

#define HUD_SIDE_STACK_W        300.0f
#define HUD_SIDE_STACK_GAP        4.0f   /* between two slots               */
#define HUD_SIDE_STACK_MARGIN     8.0f   /* kept above the inventory bar    */
/* Room the journal keeps when the map is open too; the map gives way down to
 * half of the column before the journal is squeezed. */
#define HUD_SIDE_STACK_JOURNAL_RESERVE 200.0f
/* Part of the column the chat may take when another panel is open. */
#define HUD_SIDE_STACK_CHAT_SHARE 0.55f

/* The minimap slot: its full square when alone, shorter when another panel
 * is open and the column is short. Zero-height when the minimap is hidden. */
Rectangle hud_side_stack_map_slot(void);

/* The journal slot under the minimap, at most `desired_h` tall, clipped to
 * the room left above the chat. Zero-height when the journal is hidden. */
Rectangle hud_side_stack_journal_slot(float desired_h);

/* The chat slot at the bottom of the column, at most `desired_h` tall.
 * Zero-height when the chat is hidden. */
Rectangle hud_side_stack_chat_slot(float desired_h);

#endif /* CYBERIA_UI_HUD_SIDE_STACK_H */
