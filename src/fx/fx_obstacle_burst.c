#include "fx_obstacle_burst.h"
#include "fx_shapes.h"

#include "domain/presentation_runtime.h"
#include "game_state.h"
#include "util/hash_table.h"
#include "world_types.h"

#include <math.h>
#include <raylib.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#define FX_OBSTACLE_BURSTS 16
#define FX_OBSTACLE_PARTICLES 24
#define FX_OBSTACLE_DURATION 0.7f
#define FX_OBSTACLE_TAU 6.28318530718f

typedef struct {
    EntityID id;
    bool depleted;
} ObstacleState;

typedef struct {
    Vector2 pos;
    Vector2 dims;
    hash_t seed;
    float age;
    bool active;
} ObstacleBurst;

static ObstacleState s_seen[MAX_OBJECTS];
static int s_seen_count;
static ObstacleBurst s_bursts[FX_OBSTACLE_BURSTS];

static int compare_obstacles(const void* a, const void* b) {
    const ObstacleState* left = a;
    const ObstacleState* right = b;
    return (left->id > right->id) - (left->id < right->id);
}

void fx_obstacle_burst_snapshot(void) {
    for (int i = 0; g_game_state.obstacle_count > i; i++) {
        const WorldObject* obstacle = &g_game_state.obstacles[i];
        if (0.0f >= obstacle->max_life) continue;
        ObstacleState key = { .id = obstacle->id, .depleted = game_state_obstacle_depleted(obstacle) };
        const ObstacleState* prior = bsearch(&key, s_seen, (size_t)s_seen_count,
                                            sizeof s_seen[0], compare_obstacles);
        if (NULL == prior || prior->depleted == key.depleted) continue;

        ObstacleBurst* slot = &s_bursts[0];
        for (int j = 0; FX_OBSTACLE_BURSTS > j; j++) {
            if (!s_bursts[j].active) { slot = &s_bursts[j]; break; }
            if (slot->age < s_bursts[j].age) slot = &s_bursts[j];
        }
        *slot = (ObstacleBurst){ .pos = obstacle->pos, .dims = obstacle->dims,
                                .seed = (hash_t)obstacle->id * 2654435761u,.active = true };
    }

    /* Only the last snapshot can start a transition burst. */
    s_seen_count = 0;
    for (int i = 0; g_game_state.obstacle_count > i; i++) {
        const WorldObject* obstacle = &g_game_state.obstacles[i];
        if (0.0f >= obstacle->max_life) continue;
        ObstacleState* state = &s_seen[s_seen_count++];
        state->id = obstacle->id;
        state->depleted = game_state_obstacle_depleted(obstacle);
    }
    qsort(s_seen, (size_t)s_seen_count, sizeof s_seen[0], compare_obstacles);
}

void fx_obstacle_burst_update(float dt) {
    for (int i = 0; FX_OBSTACLE_BURSTS > i; i++) {
        ObstacleBurst* burst = &s_bursts[i];
        if (!burst->active) continue;
        burst->age += dt;
        if (FX_OBSTACLE_DURATION <= burst->age) burst->active = false;
    }
}

void fx_obstacle_burst_draw(void) {
    const float cell = world_cell_size();
    for (int i = 0; FX_OBSTACLE_BURSTS > i; i++) {
        const ObstacleBurst* burst = &s_bursts[i];
        if (!burst->active) continue;
        const float t = burst->age / FX_OBSTACLE_DURATION;
        const float travel = t * (2.0f - t);
        const float cx = (burst->pos.x + burst->dims.x * 0.5f) * cell;
        const float cy = (burst->pos.y + burst->dims.y * 0.5f) * cell;
        const float radius = fmaxf(burst->dims.x, burst->dims.y) * 0.6f + 0.6f;
        for (int j = 0; FX_OBSTACLE_PARTICLES > j; j++) {
            const float lane = (float)((burst->seed ^ ((hash_t)j * 2654435761u)) % 1000u) * 0.001f;
            const float angle = FX_OBSTACLE_TAU * ((float)j + lane * 0.5f) / FX_OBSTACLE_PARTICLES;
            const float reach = radius * (0.75f + lane * 0.5f) * cell * travel;
            const float size = cell * (0.16f + lane * 0.12f) * (1.0f - t);
            if (3.0f > size) continue;
            Color body = FX_SPARK_GRAY;
            body.r = body.g = body.b = (unsigned char)((float)FX_SPARK_GRAY.r * (0.3f + lane * 0.4f));
            fx_shape_spark_shaded(cx + cosf(angle) * reach, cy + sinf(angle) * reach, size, body);
        }
    }
}

void fx_obstacle_burst_reset(void) {
    s_seen_count = 0;
    memset(s_bursts, 0, sizeof s_bursts);
}
