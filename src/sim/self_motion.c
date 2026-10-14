#include "self_motion.h"

#include <assert.h>
#include <math.h>
#include <raylib.h>
#include <string.h>

#include "world/object_layer.h"
#include "world/world_types.h"

Direction direction_of(double dx, double dy) {
    if (0.0 == dx && 0.0 == dy) return DIRECTION_NONE;
    double angle = atan2(dy, dx);
    if (0.0 > angle) angle += 2.0 * PI;
    return (Direction)(((int)round(angle / (PI / 4.0)) + 2) % 8);
}

void self_motion_tap(self_motion_t* m, Vector2 target) {
    assert(m);
    Vector2 cell = { roundf(target.x), roundf(target.y) };
    if (0 < m->path_count && cell.x == m->path[m->path_count - 1].x &&
        cell.y == m->path[m->path_count - 1].y) return;
    m->path[0]    = cell;
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
    double step = speed * dt;
    while (0 < m->path_count) {
        Vector2 node = m->path[0];
        double dx = (double)node.x - m->pos.x, dy = (double)node.y - m->pos.y;
        double dist = sqrt(dx * dx + dy * dy);
        if (dist > step) {
            m->pos.x     = (float)(m->pos.x + dx / dist * step);
            m->pos.y     = (float)(m->pos.y + dy / dist * step);
            m->direction = direction_of(dx, dy);
            return;
        }
        m->pos = node;  /* node reached: the rest of the step walks the next leg */
        step -= dist;
        m->path_count--;
        memmove(m->path, m->path + 1, (size_t)m->path_count * sizeof m->path[0]);
    }
    m->direction = DIRECTION_NONE;
}
