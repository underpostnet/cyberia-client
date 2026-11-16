#include "input/input.h"
#include "input/pointer_gesture.h"
#include "js/pointer_bridge.h"
#include "net/client_event.h"
#include "sim/replication.h"

#include <assert.h>
#include <math.h>
#include <raylib.h>
#define RAYMATH_IMPLEMENTATION
#include <raymath.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#define DT (1.0 / 60.0)
/* Frames that hold a press past the joystick hold time. */
#define HOLD_FRAMES ((int)ceil(POINTER_JOYSTICK_HOLD_S / DT) + 1)
#define MOUSE 1

/* Stubbed platform: identity camera, player at cell (10, 10), 32 px cells,
 * speed 2 and a 0.5 s cooldown, so a steering tap reaches 3 cells. The UI
 * covers the strip above y = 50; a surface right of x = 600 claims each press. */
static pointer_event_t s_bridge[64];
static int     s_bridge_count;
static Vector2 s_at[64]; /* the last position of each pointer id */
static Vector2 s_mouse;
static bool    s_keys[512];
static float   s_zoom = 1.0f;
static int     s_actions;
static client_event_t s_action;
static int     s_claims;
static int     s_claimed; /* the handle of the last claimed press */

int pointer_bridge_drain(pointer_event_t* out, int max) {
    int n = s_bridge_count < max ? s_bridge_count : max;
    memcpy(out, s_bridge, (size_t)n * sizeof *out);
    memmove(s_bridge, s_bridge + n, (size_t)(s_bridge_count - n) * sizeof *out);
    s_bridge_count -= n;
    return n;
}
Vector2 GetMousePosition(void) { return s_mouse; }
bool IsKeyDown(int key) { return s_keys[key]; }
int GetKeyPressed(void) { return 0; }
float GetMouseWheelMove(void) { return 0.0f; }
Vector2 GetScreenToWorld2D(Vector2 position, Camera2D camera) { return position; }
Vector2 GetWorldToScreen2D(Vector2 position, Camera2D camera) { return position; }
Camera2D camera_get(void) { return (Camera2D){ 0 }; }
void camera_zoom_by(float factor) { s_zoom *= factor; }
const self_motion_t* prediction_self(void) {
    static const self_motion_t self = { .pos = { 10.0f, 10.0f } };
    return &self;
}
float world_cell_size(void) { return 32.0f; }
float local_player_move_speed(void) { return 2.0f; }
float local_player_action_cooldown(void) { return 0.5f; }
void client_event_push(client_event_kind_t kind, client_event_payload_t p) {
    s_actions++;
    s_action = (client_event_t){ .kind = kind, .payload = p };
}

static bool ui_covers(int x, int y) { return 50 > y; }

static bool claim(int pointer, Vector2 position) {
    if (position.x < 600.0f) return false;
    s_claims++;
    s_claimed = pointer;
    return true;
}

static void event(pointer_event_kind_e kind, int id, Vector2 at) {
    assert(64 > s_bridge_count);
    s_bridge[s_bridge_count++] = (pointer_event_t){ .kind = kind, .id = id, .touch = MOUSE != id, .x = at.x, .y = at.y };
}
static void down(int id, float x, float y) {
    s_at[id] = (Vector2){ x, y };
    event(POINTER_EVENT_DOWN, id, s_at[id]);
}
static void move(int id, float x, float y) {
    s_at[id] = (Vector2){ x, y };
    event(POINTER_EVENT_MOVE, id, s_at[id]);
}
static void up(int id) { event(POINTER_EVENT_UP, id, s_at[id]); }
static void cancel(int id) { event(POINTER_EVENT_CANCEL, id, (Vector2){ 0 }); }

static input_queue_t frame(void) {
    input_queue_t q = { 0 };
    input_queue_on_tick(&q, DT, ui_covers, claim);
    return q;
}

/* Taps of one kind in `q`; `last` gets the newest. */
static int taps(input_queue_t q, bool synthetic, bool select_only, input_event_t* last) {
    int n = 0;
    input_event_t e;
    while (input_pop(&q, &e)) {
        if (INPUT_TAP != e.type || synthetic != e.synthetic || select_only != e.select_only) continue;
        n++;
        if (last) *last = e;
    }
    return n;
}

