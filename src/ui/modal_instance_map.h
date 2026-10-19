#ifndef CYBERIA_UI_MODAL_INSTANCE_MAP_H
#define CYBERIA_UI_MODAL_INSTANCE_MAP_H

#include <stdbool.h>

/* modal_instance_map — the expanded map content, with two views.
 *
 * Renders inside modal_map's container: toggling the Map button morphs the
 * compact corner readout into a full-screen translucent panel (and retracts
 * it on close), so the two modes read as one integrated widget. The world
 * keeps rendering behind the translucent container.
 *
 * WORLD shows the maps of this world as the engine places them: maps that
 * share an edge form one surface. Animated portal edges and static presence
 * POIs sit on their cells.
 * UNIVERSE shows the worlds of its universe on their cells, the routes
 * between them, and the availability of each world. A world the player can
 * reach now offers TRAVEL, which leaves through the launch bridge.
 *
 * Fully independent from the gameplay renderer and camera: it owns its own
 * camera (drag pan, wheel/pinch zoom, smooth interpolation), its data layers
 * (data/instance_map_data, data/universe_map_data — engine-cyberia REST,
 * never the AOI stream) and its own selection state. The universe data loads
 * only when its view first shows.
 */

void modal_instance_map_init(void);
void modal_instance_map_cleanup(void);

bool modal_instance_map_is_open(void);
void modal_instance_map_toggle(void);
void modal_instance_map_close(void);

/* Per-frame: data polling, gesture tracking, camera interpolation. */
void modal_instance_map_update(float dt);
void modal_instance_map_draw(int screen_width, int screen_height);

/* Tap dispatch (press-time). Consumes presses inside the panel; gesture
 * resolution (pan vs select) happens on release in update. */
bool modal_instance_map_handle_click(int mx, int my);

/* Wheel zoom while the pointer hovers the panel. */
bool modal_instance_map_handle_wheel(float wheel_delta);

/* True when the pixel is covered by the open panel (input guard). */
bool modal_instance_map_covers_point(int mx, int my);

#endif /* CYBERIA_UI_MODAL_INSTANCE_MAP_H */
