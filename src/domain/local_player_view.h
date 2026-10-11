#ifndef LOCAL_PLAYER_VIEW_H
#define LOCAL_PLAYER_VIEW_H

#include <raylib.h>

#include "object_layer.h"
#include "domain/self_motion.h"

/* Presentation layer for the local player. Only the renderer and the camera
 * read it. It reads the last two fixed steps and never writes simulation
 * state. */

/* Draw alpha of the way from prev to curr, 0 to 1. Call once per render
 * frame, then read the accessors below. */
void local_player_view_update(const self_motion_t* prev, const self_motion_t* curr, float alpha);

/* On-screen position (grid units). */
Vector2 local_player_view_position(void);

/* The facing of curr; the last facing while idle. */
Direction local_player_view_direction(void);

/* MODE_WALKING while curr has a path, else MODE_IDLE. */
ObjectLayerMode local_player_view_mode(void);

/* Local player's draw footprint (grid units) scaled up so the character
 * reads slightly larger than other entities. Anchored at the same feet
 * (bottom-centre) point as the unscaled footprint so growth never shifts
 * the character's ground contact or facing offset. */
Rectangle local_player_view_scaled_footprint(float pos_x, float pos_y, float width, float height);

#endif /* LOCAL_PLAYER_VIEW_H */
