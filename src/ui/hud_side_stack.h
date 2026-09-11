#ifndef CYBERIA_UI_HUD_SIDE_STACK_H
#define CYBERIA_UI_HUD_SIDE_STACK_H

#include <raylib.h>

/* Right-hand HUD column: the minimap above the quest journal, both stacked
 * between the toolbar and the inventory bar. Layout only; the two panels draw
 * themselves inside the slots this module hands them. */

#define HUD_SIDE_STACK_W        300.0f
#define HUD_SIDE_STACK_GAP        4.0f   /* between the two slots           */
#define HUD_SIDE_STACK_MARGIN     8.0f   /* kept above the inventory bar    */
/* Room the journal keeps when both are open; the map gives way down to half
 * of the column before the journal is squeezed. */
#define HUD_SIDE_STACK_JOURNAL_RESERVE 200.0f

/* Vertical room of the whole column this frame. */
float hud_side_stack_available(void);

/* The minimap slot: its full square when alone, shorter when the journal is
 * open and the column is short. Zero-height when the minimap is hidden. */
Rectangle hud_side_stack_map_slot(void);

/* The journal slot under the minimap, at most `desired_h` tall, clipped to
 * the room left. Zero-height when the journal is hidden. */
Rectangle hud_side_stack_journal_slot(float desired_h);

#endif /* CYBERIA_UI_HUD_SIDE_STACK_H */
