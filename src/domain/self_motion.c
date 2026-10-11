#include "self_motion.h"

#include <assert.h>
#include <math.h>
#include <raylib.h>
#include <string.h>

#include "object_layer.h"
#include "world_types.h"

Direction direction_of(double dx, double dy) {
    if (0.0 == dx && 0.0 == dy) return DIRECTION_NONE;
    double angle = atan2(dy, dx);
    if (0.0 > angle) angle += 2.0 * PI;
    return (Direction)(((int)round(angle / (PI / 4.0)) + 2) % 8);
}

void self_motion_tap(self_motion_t* m, Vector2 target) {
    assert(m);
    m->path[0]    = (Vector2){ roundf(target.x), roundf(target.y) };
    m->path_count = 1;
}

void self_motion_stop(self_motion_t* m) {
    assert(m);
    m->path_count = 0;
    m->direction  = DIRECTION_NONE;
}

void self_motion_step(self_motion_t* m, double speed, double dt) {
    assert(m);
    if (0 == m->path_count) return;
    Vector2 node = m->path[0];
    double dx = (double)node.x - m->pos.x, dy = (double)node.y - m->pos.y;
    double dist = sqrt(dx * dx + dy * dy);
    double step = speed * dt;
    if (dist >= step) {
        m->pos.x     = (float)(m->pos.x + dx / dist * step);
        m->pos.y     = (float)(m->pos.y + dy / dist * step);
        m->direction = direction_of(dx, dy);
        return;
    }
    m->pos = node;  /* node reached: face the next leg, if there is one */
    m->path_count--;
    memmove(m->path, m->path + 1, (size_t)m->path_count * sizeof m->path[0]);
    m->direction = 0 < m->path_count
        ? direction_of((double)m->path[0].x - node.x, (double)m->path[0].y - node.y)
        : DIRECTION_NONE;
}
