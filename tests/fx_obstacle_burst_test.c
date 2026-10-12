#include "fx/fx_obstacle_burst.h"
#include "fx/fx_shapes.h"
#include "game_state.h"
#include "world_types.h"

#include <assert.h>
#include <raylib.h>
#include <stdio.h>
#include <string.h>

GameState g_game_state;
const Color FX_SPARK_GRAY = { 168, 168, 168, 255 };
static int sparks;
static float min_x, max_x, min_y, max_y, largest;

float world_cell_size(void) { return 32.0f; }
void fx_shape_spark_shaded(float x, float y, float size, Color body) {
    assert(3.0f <= size && 255 == body.a);
    assert(body.r == body.g && body.g == body.b && 128 > body.r);
    if (min_x > x) min_x = x;
    if (max_x < x) max_x = x;
    if (min_y > y) min_y = y;
    if (max_y < y) max_y = y;
    if (largest < size) largest = size;
    sparks++;
}

static void draw(void) {
    sparks = 0;
    min_x = min_y = 1e9f;
    max_x = max_y = -1e9f;
    largest = 0.0f;
    fx_obstacle_burst_draw();
}

int main(void) {
    WorldObject* obstacle = &g_game_state.obstacles[0];
    obstacle->id = 26;
    obstacle->pos = (Vector2){ 10.0f, 10.0f };
    obstacle->dims = (Vector2){ 2.0f, 2.0f };
    obstacle->max_life = obstacle->life = 10.0f;
    g_game_state.obstacle_count = 1;

    fx_obstacle_burst_reset();
    fx_obstacle_burst_snapshot();
    draw();
    assert(0 == sparks);

    obstacle->life = 0.0f;
    fx_obstacle_burst_snapshot();
    draw();
    assert(24 == sparks);
    const float birth_size = largest;
    fx_obstacle_burst_snapshot();
    draw();
    assert(24 == sparks);

    fx_obstacle_burst_update(0.2f);
    draw();
    const float center = 11.0f * 32.0f;
    assert(min_x < center && max_x > center && min_y < center && max_y > center);
    assert(birth_size > largest);
    fx_obstacle_burst_update(0.6f);
    draw();
    assert(0 == sparks);

    obstacle->life = obstacle->max_life;
    fx_obstacle_burst_snapshot();
    draw();
    assert(24 == sparks);
    fx_obstacle_burst_update(0.8f);
    fx_obstacle_burst_snapshot();
    draw();
    assert(0 == sparks);

    /* An obstacle entering the area of interest starts no burst. */
    g_game_state.obstacle_count = 0;
    fx_obstacle_burst_snapshot();
    g_game_state.obstacle_count = 1;
    obstacle->life = 0.0f;
    fx_obstacle_burst_snapshot();
    draw();
    assert(0 == sparks);

    /* Reset clears bursts and transition history between maps. */
    obstacle->life = obstacle->max_life;
    fx_obstacle_burst_snapshot();
    fx_obstacle_burst_reset();
    fx_obstacle_burst_snapshot();
    draw();
    assert(0 == sparks);

    /* Ordinary obstacles cannot start a burst. */
    obstacle->max_life = 0.0f;
    fx_obstacle_burst_snapshot();
    obstacle->life = 0.0f;
    fx_obstacle_burst_snapshot();
    draw();
    assert(0 == sparks);

    /* Snapshot order cannot change obstacle identity. */
    fx_obstacle_burst_reset();
    g_game_state.obstacle_count = 3;
    const EntityID ids[] = { 26, 1, 13 };
    for (int i = 0; 3 > i; i++) {
        WorldObject* rock = &g_game_state.obstacles[i];
        *rock = (WorldObject){ .pos = { 10.0f, 10.0f }, .dims = { 2.0f, 2.0f },
                              .max_life = 10.0f, .life = 10.0f };
        rock->id = ids[i];
    }
    fx_obstacle_burst_snapshot();
    WorldObject swap = g_game_state.obstacles[0];
    g_game_state.obstacles[0] = g_game_state.obstacles[2];
    g_game_state.obstacles[2] = swap;
    g_game_state.obstacles[0].life = 0.0f;
    fx_obstacle_burst_snapshot();
    draw();
    assert(24 == sparks);
    fx_obstacle_burst_snapshot();
    draw();
    assert(24 == sparks);

    /* Simultaneous bursts use a fixed pool. */
    fx_obstacle_burst_reset();
    g_game_state.obstacle_count = 20;
    for (int i = 0; 20 > i; i++) {
        WorldObject* rock = &g_game_state.obstacles[i];
        *rock = (WorldObject){ .dims = { 2.0f, 2.0f }, .max_life = 10.0f, .life = 10.0f };
        rock->id = (EntityID)i + 1;
    }
    fx_obstacle_burst_snapshot();
    for (int i = 0; 20 > i; i++) g_game_state.obstacles[i].life = 0.0f;
    fx_obstacle_burst_snapshot();
    draw();
    assert(16 * 24 == sparks);
    fx_obstacle_burst_update(0.8f);
    draw();
    assert(0 == sparks);

    puts("fx_obstacle_burst_test OK");
    return 0;
}