static int all_taps(input_queue_t q) {
    int n = 0;
    input_event_t e;
    while (input_pop(&q, &e)) n += INPUT_TAP == e.type;
    return n;
}

static bool near(Vector2 a, float x, float y) { return fabsf(a.x - x) < 0.01f && fabsf(a.y - y) < 0.01f; }

/* Hold every pointer still past the joystick hold time; nothing taps. */
static void hold_still(void) {
    for (int i = 0; i < HOLD_FRAMES; i++) assert(0 == all_taps(frame()));
}

/* One synthetic tap and no other tap in a frame, at the given screen and world point. */
static void assert_steer(input_queue_t q, float x, float y) {
    input_event_t e;
    assert(1 == taps(q, true, false, &e) && 1 == all_taps(q));
    assert(near(e.screen_position, x, y) && near(e.world_position, x, y));
}

/* One real tap that walks and no other tap in a frame, at the given point. */
static void assert_tap(input_queue_t q, float x, float y) {
    input_event_t e;
    assert(1 == taps(q, false, false, &e) && 1 == all_taps(q));
    assert(near(e.screen_position, x, y));
}

/* A press of `id` at (200, 200) dragged right until it steers. */
static void steer_right(int id) {
    down(id, 200.0f, 200.0f);
    hold_still();
    move(id, 260.0f, 200.0f);
    assert_steer(frame(), 416.0f, 320.0f);
    assert(NULL != input_joystick());
}

static void test_mouse_and_keys(void) {
    /* A click on the world taps on release, at the press position. */
    down(MOUSE, 50.0f, 60.0f);
    assert(0 == all_taps(frame()));
    up(MOUSE);
    input_queue_t q = frame();
    assert_tap(q, 50.0f, 60.0f);

    /* The real tap reaches prediction as a player action in cells. */
    s_actions = 0;
    input_push_client_events(q);
    assert(1 == s_actions && CLIENT_EVENT_PLAYER_ACTION == s_action.kind);
    assert(fabsf(s_action.payload.target_x - 50.0f / 32.0f) < 0.001f);

    /* A quick click on the move taps at its press and never steers. */
    down(MOUSE, 100.0f, 100.0f);
    assert(0 == all_taps(frame()));
    move(MOUSE, 220.0f, 100.0f);
    assert(0 == all_taps(frame()) && NULL == input_joystick());
    up(MOUSE);
    assert_tap(frame(), 100.0f, 100.0f);

    /* A held drag steers like the keys; release stops the walk once. */
    steer_right(MOUSE);
    up(MOUSE);
    assert_steer(frame(), 320.0f, 320.0f);
    assert(0 == all_taps(frame()) && NULL == input_joystick());

    /* A press on the UI taps on the press frame, follows the pointer, and never steers. */
    down(MOUSE, 100.0f, 20.0f);
    q = frame();
    assert(1 == taps(q, false, false, NULL) && 1 == all_taps(q));
    assert(input_pointer_down() && !input_pointer_touch());
    for (int i = 0; i < 2 * HOLD_FRAMES; i++) {
        move(MOUSE, 100.0f + 10.0f * (float)i, 20.0f);
        assert(0 == all_taps(frame()));
    }
    assert(NULL == input_joystick() && near(input_pointer_position(), 100.0f + 10.0f * (float)(2 * HOLD_FRAMES - 1), 20.0f));
    up(MOUSE);
    assert(0 == all_taps(frame()) && !input_pointer_down());
    s_mouse = (Vector2){ 5.0f, 6.0f };
    frame();
    assert(near(input_pointer_position(), 5.0f, 6.0f));

    /* Keys: a heading emits at once, refreshes per cooldown, and stops on release. */
    s_keys[KEY_RIGHT] = true;
    assert_steer(frame(), 416.0f, 320.0f);
    for (int i = 0; i < 29; i++) assert(0 == all_taps(frame()));
    int refresh = 0;
    for (int i = 0; i < 3 && 0 == refresh; i++) refresh = taps(frame(), true, false, NULL);
    assert(1 == refresh);
    s_keys[KEY_DOWN] = true;
    assert_steer(frame(), 387.88f, 387.88f);

    /* A press on a control holds no key back: the keys keep steering. */
    int claims = s_claims;
    down(MOUSE, 700.0f, 300.0f);
    assert(0 == all_taps(frame()) && claims + 1 == s_claims);
    refresh = 0;
    for (int i = 0; i < 40 && 0 == refresh; i++) refresh = taps(frame(), true, false, NULL);
    assert(1 == refresh);
    up(MOUSE);
    s_keys[KEY_RIGHT] = s_keys[KEY_DOWN] = false;
    assert_steer(frame(), 320.0f, 320.0f);
    assert(0 == all_taps(frame()));
}

