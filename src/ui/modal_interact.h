/**
 * modal_interact — Raylib-native intermediate interaction modal.
 *
 * Tapping an interaction bubble opens this modal BEFORE any dialogue or the
 * JS chat/profile overlay. It is the general-purpose entry point for entity
 * interaction:
 *
 *   [ Talk ]            visible only when the entity has talk / quest-talk
 *                       dialogue. Opens modal_dialogue (player frozen).
 *   [ Chat / Profile ]  always visible. Opens the JS overlay (no freeze).
 *
 * Dismisses on the close button or a tap outside with no side effects.
 */

#ifndef MODAL_INTERACT_H
#define MODAL_INTERACT_H

#include <raylib.h>
#include <stdbool.h>

void modal_interact_init(void);

/* dialogue_item_id may be empty; has_talk gates the Talk tab. border tints
 * the header to match the entity's status indicator. */
void modal_interact_open(const char* entity_id, const char* display_name,
                         const char* dialogue_item_id, bool has_talk,
                         bool is_player, bool is_self, Color border);

void modal_interact_close(void);
bool modal_interact_is_open(void);
void modal_interact_update(float dt);
void modal_interact_draw(void);
bool modal_interact_handle_click(int mx, int my);

#endif /* MODAL_INTERACT_H */
