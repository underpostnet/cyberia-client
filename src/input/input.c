#include "input.h"
#include "pointer_gesture.h"
#include "network/game/client_event.h"
#include "network/game/replication.h"
#include "domain/camera.h"
#include "domain/local_player.h"
#include "domain/presentation_runtime.h"
#include "game_state.h"
#include "js/pointer_bridge.h"
#include "util/log.h"

#include <assert.h>
#include <raylib.h>
#include <raymath.h>
#include <stdlib.h>

void input_push(input_queue_t* q, input_event_t e) {
    assert(q);
    if (!(q->count < Q_CAP)) {
        assert(false && "input queue full");
        LOG_ERROR("input queue full");
        abort();
    }
    q->evt[(q->head + q->count) % Q_CAP] = e;
    q->count++;
}

bool input_pop(input_queue_t* q, input_event_t* out) {
    assert(q);
    assert(out);
    if (0 == q->count) { return false; }
    *out = q->evt[q->head];
    q->head = (q->head + 1) % Q_CAP;
    q->count--;
    return true;
}

void input_queue_filter(input_queue_t* q, input_consume_fn consume) {
    assert(q);
    assert(consume);
    input_queue_t keep = { 0 };
    input_event_t evt = { 0 };
    while (input_pop(q, &evt)) {
        if (!consume(&evt)) { input_push(&keep, evt); }
    }
    while (input_pop(&keep, &evt)) { input_push(q, evt); }
}

/* ── Pointer gestures ────────────────────────────────────────────────────
 * pointer_gesture.c turns each press of a finger or the mouse into a tap, a
 * joystick drag or a pinch. Two-finger pinch drives the gameplay camera zoom
 * (mirrors the Instance Map's gesture; desktop keeps the wheel). The emitted
 * INPUT_TAP is identical for both pointers — the wire command is built
 * downstream from the same screen/world pair. */
/* A browser touch identifier is never negative. */
#define MOUSE_POINTER_ID (-1)

static bool              s_gestures_blocked = false;
static pointer_gesture_t s_pointer          = { 0 };
static float             s_pinch_prev_dist  = 0.0f;
static int               s_pointer_presses  = 0;

/* ── Steering ────────────────────────────────────────────────────────────
 * A held direction key (arrows or WASD) or the joystick sends a
 * synthetic INPUT_TAP a short distance ahead of the player. One held key
 * gives a cardinal heading, two give a true 45° diagonal; opposite keys on
 * one axis cancel. The joystick gives the same eight headings. Release stops
 * the walk, because a zero heading targets the cell the player stands on.
 *
 * Two calibrations keep the walk fluid:
 *
 * A change of heading emits at once — a key press, a key release, a turn. That
 * is the whole point of steering, and the server re-plans on the next tick, so
 * nothing is gained by holding it back. Only the refresh of an unchanged
 * heading waits: it exists to keep the target ahead of the player, and every
 * tap also rolls the skill trigger, so repeating one faster than the action
 * cooldown spends skill rolls for no movement.
 *
 * The target is anchored on the predicted position, never on the camera.
 * The camera is a lag follower, so a camera-relative target puts the camera
 * lag into the heading and makes a diagonal follow the screen aspect ratio
 * instead of 45°. Reach stays near one refresh of travel: far enough that
 * the walk never stalls between commands, short enough that the server A*
 * returns a near-straight path instead of routing around distant terrain. */
#define STEER_REACH_FACTOR    1.75f
#define STEER_REACH_MIN_CELLS 3.0f
#define STEER_DIAGONAL_NORM   0.70710678f

static int    s_steer_x   = 0;
static int    s_steer_y   = 0;
static double s_steer_age = 0.0;

void input_gestures_set_blocked(bool blocked) {
    s_gestures_blocked = blocked;
}

const pointer_gesture_t* input_pointer_gesture(void) {
    return &s_pointer;
}

static void push_tap(input_queue_t* q, Vector2 screen_pos, Vector2 world_pos,
                     bool synthetic) {
    input_push(q, (input_event_t){
        .type = INPUT_TAP,
        .screen_position = screen_pos,
        .world_position = world_pos,
        .synthetic = synthetic,
    });
}

static void emit_tap(input_queue_t* q, Vector2 screen_pos) {
    push_tap(q, screen_pos, GetScreenToWorld2D(screen_pos, camera_get()), false);
}

/* -1 low, +1 high, 0 when no key or both keys are held. */
static int axis_dir(int low_key, int low_alt, int high_key, int high_alt) {
    bool low  = IsKeyDown(low_key)  || IsKeyDown(low_alt);
    bool high = IsKeyDown(high_key) || IsKeyDown(high_alt);
    if (low == high) { return 0; }
    return low ? -1 : 1;
}

/* World-pixel target for a heading. A zero heading gives the current cell,
 * which is the stop command. */
