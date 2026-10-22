#ifndef CYBERIA_UI_WORLD_DROP_H
#define CYBERIA_UI_WORLD_DROP_H

#include <raylib.h>
#include <stdbool.h>

#include "net/client_event.h"

/* world_drop — dropping a stack on the ground for anyone to take. A bag stack
 * dragged off the bar onto the world, the Drop zone of the personal storage
 * and the Drop button of an item land here. Each asks how many first. The
 * stack stays where it is until the snapshot shows it gone; item_drop_ack
 * reports the save, the release to the public, or a refusal. */

/* The world cell a drag released at the screen point drops on: the cell under
 * it, drawn in toward the player to the drop reach. */
Vector2 world_drop_cell(Vector2 screen);

/* Whether the player may drop item_id now, and why not. */
bool world_drop_allowed(const char* item_id, const char** reason);

/* Ask how many units of the stack in `slot` of `box` to drop on `cell` (a
 * negative cell drops at the feet), then drop them. */
void world_drop_ask(Box box, int slot, const char* item_id, int quantity, Vector2 cell);

/* The ring under a stack dragged over the world: green where it drops, red where it does not. */
void world_drop_draw_target(Vector2 cell, bool allowed);

#endif /* CYBERIA_UI_WORLD_DROP_H */