static void test_one_finger(void) {
    /* A still touch taps once on release, at the press position. */
    down(2, 100.0f, 110.0f);
    for (int i = 0; i < 30; i++) assert(0 == all_taps(frame()));
    up(2);
    assert_tap(frame(), 100.0f, 110.0f);
    assert(0 == all_taps(frame()));

    /* A held drag steers like the keys, refreshes, turns, and wins over a held key. */
    steer_right(3);
    for (int i = 0; i < 29; i++) assert(0 == all_taps(frame()));
    int refresh = 0;
    for (int i = 0; i < 3 && 0 == refresh; i++) refresh = taps(frame(), true, false, NULL);
    assert(1 == refresh);
    move(3, 200.0f, 140.0f);
    assert_steer(frame(), 320.0f, 224.0f);
    s_keys[KEY_LEFT] = true;
    assert(0 == all_taps(frame()));
    s_keys[KEY_LEFT] = false;
    up(3);
    assert_steer(frame(), 320.0f, 320.0f);
    assert(0 == all_taps(frame()) && NULL == input_joystick());

    /* Quick taps whose release and next press share one frame tap at their own presses. */
    down(4, 100.0f, 100.0f);
    frame();
    up(4);
    down(5, 100.0f, 400.0f);
    assert_tap(frame(), 100.0f, 100.0f);
    up(5);
    assert_tap(frame(), 100.0f, 400.0f);

    /* A browser id that ends and presses again in one frame is two presses. */
    down(4, 100.0f, 100.0f);
    frame();
    up(4);
    down(4, 300.0f, 100.0f);
    assert_tap(frame(), 100.0f, 100.0f);
    up(4);
    assert_tap(frame(), 300.0f, 100.0f);

    /* A held id that presses again lost its release: the old press ends as cancelled. */
    down(6, 100.0f, 100.0f);
    frame();
    down(6, 300.0f, 300.0f);
    assert(0 == all_taps(frame()));
    up(6);
    assert_tap(frame(), 300.0f, 300.0f);

    /* A cancelled touch taps nothing. */
    down(6, 100.0f, 100.0f);
    frame();
    cancel(6);
    assert(0 == all_taps(frame()));
}

static void test_world_fingers(void) {
    /* A second world finger never ends the joystick. Its tap selects and does not walk. */
    steer_right(7);
    down(8, 300.0f, 400.0f);
    assert(0 == all_taps(frame()));
    move(7, 200.0f, 140.0f);
    assert_steer(frame(), 320.0f, 224.0f);
    up(8);
    input_queue_t q = frame();
    input_event_t e;
    assert(1 == taps(q, false, true, &e) && 1 == all_taps(q) && near(e.screen_position, 300.0f, 400.0f));
    s_actions = 0;
    input_push_client_events(q);
    assert(0 == s_actions && NULL != input_joystick());
    up(7);
    assert_steer(frame(), 320.0f, 320.0f);

    /* Two world fingers pinch: the camera zooms, and neither taps nor steers until each lifts. */
    s_zoom = 1.0f;
    down(9, 200.0f, 300.0f);
    frame();
    down(10, 260.0f, 300.0f);
    assert(0 == all_taps(frame()));
    move(10, 320.0f, 300.0f);
    assert(0 == all_taps(frame()) && fabsf(s_zoom - 2.0f) < 0.001f);
    up(9);
    assert(0 == all_taps(frame()));
    for (int i = 0; i < 2 * HOLD_FRAMES; i++) {
        move(10, 320.0f + 10.0f * (float)i, 300.0f);
        assert(0 == all_taps(frame()));
    }
    assert(NULL == input_joystick() && fabsf(s_zoom - 2.0f) < 0.001f);
    up(10);
    assert(0 == all_taps(frame()));
    s_zoom = 1.0f;
}

