#include "fx/fx_hit_burst.h"

#include <assert.h>
#include <raylib.h>
#include <stdio.h>

static int sparks;
static bool red;
static float highest_y;
static float lowest_y;

float world_cell_size(void) { return 32.0f; }
const Color FX_SPARK_RED = { 220, 40, 40, 255 };
void fx_shape_spark_shaded(float cx, float cy, float size_px, Color body) {
    assert(3.0f <= size_px);
    red = red && 220 == body.r && 40 == body.g;
    if (cy < highest_y) highest_y = cy;
    if (cy > lowest_y) lowest_y = cy;
    sparks++;
}

static void draw(void) {
    sparks = 0;
    red = true;
    highest_y = 1e9f;
    lowest_y = -1e9f;
    fx_hit_burst_draw();
}

int main(void) {
    const float hit_y = 5.0f * 32.0f;
    fx_hit_burst_reset();
    draw();
    assert(0 == sparks);

    /* A hit throws red sparks from its point. */
    fx_hit_burst_spawn((Vector2){ 10.0f, 5.0f });
    draw();
    assert(0 < sparks && red && hit_y == highest_y && hit_y == lowest_y);

    /* They jump up, and they are still in sight at the top of the jump. */
    fx_hit_burst_update(0.1f);
    draw();
    assert(0 < sparks && lowest_y < hit_y);
    float rising_top = highest_y;
    fx_hit_burst_update(0.15f);
    draw();
    assert(0 < sparks && highest_y < rising_top);

    /* Gravity pulls them back down while they shrink. */
    float apex = highest_y;
    fx_hit_burst_update(0.05f);
    draw();
    assert(0 < sparks && highest_y > apex);

    /* Gone after the whole burst. */
    fx_hit_burst_update(0.15f);
    draw();
    assert(0 == sparks);

    /* A burst of hits keeps every one of them, up to the pool, and a reset clears them. */
    for (int i = 0; 30 > i; i++) fx_hit_burst_spawn((Vector2){ (float)i, 5.0f });
    draw();
    assert(0 < sparks);
    fx_hit_burst_reset();
    draw();
    assert(0 == sparks);

    puts("fx_hit_burst_test OK");
    return 0;
}
