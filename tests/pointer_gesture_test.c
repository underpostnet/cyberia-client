#include "input/pointer_gesture.h"

#include <assert.h>
#include <math.h>
#include <raylib.h>
#define RAYMATH_IMPLEMENTATION
#include <raymath.h>
#include <stdio.h>

#define DT (1.0 / 60.0)
#define FINGER 7
#define MOUSE  (-1)

static const Vector2 ORIGIN = { 200.0f, 300.0f };

static pointer_sample_t press_at(Vector2 p, bool ui_owned) {
    return (pointer_sample_t){ .count = 1, .id = FINGER, .position = p, .pressed = true, .ui_owned = ui_owned, .touch = true };
}
static pointer_sample_t hold_at(Vector2 p) {
    return (pointer_sample_t){ .count = 1, .id = FINGER, .position = p, .touch = true };
}
static pointer_sample_t two_fingers(void) {
    return (pointer_sample_t){ .count = 2, .id = FINGER, .position = ORIGIN, .touch = true };
}
static pointer_sample_t mouse_at(Vector2 p, bool pressed, bool ui_owned) {
    return (pointer_sample_t){ .count = 1, .id = MOUSE, .position = p, .pressed = pressed, .ui_owned = ui_owned };
}
static const pointer_sample_t lifted = { 0 };

static Vector2 offset(float dx, float dy) { return (Vector2){ ORIGIN.x + dx, ORIGIN.y + dy }; }

/* A gameplay press at ORIGIN, then a hold at ORIGIN + (dx, dy). */
static pointer_gesture_t drag(float dx, float dy) {
    pointer_gesture_t g = { 0 };
    assert(POINTER_NONE == pointer_gesture_update(&g, press_at(ORIGIN, false), DT));
    assert(POINTER_NONE == pointer_gesture_update(&g, hold_at(offset(dx, dy)), DT));
    return g;
}

static void assert_heading(float angle_deg, int dir_x, int dir_y) {
    float a = angle_deg * DEG2RAD;
    pointer_gesture_t g = drag(50.0f * cosf(a), 50.0f * sinf(a));
    assert(POINTER_JOYSTICK == g.state);
    assert(dir_x == g.dir_x && dir_y == g.dir_y);
}

