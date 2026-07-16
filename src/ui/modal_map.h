#ifndef MODAL_MAP_H
#define MODAL_MAP_H

#include "modal.h"
#include <stdbool.h>

/* modal_map — two-mode map container.
 *
 * Compact mode: minimal two-line readout (map code, position, fps) sharing
 * modal.c's fade-in easing, left-aligned inside the top toolbar. The Map
 * toggle button lives in ui/toolbar's right-hand toggle row.
 *
 * Expanded mode: the container morphs from the compact box to the full
 * screen with an eased transition and hosts the Instance Map content
 * (ui/modal_instance_map renders inside modal_map's container geometry).
 * Toggling back retracts the container to the compact readout.
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
    Rectangle bounds;  // last drawn compact box rect, screen pixels

    // Container expansion: 0 = compact readout, 1 = full-screen container.
    bool  expanded;
    float expand_t;    // raw linear parameter, eased via accessor

} ModalMap;

int modal_map_init(void);
void modal_map_cleanup(void);

void modal_map_update(float delta_time);
void modal_map_draw(int screen_width, int screen_height);

/* Screen-space rect of the last drawn compact box. Zero-sized until the
 * first draw. Also the morph origin/target of the expanded container. */
Rectangle modal_map_bounds(void);

/* Container expansion state — driven by modal_instance_map open/close. */
void  modal_map_set_expanded(bool expanded);
bool  modal_map_is_expanded(void);
/* Eased 0..1 expansion progress (smoothstep over the transition). */
float modal_map_expand_progress(void);

#endif // MODAL_MAP_H
