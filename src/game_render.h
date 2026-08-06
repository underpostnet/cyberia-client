#ifndef GAME_RENDER_H
#define GAME_RENDER_H

#include "game_state.h"
#include <raylib.h>
#include <stdbool.h>

/* Game rendering: world, entities, and HUD. Builds on render.c. */

typedef struct {
    int screen_width;
    int screen_height;
} GameRenderer;

/* Returns 0 on success, -1 on failure. */
int game_render_init(int screen_width, int screen_height);

/* Call on window resize. */
void game_render_set_screen_size(int width, int height);

void game_render_frame(void);
void game_render_cleanup(void);

/* World space — everything below draws inside the camera transform. */
void game_render_world(void);
void game_render_grid(void);
void game_render_floors(void);

/* Obstacles and portals, but not foregrounds. */
void game_render_world_objects(void);

/* Foregrounds — always above the entities. */
void game_render_foregrounds(void);

void game_render_entities(void);
void game_render_player_path(void);
void game_render_aoi_circle(void);

/* Screen space — the camera transform does not apply. */
void game_render_ui(void);

Vector2 game_render_world_to_screen(Vector2 world_pos);
Vector2 game_render_screen_to_world(Vector2 screen_pos);

/* Camera view bounds in world coordinates. */
Rectangle game_render_get_camera_bounds(void);

#endif // GAME_RENDER_H
