#ifndef CYBERIA_DOMAIN_CAMERA_H
#define CYBERIA_DOMAIN_CAMERA_H

#include <raylib.h>

/* Camera module.
 *
 * Owns the Camera2D state and the smoothing logic that follows the local
 * player. Strictly client-side presentation — the simulation never reads or
 * writes camera fields. The local player's predicted position is fetched
 * via prediction_self_position().
 */

void     camera_init(int screen_width, int screen_height);
void     camera_resize(int screen_width, int screen_height);
void     camera_set_zoom(float zoom);
float    camera_zoom(void);
void     camera_zoom_by(float factor);
void     camera_on_tick(float frame_dt);
Camera2D camera_get(void);

#endif /* CYBERIA_DOMAIN_CAMERA_H */
