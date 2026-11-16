#include "self_motion.h"

#include <assert.h>
#include <math.h>
#include <raylib.h>
#include <string.h>

#include "content/stat_contract_generated.h"
#include "world/object_layer.h"
#include "world/world_types.h"

Direction direction_of(double dx, double dy) {
    if (0.0 == dx && 0.0 == dy) return DIRECTION_NONE;
    Direction best = DIRECTION_UP;
    double best_dot = -INFINITY;
    for (int dir = 0; CYBERIA_DIRECTION_COUNT > dir; dir++) {
        double dot = dx * CYBERIA_DIRECTION_VECTORS[dir][0] + dy * CYBERIA_DIRECTION_VECTORS[dir][1];
        if (best_dot < dot) {
            best     = (Direction)dir;
            best_dot = dot;
        }
    }
    return best;
}

void self_motion_tap(self_motion_t* m, Vector2 target) {
    assert(m);
    Vector2 cell = { roundf(target.x), roundf(target.y) };
    /* A tap on the cell the player stands on stops the walk in place, as the server does. */
    if (cell.x == roundf(m->pos.x) && cell.y == roundf(m->pos.y)) {
        m->path_count = 0;
        return;
    }
    if (0 < m->path_count && cell.x == m->path[m->path_count - 1].x &&
        cell.y == m->path[m->path_count - 1].y) return;
    m->path[0]    = cell;
    m->path_count = 1;
}

void self_motion_stop(self_motion_t* m) {
    assert(m);
    m->path_count = 0;
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
}
