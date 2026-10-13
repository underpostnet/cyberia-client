// Hit burst. Each burst throws a few sparks on headings that fan upward; every
// spark flies out, falls under gravity and shrinks to nothing. Shapes stay
// opaque, in the particle style of the other effects, and are sized from the
// cell so they read at any zoom.

#include "fx_hit_burst.h"

#include "fx_shapes.h"

#include "content/presentation_runtime.h"

#include <math.h>
#include <raylib.h>
#include <stdbool.h>
#include <string.h>

#define FX_HIT_BURST_CAPACITY 24
#define FX_HIT_BURST_SPARKS   6
#define FX_HIT_BURST_DURATION 0.45f
#define FX_HIT_BURST_TAU      6.28318530718f
#define FX_HIT_BURST_GRAVITY  16.0f   /* cells per second squared */

typedef struct {
    Vector2 pos;   /* grid units */
    float   age;
    int     seed;
    bool    active;
} FxHitBurst;

static FxHitBurst s_bursts[FX_HIT_BURST_CAPACITY];
static int s_spawned;

/* Deterministic 0..1 per spark, so each keeps its heading and speed. */
static float lane(int i, int salt) {
    unsigned h = (unsigned)(i * 2654435761u) ^ (unsigned)(salt * 40503u);
    return (float)(h % 1000u) * 0.001f;
}

void fx_hit_burst_spawn(Vector2 world_position) {
    FxHitBurst* slot = &s_bursts[0];
    for (int i = 0; FX_HIT_BURST_CAPACITY > i; i++) {
        if (!s_bursts[i].active) { slot = &s_bursts[i]; break; }
        if (s_bursts[i].age > slot->age) slot = &s_bursts[i];
    }
    *slot = (FxHitBurst){ .pos = world_position, .seed = ++s_spawned, .active = true };
}

void fx_hit_burst_update(float dt) {
    for (int i = 0; FX_HIT_BURST_CAPACITY > i; i++) {
        FxHitBurst* burst = &s_bursts[i];
        if (!burst->active) continue;
        burst->age += dt;
        if (FX_HIT_BURST_DURATION <= burst->age) burst->active = false;
    }
}

void fx_hit_burst_draw(void) {
    float cell = world_cell_size();
    for (int i = 0; FX_HIT_BURST_CAPACITY > i; i++) {
        const FxHitBurst* burst = &s_bursts[i];
        if (!burst->active) continue;
        float t = burst->age;
        for (int j = 0; FX_HIT_BURST_SPARKS > j; j++) {
            /* Within 60° of straight up. */
            float angle = -FX_HIT_BURST_TAU * 0.25f + (lane(j, burst->seed) - 0.5f) * FX_HIT_BURST_TAU / 3.0f;
            float speed = 3.0f + 1.5f * lane(j, burst->seed + 1);
            float x = burst->pos.x + cosf(angle) * speed * t;
            float y = burst->pos.y + sinf(angle) * speed * t + 0.5f * FX_HIT_BURST_GRAVITY * t * t;
            /* Full size through the top of the jump, shrinking away on the fall. */
            float u = t / FX_HIT_BURST_DURATION;
            float size = cell * (0.16f + 0.08f * lane(j, burst->seed + 2)) * (1.0f - u * u);
            if (3.0f > size) continue;
            fx_shape_spark_shaded(x * cell, y * cell, size, FX_SPARK_RED);
        }
    }
}

void fx_hit_burst_reset(void) {
    memset(s_bursts, 0, sizeof s_bursts);
}
