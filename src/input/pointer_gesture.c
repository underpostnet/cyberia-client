#include "pointer_gesture.h"

#include <assert.h>
#include <math.h>
#include <raylib.h>
#include <raymath.h>
#include <stdbool.h>

/* A second finger inside this window makes a UI press a pinch, not a tap. */
#define TAP_CONFIRM_WINDOW_S 0.12
/* Drag threshold: a gameplay press that travels farther starts the joystick. */
#define JOYSTICK_ACTIVATE_PX 14.0f
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

/* End the gesture as a release does: a pending press taps at its origin, the
 * joystick stops steering. */
static enum pointer_result release(pointer_gesture_t* g) {
    enum pointer_state ended = g->state;
    g->state = POINTER_IDLE;
    if (POINTER_UI_PENDING == ended || POINTER_PENDING == ended) {
        g->tap = g->origin;
        return POINTER_TAP;
    }
    return POINTER_JOYSTICK == ended ? POINTER_STEER_END : POINTER_NONE;
}

static void start(pointer_gesture_t* g, pointer_sample_t s) {
    g->state  = s.ui_owned ? POINTER_UI_PENDING : POINTER_PENDING;
    g->id     = s.id;
    g->age    = 0.0;
    g->origin = s.position;
    g->dir_x  = 0;
    g->dir_y  = 0;
}

enum pointer_result pointer_gesture_update(pointer_gesture_t* g, pointer_sample_t s, double dt) {
    assert(g);

    if (0 == s.count) return release(g);

    /* A press on a single contact while a gesture runs: the old contact's release
     * and this press fell inside one frame. The old gesture ends as released, so
     * the jump to the new press position never reads as a drag. */
    if (s.pressed && 1 == s.count && POINTER_IDLE != g->state) {
        enum pointer_result ended = release(g);
        start(g, s);
        return ended;
    }

    /* A second finger, or a new pointer, ends the gesture with no tap. */
    bool tracked = POINTER_IDLE != g->state && POINTER_SUPPRESSED != g->state;
    if (2 <= s.count || (tracked && s.id != g->id)) {
        bool steering = POINTER_JOYSTICK == g->state;
        g->state = POINTER_SUPPRESSED;
        return steering ? POINTER_STEER_END : POINTER_NONE;
    }

    if (POINTER_IDLE == g->state && s.pressed) start(g, s);

    if (POINTER_UI_PENDING == g->state) {
        g->age += dt;
        /* The mouse cannot pinch, so its UI press taps at once. */
        if (s.touch && g->age < TAP_CONFIRM_WINDOW_S) return POINTER_NONE;
        return release(g);
    }

    Vector2 d   = Vector2Subtract(s.position, g->origin);
    float   len = Vector2Length(d);
    if (POINTER_PENDING == g->state && JOYSTICK_ACTIVATE_PX < len) g->state = POINTER_JOYSTICK;
    if (POINTER_JOYSTICK != g->state) return POINTER_NONE;

    g->thumb = Vector2Add(g->origin, Vector2ClampValue(d, 0.0f, POINTER_JOYSTICK_RADIUS));
    if (len <= POINTER_JOYSTICK_RADIUS * JOYSTICK_DEAD_ZONE) {
        g->dir_x = 0;
        g->dir_y = 0;
        return POINTER_NONE;
    }
    Vector2 heading = Vector2Normalize((Vector2){ (float)g->dir_x, (float)g->dir_y });
    if (SECTOR_HOLD_COS <= Vector2DotProduct(Vector2Scale(d, 1.0f / len), heading)) return POINTER_NONE;
    g->dir_x = sector_axis(d.x, d.y);
    g->dir_y = sector_axis(d.y, d.x);
    return POINTER_NONE;
}