static void test_joystick_and_control(void) {
    Vector2 at;

    /* Skill first: the control takes the first finger; the second steers. Neither pinches. */
    down(11, 700.0f, 300.0f);
    assert(0 == all_taps(frame()));
    int skill = s_claimed;
    steer_right(12);
    move(11, 650.0f, 250.0f);
    assert(0 == all_taps(frame()) && 1.0f == s_zoom);
    assert(INPUT_POINTER_HELD == input_pointer_at(skill, &at) && near(at, 650.0f, 250.0f));
    up(11);
    assert(0 == all_taps(frame()) && NULL != input_joystick());
    assert(INPUT_POINTER_RELEASED == input_pointer_at(skill, &at) && near(at, 650.0f, 250.0f));
    frame();
    assert(INPUT_POINTER_GONE == input_pointer_at(skill, &at));
    up(12);
    assert_steer(frame(), 320.0f, 320.0f);

    /* Joystick first: a quick tap on a control lands and lifts in one frame. */
    steer_right(13);
    down(14, 700.0f, 300.0f);
    up(14);
    assert(0 == all_taps(frame()));
    skill = s_claimed;
    assert(INPUT_POINTER_RELEASED == input_pointer_at(skill, &at) && near(at, 700.0f, 300.0f));
    assert(NULL != input_joystick() && 1 == input_joystick()->dir_x);

    /* A drag on a control reads its own finger; the joystick keeps its heading. */
    down(15, 700.0f, 300.0f);
    frame();
    skill = s_claimed;
    move(15, 640.0f, 360.0f);
    assert(0 == all_taps(frame()));
    assert(INPUT_POINTER_HELD == input_pointer_at(skill, &at) && near(at, 640.0f, 360.0f));
    assert(1 == input_joystick()->dir_x && 0 == input_joystick()->dir_y);

    /* A cancelled control press ends as cancelled; the joystick keeps steering. */
    cancel(15);
    assert(0 == all_taps(frame()));
    assert(INPUT_POINTER_CANCELLED == input_pointer_at(skill, &at) && NULL != input_joystick());

    /* A cancelled joystick stops the walk once; the held control keeps its press. */
    down(16, 700.0f, 300.0f);
    frame();
    skill = s_claimed;
    cancel(13);
    assert_steer(frame(), 320.0f, 320.0f);
    assert(NULL == input_joystick() && INPUT_POINTER_HELD == input_pointer_at(skill, &at));
    up(16);
    assert(0 == all_taps(frame()) && INPUT_POINTER_RELEASED == input_pointer_at(skill, &at));

    /* Each press gets its own handle, also when the browser id repeats. */
    down(16, 700.0f, 300.0f);
    frame();
    assert(skill != s_claimed);
    up(16);
    frame();
}

static void test_ui_pointer(void) {
    Vector2 pair[2];

    /* A UI press never starts the joystick, however far it drags. */
    down(17, 100.0f, 20.0f);
    assert(1 == taps(frame(), false, false, NULL));
    for (int i = 0; i < 2 * HOLD_FRAMES; i++) {
        move(17, 100.0f + 20.0f * (float)i, 20.0f);
        assert(0 == all_taps(frame()));
    }
    assert(NULL == input_joystick() && input_pointer_touch());
    up(17);
    assert(0 == all_taps(frame()) && !input_pointer_down() && !input_pointer_cancelled());

    /* A lost UI pointer ends as cancelled for one frame. */
    down(18, 100.0f, 20.0f);
    assert(1 == taps(frame(), false, false, NULL));
    cancel(18);
    assert(0 == all_taps(frame()) && !input_pointer_down() && input_pointer_cancelled());
    frame();
    assert(!input_pointer_cancelled());

    /* While the joystick steers, the UI pointer is the UI finger. A second UI
     * press sends no tap; the UI pointer stays the first until it lifts. */
    steer_right(20);
    down(21, 100.0f, 20.0f);
    assert(1 == taps(frame(), false, false, NULL));
    assert(input_pointer_down() && near(input_pointer_position(), 100.0f, 20.0f));
    down(22, 300.0f, 30.0f);
    assert(0 == all_taps(frame()));
    assert(2 == input_ui_pointers(pair) && near(pair[0], 100.0f, 20.0f) && near(pair[1], 300.0f, 30.0f));
    move(21, 120.0f, 25.0f);
    frame();
    assert(near(input_pointer_position(), 120.0f, 25.0f));
    up(21);
    assert(0 == all_taps(frame()) && !input_pointer_down() && near(input_pointer_position(), 120.0f, 25.0f));
    assert(NULL != input_joystick());
    up(22);
    up(20);
    assert_steer(frame(), 320.0f, 320.0f);
    assert(0 == input_ui_pointers(pair));
}

