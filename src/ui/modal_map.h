#ifndef MODAL_MAP_H
#define MODAL_MAP_H

#include "modal.h"
#include <stdbool.h>

/* modal_map — compact frameless HUD for map/connection status.
 *
 * Renders a minimal two-line corner readout: map code, position, and fps.
 * Shares modal.c's fade-in easing so it opens with the same feel as every
 * other panel, but keeps its own frameless rounded look (no border) since
 * it is a small always-on HUD, not a dismissible panel.
 */

typedef struct {
    // Display options
    bool show_connection;
    bool show_map;
    bool show_position;
    bool show_fps;

    // Cached values for smooth updates
    float cached_fps;
    double last_fps_update;

    float age;         // seconds since init, feeds modal_pop_alpha()
    Rectangle bounds;  // last drawn panel rect, screen pixels

} ModalMap;

int modal_map_init(void);
void modal_map_cleanup(void);

void modal_map_update(float delta_time);
void modal_map_draw(int screen_width, int screen_height);

/* Screen-space rect of the last drawn panel, so other UI (e.g. the Quest
 * Journal) can align beneath it. Zero-sized until the first draw. */
Rectangle modal_map_bounds(void);

#endif // MODAL_MAP_H
