#ifndef CYBERIA_UI_DISPATCH_H
#define CYBERIA_UI_DISPATCH_H

#include <stdbool.h>

/* UI tap dispatcher.
 *
 * Single entry point that lets the input module ask the UI layer: "did the
 * UI absorb the tap?" instead of reaching into individual modules to
 * check open modals, inventory bars, etc. The implementation walks the UI
 * stack in z-order and returns the first hit.
 *
 *   ui_dispatch_tap          true  → UI absorbed the tap, world should ignore
 *   ui_dispatch_covers_point true  → screen pixel is currently covered by UI
 *                                    (used by the world hit-test guard)
 */

bool ui_dispatch_tap(int screen_x, int screen_y);
bool ui_dispatch_covers_point(int screen_x, int screen_y);

#endif /* CYBERIA_UI_DISPATCH_H */