static void test_blocked(void) {
    Vector2 pair[2];

    /* Blocked gestures: every press is UI. No joystick, no pinch zoom, no key steering. */
    input_gestures_set_blocked(true);
    down(24, 100.0f, 300.0f);
    assert_tap(frame(), 100.0f, 300.0f);
    for (int i = 0; i < 2 * HOLD_FRAMES; i++) {
        move(24, 100.0f + 20.0f * (float)i, 300.0f);
        assert(0 == all_taps(frame()));
    }
    down(25, 200.0f, 300.0f);
    assert(0 == all_taps(frame()) && 2 == input_ui_pointers(pair));
    move(25, 400.0f, 300.0f);
    assert(0 == all_taps(frame()) && 1.0f == s_zoom && NULL == input_joystick());
    up(24);
    up(25);
    frame();
    s_keys[KEY_UP] = true;
    assert(0 == all_taps(frame()));
    s_keys[KEY_UP] = false;
    input_gestures_set_blocked(false);
    frame();
}

static void test_focus_loss(void) {
    Vector2 at;

    /* A focus loss cancels every held press: one stop, no tap, no cast. */
    steer_right(26);
    down(27, 700.0f, 300.0f);
    frame();
    int skill = s_claimed;
    down(28, 100.0f, 400.0f);
    frame();
    cancel(26);
    cancel(27);
    cancel(28);
    assert_steer(frame(), 320.0f, 320.0f);
    assert(INPUT_POINTER_CANCELLED == input_pointer_at(skill, &at) && NULL == input_joystick());
    assert(0 == all_taps(frame()));
}

static bool drops_all_but_taps(const input_event_t* e) { return INPUT_TAP != e->type; }

static void test_queue_order(void) {
    /* The filter keeps the events it does not consume, in their order. */
    input_queue_t q = { 0 };
    input_push(&q, (input_event_t){ .type = INPUT_TAP, .screen_position = { 1.0f, 0.0f } });
    input_push(&q, (input_event_t){ .type = INPUT_ZOOM });
    input_push(&q, (input_event_t){ .type = INPUT_TAP, .screen_position = { 2.0f, 0.0f } });
    input_push(&q, (input_event_t){ .type = INPUT_KEY_ESCAPE });
    input_push(&q, (input_event_t){ .type = INPUT_TAP, .screen_position = { 3.0f, 0.0f } });
    input_queue_filter(&q, drops_all_but_taps);
    assert(3 == q.count);
    for (float x = 1.0f; 3.0f >= x; x += 1.0f) {
        input_event_t e;
        assert(input_pop(&q, &e) && INPUT_TAP == e.type && x == e.screen_position.x);
    }

    /* Two quick taps in one frame keep their order up to the player actions. */
    down(29, 100.0f, 100.0f);
    up(29);
    down(30, 200.0f, 200.0f);
    up(30);
    q = frame();
    input_queue_t copy = q;
    input_event_t e;
    assert(input_pop(&q, &e) && near(e.screen_position, 100.0f, 100.0f));
    assert(input_pop(&q, &e) && near(e.screen_position, 200.0f, 200.0f));
    s_actions = 0;
    input_push_client_events(copy);
    assert(2 == s_actions && fabsf(s_action.payload.target_x - 200.0f / 32.0f) < 0.001f);

    /* A select tap and the stop of a cancelled joystick in one frame: the stop walks. */
    steer_right(31);
    down(32, 300.0f, 400.0f);
    up(32);
    cancel(31);
    q = frame();
    assert(input_pop(&q, &e) && e.select_only && !e.synthetic);
    assert(input_pop(&q, &e) && e.synthetic && near(e.world_position, 320.0f, 320.0f));
    assert(0 == q.count);
}

int main(void) {
    test_mouse_and_keys();
    test_one_finger();
    test_world_fingers();
    test_joystick_and_control();
    test_ui_pointer();
    test_blocked();
    test_focus_loss();
    test_queue_order();
    puts("input_test OK");
    return 0;
}
