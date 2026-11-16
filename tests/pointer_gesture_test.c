#include "input/pointer_gesture.h"

#include <assert.h>
#include <math.h>
#include <raylib.h>
#define RAYMATH_IMPLEMENTATION
#include <raymath.h>
#include <stdio.h>

#define DT (1.0 / 60.0)
/* Frames that hold a press past the joystick hold time. */
#define HOLD_FRAMES ((int)ceil(POINTER_JOYSTICK_HOLD_S / DT) + 1)
#define ACTIVATE    POINTER_JOYSTICK_ACTIVATE_PX

static const Vector2 ORIGIN = { 200.0f, 300.0f };

static Vector2 offset(float dx, float dy) { return (Vector2){ ORIGIN.x + dx, ORIGIN.y + dy }; }

/* A press at ORIGIN held at ORIGIN + (dx, dy) past the hold time. */
static pointer_gesture_t drag(float dx, float dy) {
    pointer_gesture_t g = pointer_gesture_start(ORIGIN);
    for (int i = 0; i < HOLD_FRAMES; i++) pointer_gesture_hold(&g, offset(dx, dy), DT, true);
    return g;
}

static void assert_heading(float angle_deg, int dir_x, int dir_y) {
    float a = angle_deg * DEG2RAD;
    pointer_gesture_t g = drag(50.0f * cosf(a), 50.0f * sinf(a));
    assert(g.steering && dir_x == g.dir_x && dir_y == g.dir_y);
}

int main(void) {
    /* A still press never steers; its origin is the tap position. */
    pointer_gesture_t g = drag(0.0f, 0.0f);
    for (int i = 0; i < 120; i++) pointer_gesture_hold(&g, ORIGIN, DT, true);
    assert(!g.steering && Vector2Equals(ORIGIN, g.origin));

    /* Travel inside the threshold never steers. */
    g = drag(ACTIVATE - 1.0f, 0.0f);
    assert(!g.steering);

    /* A quick press never steers, however far it travelled: a fast click on the move. */
    g = pointer_gesture_start(ORIGIN);
    for (int i = 0; i < HOLD_FRAMES / 2; i++) pointer_gesture_hold(&g, offset(3.0f * ACTIVATE, 0.0f), DT, true);
    assert(!g.steering);

    /* A press held still past the hold time steers on its first travel. */
    g = drag(0.0f, 0.0f);
    pointer_gesture_hold(&g, offset(0.0f, -(ACTIVATE + 1.0f)), DT, true);
    assert(g.steering && 0 == g.dir_x && -1 == g.dir_y);

    /* A press that may not steer stays a tap: another pointer steers. */
    g = pointer_gesture_start(ORIGIN);
    for (int i = 0; i < 2 * HOLD_FRAMES; i++) pointer_gesture_hold(&g, offset(3.0f * ACTIVATE, 0.0f), DT, false);
    assert(!g.steering);
    pointer_gesture_hold(&g, offset(3.0f * ACTIVATE, 0.0f), DT, true);
    assert(g.steering && 1 == g.dir_x && 0 == g.dir_y);

    /* Past the hold time and the threshold the joystick starts; its base stays at the press. */
    g = drag(ACTIVATE + 1.0f, 0.0f);
    assert(g.steering && 1 == g.dir_x && 0 == g.dir_y);
    pointer_gesture_hold(&g, offset(40.0f, 0.0f), DT, true);
    assert(Vector2Equals(ORIGIN, g.origin) && Vector2Equals(offset(40.0f, 0.0f), g.thumb));

    /* The thumb follows the pointer up to the radius. */
    pointer_gesture_hold(&g, offset(0.0f, 300.0f), DT, true);
    assert(Vector2Equals(offset(0.0f, POINTER_JOYSTICK_RADIUS), g.thumb));
    assert(0 == g.dir_x && 1 == g.dir_y);

    /* The dead zone gives no heading. */
    pointer_gesture_hold(&g, offset(6.0f, -5.0f), DT, true);
    assert(g.steering && 0 == g.dir_x && 0 == g.dir_y);

    /* Eight 45° sectors give the arrow-key headings; screen y grows down. */
    assert_heading(0.0f, 1, 0);
    assert_heading(45.0f, 1, 1);
    assert_heading(90.0f, 0, 1);
    assert_heading(135.0f, -1, 1);
    assert_heading(180.0f, -1, 0);
    assert_heading(-135.0f, -1, -1);
    assert_heading(-90.0f, 0, -1);
    assert_heading(-45.0f, 1, -1);
    assert_heading(20.0f, 1, 0);
    assert_heading(25.0f, 1, 1);

    /* A heading holds 5° past its sector edge. */
    g = drag(50.0f, 0.0f);
    pointer_gesture_hold(&g, offset(50.0f * cosf(25.0f * DEG2RAD), 50.0f * sinf(25.0f * DEG2RAD)), DT, true);
    assert(1 == g.dir_x && 0 == g.dir_y);
    pointer_gesture_hold(&g, offset(50.0f * cosf(30.0f * DEG2RAD), 50.0f * sinf(30.0f * DEG2RAD)), DT, true);
    assert(1 == g.dir_x && 1 == g.dir_y);

    puts("pointer_gesture_test OK");
    return 0;
}
