#ifndef MODAL_MAP_H
#define MODAL_MAP_H

#include "modal.h"
#include <stdbool.h>

/* modal_map — two-mode map container.
 *
 * Compact mode: minimal two-line corner readout (map code, position, fps)
 * sharing modal.c's fade-in easing, plus a Map toggle button styled like the
 * neighbouring fullscreen button (ui-icon "map").
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
    Rectangle map_btn_bounds; // Map toggle button rect, screen pixels

    // Container expansion: 0 = compact readout, 1 = full-screen container.
    bool  expanded;
    float expand_t;    // raw linear parameter, eased via accessor

} ModalMap;

int modal_map_init(void);
void modal_map_cleanup(void);

void modal_map_update(float delta_time);
void modal_map_draw(int screen_width, int screen_height);

/* Screen-space rect of the last drawn compact box, so other UI (e.g. the
 * Quest Journal) can align beneath it. Zero-sized until the first draw.
 * Also the morph origin/target of the expanded container. */
Rectangle modal_map_bounds(void);

/* Map toggle button: expands/retracts the Instance Map container. Returns
 * true when the tap hit the button (works both to open and to close). */
bool modal_map_handle_expand_click(int mx, int my);

/* Container expansion state — driven by modal_instance_map open/close. */
void  modal_map_set_expanded(bool expanded);
bool  modal_map_is_expanded(void);
/* Eased 0..1 expansion progress (smoothstep over the transition). */
float modal_map_expand_progress(void);

#endif // MODAL_MAP_H
