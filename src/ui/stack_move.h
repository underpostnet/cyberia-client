#ifndef CYBERIA_UI_STACK_MOVE_H
#define CYBERIA_UI_STACK_MOVE_H

#include <raylib.h>
#include <stdbool.h>

#include "sim/container_ops.h"

/* stack_move — the one way a dropped stack lands, in the bag, the personal
 * storage and the vaults alike. An empty slot takes the units, a stack of the
 * same item takes them on top, and another stack of the same box trades places
 * with a whole stack. When a split is possible, the quantity picker asks how
 * many first; Cancel leaves both places as they were. */

/* Drop the stack of one place on another. The units slide into their slot
 * from `origin`, the screen rect of the release, or from their stack when the
 * picker asked. False when the move is refused. */
bool stack_move_drop(Box from, int from_slot, Box to, int to_slot, Rectangle origin);

#endif /* CYBERIA_UI_STACK_MOVE_H */
