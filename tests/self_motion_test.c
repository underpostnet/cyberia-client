/* The own player's motion step mirrors the server stepAlongPath: the speed is
 * the same on every leg, and a tap to the walked cell keeps the path. */

#include "sim/self_motion.h"
#include "world/object_layer.h"

#include <assert.h>
#include <math.h>
#include <raylib.h>
#include <stdbool.h>
#include <stdio.h>

static void expect_pos(const self_motion_t* m, float x, float y, const char* what) {
    if (1e-4f < fabsf(m->pos.x - x) || 1e-4f < fabsf(m->pos.y - y)) {
        printf("%s: pos (%.5f, %.5f), want (%.5f, %.5f)\n", what, m->pos.x, m->pos.y, x, y);
        assert(false);
    }
}

int main(void) {
    /* A step that reaches a node walks the rest of its length on the next leg. */
    self_motion_t m = { .pos = { 0.9f, 0.0f }, .path = { { 1, 0 }, { 1, 1 } }, .path_count = 2 };
    self_motion_step(&m, 3.0, 0.1);
    expect_pos(&m, 1.0f, 0.2f, "carry");
    assert(1 == m.path_count);
    assert(DIRECTION_DOWN == m.direction);

    /* One node per cell walks as fast as one straight node. */
    self_motion_t cells = { .pos = { 0, 0 } }, line = { .pos = { 0, 0 } };
    for (int i = 0; 10 > i; i++) cells.path[cells.path_count++] = (Vector2){ (float)(i + 1), 0 };
    self_motion_tap(&line, (Vector2){ 10, 0 });
    for (int i = 0; 30 > i; i++) {
        self_motion_step(&cells, 5.0, 1.0 / 30.0);
        self_motion_step(&line, 5.0, 1.0 / 30.0);
    }
    expect_pos(&cells, 5.0f, 0.0f, "cell path");
    expect_pos(&line, 5.0f, 0.0f, "straight path");

    /* A tap to the walked cell keeps the path; a tap to another cell replaces it. */
    int walked = cells.path_count;
    self_motion_tap(&cells, (Vector2){ 10.2f, 0.3f });
    assert(walked == cells.path_count && 1 < walked);
    self_motion_tap(&cells, (Vector2){ 7.0f, 4.0f });
    assert(1 == cells.path_count && 7 == cells.path[0].x && 4 == cells.path[0].y);

    /* The walk ends on the last node, idle. */
    self_motion_t end = { .pos = { 0.95f, 0 }, .path = { { 1, 0 } }, .path_count = 1 };
    self_motion_step(&end, 3.0, 0.1);
    expect_pos(&end, 1.0f, 0.0f, "end");
    assert(0 == end.path_count && DIRECTION_NONE == end.direction);

    printf("self_motion_test OK\n");
    return 0;
}
