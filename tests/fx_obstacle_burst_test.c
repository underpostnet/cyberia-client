#include "fx/fx_obstacle_burst.h"
#include "fx/fx_shapes.h"
#include "sim/game_state.h"
#include "world/world_types.h"

#include <assert.h>
#include <raylib.h>
#include <stdio.h>
#include <string.h>

GameState g_game_state;
const Color FX_SPARK_GRAY = { 168, 168, 168, 255 };
static int sparks;
static float min_x, max_x, min_y, max_y, largest;

float world_cell_size(void) { return 32.0f; }
int game_state_slot(EntityID id) {
    for (int i = 0; ENTITY_CAP > i; i++)
        if (0 != id && id == g_game_state.ids[i]) return i;
    return -1;
}
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

/* Put a standing destructible obstacle `id` in `slot`, listed at that index of
 * the scenery: one 2x2 footprint at (10, 10). */
static Vitals* place(int slot, EntityID id) {
    g_game_state.ids[slot] = id;
    g_game_state.mask[slot] = COMP_TRANSFORM | COMP_VITALS;
    g_game_state.transform[slot] = (EntityTransform){ .pos = { 10.0f, 10.0f }, .size = { 2.0f, 2.0f } };
    g_game_state.vitals[slot] = (Vitals){ .life = 10, .max_life = 10 };
    g_game_state.scenery[slot] = id;
    return &g_game_state.vitals[slot];
}

int main(void) {
    Vitals* obstacle = place(0, 26);
    g_game_state.scenery_count = 1;

    fx_obstacle_burst_reset();
    fx_obstacle_burst_snapshot();
    draw();
    assert(0 == sparks);

    obstacle->life = 0;
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
    g_game_state.scenery_count = 0;
    fx_obstacle_burst_snapshot();
    g_game_state.scenery_count = 1;
    obstacle->life = 0;
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
    obstacle->max_life = 0;
    fx_obstacle_burst_snapshot();
    obstacle->life = 0;
    fx_obstacle_burst_snapshot();
    draw();
    assert(0 == sparks);

    /* Snapshot order cannot change obstacle identity. */
    fx_obstacle_burst_reset();
    g_game_state.scenery_count = 3;
    const EntityID ids[] = { 26, 1, 13 };
    for (int i = 0; 3 > i; i++) place(i, ids[i]);
    fx_obstacle_burst_snapshot();
    g_game_state.scenery[0] = 13;
    g_game_state.scenery[2] = 26;
    g_game_state.vitals[2].life = 0;
    fx_obstacle_burst_snapshot();
    draw();
    assert(24 == sparks);
    fx_obstacle_burst_snapshot();
    draw();
    assert(24 == sparks);

    /* Simultaneous bursts use a fixed pool. */
    fx_obstacle_burst_reset();
    g_game_state.scenery_count = 20;
    for (int i = 0; 20 > i; i++) place(i, (EntityID)i + 1);
    fx_obstacle_burst_snapshot();
    for (int i = 0; 20 > i; i++) g_game_state.vitals[i].life = 0;
    fx_obstacle_burst_snapshot();
    draw();
    assert(16 * 24 == sparks);
    fx_obstacle_burst_update(0.8f);
    draw();
    assert(0 == sparks);

    puts("fx_obstacle_burst_test OK");
    return 0;
}
