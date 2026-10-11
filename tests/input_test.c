#include "input/input.h"
#include "input/pointer_gesture.h"
#include "network/game/client_event.h"
#include "network/game/replication.h"

#include <assert.h>
#include <math.h>
#include <raylib.h>
#define RAYMATH_IMPLEMENTATION
#include <raymath.h>
#include <stdbool.h>
#include <stdio.h>

#define DT (1.0 / 60.0)
/* Frames that hold a press past the joystick hold time. */
#define HOLD_FRAMES ((int)ceil(POINTER_JOYSTICK_HOLD_S / DT) + 1)

/* Stubbed platform: identity camera, player at cell (10, 10), 32 px cells,
 * speed 2 and a 0.5 s cooldown, so a steering tap reaches 3 cells. */
static int     s_touches;
static Vector2 s_touch_pos[2];
static int     s_presses;
static bool    s_mouse_down;
static Vector2 s_mouse;
static bool    s_keys[512];
static bool    s_ui_covers;
static int     s_cover_calls;
static float   s_zoom = 1.0f;
static int     s_actions;
static client_event_t s_action;

int GetTouchPointCount(void) { return s_touches; }
int GetTouchPointId(int index) { return 40 + index; }
Vector2 GetTouchPosition(int index) { return s_touch_pos[index]; }
int pointer_bridge_press_count(void) { return s_presses; }
bool IsMouseButtonDown(int button) { return MOUSE_BUTTON_LEFT == button && (s_mouse_down || 0 < s_touches); }
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

static bool ui_covers(int x, int y) {
    s_cover_calls++;
    return s_ui_covers;
}

static input_queue_t frame(void) {
    input_queue_t q = { 0 };
    input_queue_on_tick(&q, DT, ui_covers);
    return q;
}

/* Taps of one kind in `q`; `last` gets the newest. */
static int taps(input_queue_t q, bool synthetic, input_event_t* last) {
    int n = 0;
    input_event_t e;
    while (input_pop(&q, &e)) {
        if (INPUT_TAP != e.type || synthetic != e.synthetic) continue;
        n++;
        if (last) *last = e;
    }
    return n;
}

static int all_taps(input_queue_t q) { return taps(q, false, NULL) + taps(q, true, NULL); }

static bool near(Vector2 a, float x, float y) { return fabsf(a.x - x) < 0.01f && fabsf(a.y - y) < 0.01f; }

static void mouse_down(float x, float y) {
    s_mouse_down = true;
    s_mouse = (Vector2){ x, y };
    s_presses++;
}

static void touch_down(float x, float y) {
    s_touches = 1;
    s_touch_pos[0] = (Vector2){ x, y };
    s_presses++;
}

/* Hold the pointer still past the joystick hold time; nothing taps. */
static void hold_still(void) {
    for (int i = 0; i < HOLD_FRAMES; i++) assert(0 == all_taps(frame()));
}

/* One synthetic tap in a frame, at the given screen and world point. */
static void assert_steer(input_queue_t q, float x, float y) {
    input_event_t e;
    assert(1 == taps(q, true, &e) && 0 == taps(q, false, NULL));
    assert(near(e.screen_position, x, y) && near(e.world_position, x, y));
}

