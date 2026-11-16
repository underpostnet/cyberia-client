#include "pointer_gesture.h"

#include <assert.h>
#include <math.h>
#include <raylib.h>
#include <raymath.h>
#include <stdbool.h>

/* Part of the radius around the base that gives no heading. */
#define JOYSTICK_DEAD_ZONE   0.15f
/* tan 22.5°: the edge between a cardinal and a diagonal 45° sector. */
#define SECTOR_EDGE_TAN      0.41421356f
/* cos 27.5°: a heading holds 5° past its sector edge, so jitter cannot flip it. */
#define SECTOR_HOLD_COS      0.88701083f

/* -1, 0 or +1 on one axis. Zero when the other axis keeps the heading cardinal. */
static int sector_axis(float along, float across) {
    if (!(fabsf(along) > SECTOR_EDGE_TAN * fabsf(across))) return 0;
    return 0.0f < along ? 1 : -1;
}

pointer_gesture_t pointer_gesture_start(Vector2 origin) {
    return (pointer_gesture_t){ .origin = origin, .thumb = origin };
}

void pointer_gesture_hold(pointer_gesture_t* g, Vector2 position, double dt, bool may_steer) {
    assert(g);
    Vector2 d   = Vector2Subtract(position, g->origin);
    float   len = Vector2Length(d);
    if (!g->steering) {
        g->age += dt;
        g->steering = may_steer && POINTER_JOYSTICK_HOLD_S <= g->age && POINTER_JOYSTICK_ACTIVATE_PX < len;
        if (!g->steering) return;
    }

    g->thumb = Vector2Add(g->origin, Vector2ClampValue(d, 0.0f, POINTER_JOYSTICK_RADIUS));
    if (len <= POINTER_JOYSTICK_RADIUS * JOYSTICK_DEAD_ZONE) {
        g->dir_x = 0;
        g->dir_y = 0;
        return;
    }
    Vector2 heading = Vector2Normalize((Vector2){ (float)g->dir_x, (float)g->dir_y });
    if (SECTOR_HOLD_COS <= Vector2DotProduct(Vector2Scale(d, 1.0f / len), heading)) return;
    g->dir_x = sector_axis(d.x, d.y);
    g->dir_y = sector_axis(d.y, d.x);
}
