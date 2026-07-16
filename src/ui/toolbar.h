#ifndef CYBERIA_UI_TOOLBAR_H
#define CYBERIA_UI_TOOLBAR_H

#include <stdbool.h>

/* toolbar — top HUD strip spanning the viewport width.
 *
 * Hosts the persistent HUD controls: the compact map readout (ui/modal_map,
 * left-aligned) and the right-aligned toggle row [quest][map][fullscreen].
 * Every panel that anchors to the top of the screen offsets itself below
 * toolbar_height().
 */

#define TOOLBAR_H          48.0f
#define TOOLBAR_BTN_SIZE   32.0f
#define TOOLBAR_BTN_MARGIN 8.0f

float toolbar_height(void);
void  toolbar_draw(int screen_width);
/* Toggle-row taps: quest journal, instance map, fullscreen. */
bool  toolbar_handle_click(int mx, int my);
/* True anywhere on the strip (input guard for the world beneath). */
bool  toolbar_covers_point(int mx, int my);

#endif /* CYBERIA_UI_TOOLBAR_H */