static Vector2 steer_world_target(int dir_x, int dir_y) {
    Vector2 self = prediction_self_position();
    float   cell = world_cell_size();

    if (0 == dir_x && 0 == dir_y) {
        return (Vector2){ self.x * cell, self.y * cell };
    }

    float reach = local_player_move_speed() * local_player_action_cooldown() *
                  STEER_REACH_FACTOR;
    if (reach < STEER_REACH_MIN_CELLS) { reach = STEER_REACH_MIN_CELLS; }
    if (0 != dir_x && 0 != dir_y) { reach *= STEER_DIAGONAL_NORM; }

    return (Vector2){
        (self.x + (float)dir_x * reach) * cell,
        (self.y + (float)dir_y * reach) * cell,
    };
}

/* Send one steering tap and record its heading. */
static void emit_steer_tap(input_queue_t* q, int dir_x, int dir_y) {
    Vector2 world_pos = steer_world_target(dir_x, dir_y);
    push_tap(q, GetWorldToScreen2D(world_pos, camera_get()), world_pos, true);
    s_steer_x   = dir_x;
    s_steer_y   = dir_y;
    s_steer_age = 0.0;
}

static void pinch_zoom_on_tick(void) {
    Vector2 a = GetTouchPosition(0);
    Vector2 b = GetTouchPosition(1);
    float   d = Vector2Distance(a, b);
    if (s_pinch_prev_dist > 1.0f && d > 1.0f && !s_gestures_blocked) {
        camera_zoom_by(d / s_pinch_prev_dist);
    }
    s_pinch_prev_dist = d;
}

void input_queue_on_tick(input_queue_t* q, double dt, input_hit_fn ui_covers) {
    assert(q);
    assert(ui_covers);

    int touches = GetTouchPointCount();

    if (touches >= 2) {
        pinch_zoom_on_tick();
    } else {
        s_pinch_prev_dist = 0.0f;
    }

    int  presses     = pointer_bridge_press_count();
    bool pressed     = presses != s_pointer_presses;
    bool pointer_tap = false;
    s_pointer_presses = presses;

    pointer_sample_t pointer = { .count = touches, .pressed = pressed, .touch = 0 < touches };
    if (pointer.touch) {
        pointer.id       = GetTouchPointId(0);
        pointer.position = GetTouchPosition(0);
    } else {
        pointer.count    = IsMouseButtonDown(MOUSE_BUTTON_LEFT) ? 1 : 0;
        pointer.id       = MOUSE_POINTER_ID;
        pointer.position = GetMousePosition();
    }
    pointer.ui_owned = pressed && (s_gestures_blocked ||
                       ui_covers((int)pointer.position.x, (int)pointer.position.y));
    switch (pointer_gesture_update(&s_pointer, pointer, dt)) {
        case POINTER_TAP:
            emit_tap(q, s_pointer.tap);
            pointer_tap = true;
            break;
        case POINTER_STEER_END:
            /* The joystick owes one stop, also when a pinch ends it. */
            if (0 != s_steer_x || 0 != s_steer_y) { emit_steer_tap(q, 0, 0); }
            break;
        case POINTER_NONE: break;
    }

    /* The joystick steers while it is active; the keys steer otherwise. */
    bool joystick = POINTER_JOYSTICK == s_pointer.state;
    int steer_x = joystick ? s_pointer.dir_x : axis_dir(KEY_LEFT, KEY_A, KEY_RIGHT, KEY_D);
    int steer_y = joystick ? s_pointer.dir_y : axis_dir(KEY_UP,   KEY_W, KEY_DOWN,  KEY_S);
    bool turned = steer_x != s_steer_x || steer_y != s_steer_y;
    /* A heading that turned to zero still owes one stop command. */
    bool refresh = (0 != steer_x || 0 != steer_y) &&
                   s_steer_age >= (double)local_player_action_cooldown();
    /* A pending tap or a pinch holds the steering. */
    bool gesture_held = POINTER_IDLE != s_pointer.state && !joystick;

    s_steer_age += dt;
    if ((turned || refresh) &&
        !pointer_tap && !s_gestures_blocked && !gesture_held) {
        emit_steer_tap(q, steer_x, steer_y);
    }

    switch (GetKeyPressed()) {
        case KEY_F3:
            input_push(q, (input_event_t){ .type = INPUT_KEY_DEBUG });
            break;
        case KEY_ESCAPE:
            input_push(q, (input_event_t){ .type = INPUT_KEY_ESCAPE });
            break;
        default: break;
    }

    float wheel = GetMouseWheelMove();
    if (!FloatEquals(wheel, 0.0f)) {
        input_push(q, (input_event_t){ .type = INPUT_ZOOM,
                                       .zoom_in = wheel > 0,
                                       .wheel_delta = wheel });
    }
}

void input_push_client_events(input_queue_t q) {
    input_event_t evt = { 0 };
    while (input_pop(&q, &evt)) {
        if (INPUT_TAP != evt.type) continue;
        float cell = world_cell_size();
        client_event_payload_t p = {
            .target_x = evt.world_position.x / cell,
            .target_y = evt.world_position.y / cell,
        };
        client_event_t e = client_event_push(CLIENT_EVENT_PLAYER_ACTION, p);
        prediction_enqueue_input(&e);
    }
}