int main(void) {
    /* A stationary touch taps once at the press position, on release only. */
    pointer_gesture_t g = { 0 };
    assert(POINTER_NONE == pointer_gesture_update(&g, press_at(ORIGIN, false), DT));
    assert(POINTER_PENDING == g.state);
    for (int i = 0; i < 120; i++) assert(POINTER_NONE == pointer_gesture_update(&g, hold_at(ORIGIN), DT));
    assert(POINTER_TAP == pointer_gesture_update(&g, lifted, DT));
    assert(Vector2Equals(ORIGIN, g.tap) && POINTER_IDLE == g.state);
    assert(POINTER_NONE == pointer_gesture_update(&g, lifted, DT));

    /* Travel inside the drag threshold stays a tap. */
    g = drag(10.0f, 0.0f);
    assert(POINTER_PENDING == g.state);
    assert(POINTER_TAP == pointer_gesture_update(&g, lifted, DT));

    /* Past the threshold the joystick starts; its base stays at the press. */
    g = drag(15.0f, 0.0f);
    assert(POINTER_JOYSTICK == g.state && 1 == g.dir_x && 0 == g.dir_y);
    assert(POINTER_NONE == pointer_gesture_update(&g, hold_at(offset(40.0f, 0.0f)), DT));
    assert(Vector2Equals(ORIGIN, g.origin));
    assert(Vector2Equals(offset(40.0f, 0.0f), g.thumb));

    /* The thumb follows the finger up to the radius. */
    assert(POINTER_NONE == pointer_gesture_update(&g, hold_at(offset(0.0f, 300.0f)), DT));
    assert(Vector2Equals(offset(0.0f, POINTER_JOYSTICK_RADIUS), g.thumb));
    assert(0 == g.dir_x && 1 == g.dir_y);

    /* The dead zone gives no heading. */
    assert(POINTER_NONE == pointer_gesture_update(&g, hold_at(offset(6.0f, -5.0f)), DT));
    assert(POINTER_JOYSTICK == g.state && 0 == g.dir_x && 0 == g.dir_y);

    /* Release ends the steering once and taps nothing. */
    assert(POINTER_STEER_END == pointer_gesture_update(&g, lifted, DT));
    assert(POINTER_IDLE == g.state);
    assert(POINTER_NONE == pointer_gesture_update(&g, lifted, DT));

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
    assert(POINTER_NONE == pointer_gesture_update(&g, hold_at(offset(50.0f * cosf(25.0f * DEG2RAD), 50.0f * sinf(25.0f * DEG2RAD))), DT));
    assert(1 == g.dir_x && 0 == g.dir_y);
    assert(POINTER_NONE == pointer_gesture_update(&g, hold_at(offset(50.0f * cosf(30.0f * DEG2RAD), 50.0f * sinf(30.0f * DEG2RAD))), DT));
    assert(1 == g.dir_x && 1 == g.dir_y);

    /* A press while steering means the contact was released inside the frame: the steering ends
     * and a new gesture starts at the press. */
    assert(POINTER_STEER_END == pointer_gesture_update(&g, press_at(offset(-50.0f, 0.0f), false), DT));
    assert(POINTER_PENDING == g.state && Vector2Equals(offset(-50.0f, 0.0f), g.origin));
    g = drag(50.0f, 0.0f);

    /* A second finger ends the steering; nothing taps until every finger lifts. */
    assert(POINTER_STEER_END == pointer_gesture_update(&g, two_fingers(), DT));
    assert(POINTER_SUPPRESSED == g.state);
    assert(POINTER_NONE == pointer_gesture_update(&g, two_fingers(), DT));
    assert(POINTER_NONE == pointer_gesture_update(&g, hold_at(ORIGIN), DT));
    assert(POINTER_SUPPRESSED == g.state);
    assert(POINTER_NONE == pointer_gesture_update(&g, lifted, DT));
    assert(POINTER_IDLE == g.state);

    /* Quick taps whose release and next press share one frame: each taps at its own press,
     * however far apart, and none steers. */
    g = drag(5.0f, 0.0f);
    assert(POINTER_TAP == pointer_gesture_update(&g, press_at(offset(0.0f, 200.0f), false), DT));
    assert(Vector2Equals(ORIGIN, g.tap) && POINTER_PENDING == g.state);
    assert(POINTER_NONE == pointer_gesture_update(&g, hold_at(offset(0.0f, 200.0f)), DT));
    assert(POINTER_TAP == pointer_gesture_update(&g, press_at(offset(0.0f, -200.0f), false), DT));
    assert(Vector2Equals(offset(0.0f, 200.0f), g.tap) && POINTER_PENDING == g.state);
    assert(POINTER_TAP == pointer_gesture_update(&g, lifted, DT));
    assert(Vector2Equals(offset(0.0f, -200.0f), g.tap) && POINTER_IDLE == g.state);

    /* A second finger cancels a pending tap. */
    g = drag(5.0f, 0.0f);
    assert(POINTER_NONE == pointer_gesture_update(&g, two_fingers(), DT));
    assert(POINTER_NONE == pointer_gesture_update(&g, lifted, DT));

    /* A new contact 0 ends the gesture as a lost contact. */
    g = drag(30.0f, 0.0f);
    pointer_sample_t other = hold_at(offset(30.0f, 0.0f));
    other.id = FINGER + 1;
    assert(POINTER_STEER_END == pointer_gesture_update(&g, other, DT));
    assert(POINTER_SUPPRESSED == g.state);
    assert(POINTER_NONE == pointer_gesture_update(&g, lifted, DT));

    /* A UI press taps after the confirm window, still held, and never steers. */
    g = (pointer_gesture_t){ 0 };
    assert(POINTER_NONE == pointer_gesture_update(&g, press_at(ORIGIN, true), DT));
    assert(POINTER_UI_PENDING == g.state);
    int frames = 1;
    while (POINTER_NONE == pointer_gesture_update(&g, hold_at(offset(40.0f, 0.0f)), DT)) frames++;
    assert(7 <= frames && frames <= 8);
    assert(POINTER_IDLE == g.state && Vector2Equals(ORIGIN, g.tap));
    assert(POINTER_NONE == pointer_gesture_update(&g, hold_at(offset(80.0f, 0.0f)), DT));
    assert(POINTER_IDLE == g.state);
    assert(POINTER_NONE == pointer_gesture_update(&g, lifted, DT));

    /* A UI press released inside the window taps at once. */
    assert(POINTER_NONE == pointer_gesture_update(&g, press_at(ORIGIN, true), DT));
    assert(POINTER_TAP == pointer_gesture_update(&g, lifted, DT));

    /* A second finger inside the window makes a UI press a pinch. */
    assert(POINTER_NONE == pointer_gesture_update(&g, press_at(ORIGIN, true), DT));
    assert(POINTER_NONE == pointer_gesture_update(&g, two_fingers(), DT));
    assert(POINTER_NONE == pointer_gesture_update(&g, lifted, DT));

    /* The mouse cannot pinch: its UI press taps on the press frame. */
    assert(POINTER_TAP == pointer_gesture_update(&g, mouse_at(ORIGIN, true, true), DT));
    assert(POINTER_IDLE == g.state && Vector2Equals(ORIGIN, g.tap));
    assert(POINTER_NONE == pointer_gesture_update(&g, mouse_at(offset(80.0f, 0.0f), false, false), DT));
    assert(POINTER_NONE == pointer_gesture_update(&g, lifted, DT));

    /* A mouse click on the world taps on release; a mouse drag steers. */
    assert(POINTER_NONE == pointer_gesture_update(&g, mouse_at(ORIGIN, true, false), DT));
    assert(POINTER_PENDING == g.state);
    assert(POINTER_TAP == pointer_gesture_update(&g, lifted, DT));
    assert(POINTER_NONE == pointer_gesture_update(&g, mouse_at(ORIGIN, true, false), DT));
    assert(POINTER_NONE == pointer_gesture_update(&g, mouse_at(offset(0.0f, -40.0f), false, false), DT));
    assert(POINTER_JOYSTICK == g.state && 0 == g.dir_x && -1 == g.dir_y);
    assert(POINTER_STEER_END == pointer_gesture_update(&g, lifted, DT));

    /* A finger that lands during a mouse gesture ends it with no tap. */
    assert(POINTER_NONE == pointer_gesture_update(&g, mouse_at(ORIGIN, true, false), DT));
    assert(POINTER_NONE == pointer_gesture_update(&g, hold_at(ORIGIN), DT));
    assert(POINTER_SUPPRESSED == g.state);
    assert(POINTER_NONE == pointer_gesture_update(&g, lifted, DT));

    puts("pointer_gesture_test OK");
    return 0;
}
