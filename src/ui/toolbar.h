#ifndef CYBERIA_UI_TOOLBAR_H
#define CYBERIA_UI_TOOLBAR_H

#include <stdbool.h>

/* toolbar — top HUD strip spanning the viewport width.
 *
 * Hosts the persistent HUD controls: the pinned hide/show toggle in the
 * top-left corner, the compact map readout beside it (ui/modal_map), and the
 * right-aligned toggle row [quest][map][fullscreen]. The hide toggle slides
 * the whole strip up to clear the screen and stays on top to bring it back.
 *
 * Every panel that anchors to the top of the screen offsets itself below
 * toolbar_height(), which animates to 0 while the strip is hidden — the
 * panels reclaim the space dynamically.
 */

#define TOOLBAR_H              48.0f
#define TOOLBAR_BTN_SIZE       32.0f
#define TOOLBAR_BTN_MARGIN     8.0f
#define TOGGLE_EDGE_MARGIN      3.0f   /* internal toggle edge margin       */
#define TOOLBAR_TOGGLE_GAP    12.0f   /* gap after toggle for header text  */

/* Right edge of the pinned top-left toggle button. Modals offset their
 * header text past this so the toggle is never covered. */
static inline float toolbar_toggle_right(void) {
    return TOGGLE_EDGE_MARGIN + (TOOLBAR_H - 2.0f * TOGGLE_EDGE_MARGIN) + TOOLBAR_TOGGLE_GAP;
}

/* Effective strip height this frame: TOOLBAR_H → 0 while sliding hidden. */
float toolbar_height(void);
/* Vertical offset of the sliding strip (0 → -TOOLBAR_H). Content drawn on
 * the strip (the map readout) rides this. */
float toolbar_offset_y(void);
bool  toolbar_is_hidden(void);

void  toolbar_draw(int screen_width);
/* Toggle-row taps (quest / map / fullscreen / hide) + the map readout. */
bool  toolbar_handle_click(int mx, int my);
/* True anywhere on the visible strip and on the always-on hide toggle. */
bool  toolbar_covers_point(int mx, int my);

#endif /* CYBERIA_UI_TOOLBAR_H */