int main(void) {
    /* Mouse: a click on the world taps on release, at the press position. */
    mouse_down(50.0f, 60.0f);
    assert(0 == all_taps(frame()));
    s_mouse_down = false;
    input_event_t e;
    input_queue_t q = frame();
    assert(1 == taps(q, false, &e) && 0 == taps(q, true, NULL));
    assert(near(e.screen_position, 50.0f, 60.0f) && near(e.world_position, 50.0f, 60.0f));

    /* A quick click on the move taps at its press and never steers. */
    mouse_down(100.0f, 100.0f);
    assert(0 == all_taps(frame()));
    s_mouse = (Vector2){ 220.0f, 100.0f };
    assert(0 == all_taps(frame()) && POINTER_JOYSTICK != input_pointer_gesture()->state);
    s_mouse_down = false;
    q = frame();
    assert(1 == taps(q, false, &e) && 0 == taps(q, true, NULL) && near(e.screen_position, 100.0f, 100.0f));

    /* A held mouse drag steers like the keys; release stops the walk once. */
    mouse_down(200.0f, 200.0f);
    hold_still();
    s_mouse = (Vector2){ 260.0f, 200.0f };
    assert_steer(frame(), 416.0f, 320.0f);
    assert(POINTER_JOYSTICK == input_pointer_gesture()->state);
    s_mouse_down = false;
    assert_steer(frame(), 320.0f, 320.0f);
    assert(0 == all_taps(frame()));

    /* A mouse press on the UI taps on the press frame and never steers. */
    s_ui_covers = true;
    mouse_down(100.0f, 100.0f);
    q = frame();
    assert(1 == taps(q, false, NULL) && 0 == taps(q, true, NULL));
    for (int i = 0; i < 30; i++) {
        s_mouse.x += 5.0f;
        assert(0 == all_taps(frame()));
    }
    assert(POINTER_IDLE == input_pointer_gesture()->state);
    s_mouse_down = false;
    assert(0 == all_taps(frame()));
    s_ui_covers = false;

    /* Keys: a heading emits at once, refreshes per cooldown, and stops on release. */
    s_keys[KEY_RIGHT] = true;
    assert_steer(frame(), 416.0f, 320.0f);
    for (int i = 0; i < 29; i++) assert(0 == all_taps(frame()));
    int refresh = 0;
    for (int i = 0; i < 3 && 0 == refresh; i++) refresh = taps(frame(), true, NULL);
    assert(1 == refresh);
    s_keys[KEY_DOWN] = true;
    assert_steer(frame(), 387.88f, 387.88f);
    s_keys[KEY_RIGHT] = s_keys[KEY_DOWN] = false;
    assert_steer(frame(), 320.0f, 320.0f);
    assert(0 == all_taps(frame()));

    /* A still touch taps once on release, at the press position. */
    s_cover_calls = 0;
    touch_down(100.0f, 110.0f);
    for (int i = 0; i < 30; i++) assert(0 == all_taps(frame()));
    s_touches = 0;
    q = frame();
    assert(1 == taps(q, false, &e) && 0 == taps(q, true, NULL));
    assert(near(e.screen_position, 100.0f, 110.0f) && 1 == s_cover_calls);
    assert(0 == all_taps(frame()));

    /* The real tap reaches prediction as a player action in cells. */
    s_actions = 0;
    input_push_client_events(q);
    assert(1 == s_actions && CLIENT_EVENT_PLAYER_ACTION == s_action.kind);
    assert(fabsf(s_action.payload.target_x - 100.0f / 32.0f) < 0.001f);

    /* A held drag steers like the keys and never taps the press position. */
    touch_down(200.0f, 200.0f);
    hold_still();
    s_touch_pos[0] = (Vector2){ 260.0f, 200.0f };
    q = frame();
    assert_steer(q, 416.0f, 320.0f);
    assert(POINTER_JOYSTICK == input_pointer_gesture()->state);
    s_actions = 0;
    input_push_client_events(q);
    assert(1 == s_actions && CLIENT_EVENT_PLAYER_ACTION == s_action.kind);
    assert(fabsf(s_action.payload.target_x - 13.0f) < 0.001f && fabsf(s_action.payload.target_y - 10.0f) < 0.001f);
    for (int i = 0; i < 29; i++) assert(0 == all_taps(frame()));
    refresh = 0;
    for (int i = 0; i < 3 && 0 == refresh; i++) refresh = taps(frame(), true, NULL);
    assert(1 == refresh);
    s_touch_pos[0] = (Vector2){ 200.0f, 140.0f };
    assert_steer(frame(), 320.0f, 224.0f);

    /* The joystick wins over a held key while it steers. */
    s_keys[KEY_LEFT] = true;
    assert(0 == all_taps(frame()));
    s_keys[KEY_LEFT] = false;

    /* Release stops the walk once and taps nothing. */
    s_touches = 0;
    assert_steer(frame(), 320.0f, 320.0f);
    assert(0 == all_taps(frame()));
    assert(POINTER_IDLE == input_pointer_gesture()->state);

    /* Quick taps whose release and next press share one frame tap at their own presses and
     * never steer, however far apart. */
    touch_down(100.0f, 100.0f);
    assert(0 == all_taps(frame()));
    touch_down(100.0f, 400.0f);
    q = frame();
    assert(1 == taps(q, false, &e) && 0 == taps(q, true, NULL) && near(e.screen_position, 100.0f, 100.0f));
    assert(POINTER_PENDING == input_pointer_gesture()->state);
    s_touches = 0;
    q = frame();
    assert(1 == taps(q, false, &e) && near(e.screen_position, 100.0f, 400.0f));

    /* A second finger stops the joystick once; the pinch still zooms. */
    touch_down(200.0f, 200.0f);
    hold_still();
    s_touch_pos[0] = (Vector2){ 260.0f, 200.0f };
    assert_steer(frame(), 416.0f, 320.0f);
    s_touches = 2;
    s_touch_pos[1] = (Vector2){ 260.0f, 300.0f };
    assert_steer(frame(), 320.0f, 320.0f);
    s_touch_pos[1] = (Vector2){ 260.0f, 400.0f };
    assert(0 == all_taps(frame()) && s_zoom > 1.9f);
    s_touches = 1;
    assert(0 == all_taps(frame()));
    s_touches = 0;
    assert(0 == all_taps(frame()));

    /* A pinch over a pending touch taps nothing. */
    touch_down(100.0f, 100.0f);
    frame();
    s_touches = 2;
    s_touch_pos[1] = (Vector2){ 150.0f, 100.0f };
    assert(0 == all_taps(frame()));
    s_touches = 0;
    assert(0 == all_taps(frame()));

    /* A touch on the UI taps after the confirm window and never steers. */
    s_ui_covers = true;
    touch_down(100.0f, 100.0f);
    int real = 0;
    for (int i = 0; i < 10; i++) real += taps(frame(), false, NULL);
    assert(1 == real);
    s_touch_pos[0] = (Vector2){ 300.0f, 100.0f };
    for (int i = 0; i < 10; i++) assert(0 == all_taps(frame()));
    assert(POINTER_JOYSTICK != input_pointer_gesture()->state);
    s_touches = 0;
    assert(0 == all_taps(frame()));
    s_ui_covers = false;

    /* Blocked gestures: no joystick, no pinch zoom, no key steering. */
    input_gestures_set_blocked(true);
    touch_down(100.0f, 100.0f);
    real = 0;
    for (int i = 0; i < 10; i++) real += taps(frame(), false, NULL);
    s_touch_pos[0] = (Vector2){ 300.0f, 100.0f };
    for (int i = 0; i < 10; i++) assert(0 == all_taps(frame()));
    assert(1 == real && POINTER_JOYSTICK != input_pointer_gesture()->state);
    s_touches = 2;
    s_touch_pos[1] = (Vector2){ 300.0f, 200.0f };
    frame();
    float zoom = s_zoom;
    s_touch_pos[1] = (Vector2){ 300.0f, 300.0f };
    frame();
    assert(zoom == s_zoom);
    s_touches = 0;
    s_keys[KEY_UP] = true;
    assert(0 == all_taps(frame()));
    s_keys[KEY_UP] = false;
    input_gestures_set_blocked(false);

    puts("input_test OK");
    return 0;
}
