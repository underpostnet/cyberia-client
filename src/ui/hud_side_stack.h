#ifndef CYBERIA_UI_HUD_SIDE_STACK_H
#define CYBERIA_UI_HUD_SIDE_STACK_H

#include <raylib.h>

/* Right-hand HUD column between the toolbar and the inventory bar. The
 * minimap and the quest journal share it: the minimap on top, the journal
 * under it. The chat panel or the social panel takes the whole column while it
 * shows, and the social panel covers the chat. Layout only; the panels draw
 * themselves inside the slots this module hands them, and a zero-height slot
 * draws nothing. */

#define HUD_SIDE_STACK_W        300.0f
#define HUD_SIDE_STACK_GAP        4.0f   /* between two slots               */
#define HUD_SIDE_STACK_MARGIN     8.0f   /* kept above the inventory bar    */
/* Room the journal keeps when the map is open too; the map gives way down to
 * half of the column before the journal is squeezed. */
#define HUD_SIDE_STACK_JOURNAL_RESERVE 200.0f
/* A shorter map slot shows too little map: the map gives way. */
#define HUD_SIDE_STACK_MAP_MIN   100.0f

/* The minimap slot: its full square when alone, shorter when the journal is
 * open and the column is short. Zero-height when the minimap is hidden or
 * covered. */
Rectangle hud_side_stack_map_slot(void);

/* The journal slot under the minimap, at most `desired_h` tall, clipped to
 * the room left in the column. Zero-height when the journal is hidden or
 * covered. */
Rectangle hud_side_stack_journal_slot(float desired_h);

/* The chat slot: the whole column. Zero-height when the chat is hidden or
 * covered. */
Rectangle hud_side_stack_chat_slot(void);

/* The social slot: the whole column. Zero-height when the social panel is
 * hidden. */
Rectangle hud_side_stack_social_slot(void);

#endif /* CYBERIA_UI_HUD_SIDE_STACK_H */
