#include "input_command.h"
#include "input.h"
#include "network/replication.h"
#include "domain/camera.h"
#include "domain/local_player.h"
#include "game_state.h"

#include <assert.h>
#include <raylib.h>
#include <raymath.h>

void input_push(input_queue_t* q, input_event_t e) {
    assert(q);
    assert(q->count < Q_CAP);
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

/* ── Touch gestures ──────────────────────────────────────────────────────
 * Two-finger pinch drives the gameplay camera zoom (mirrors the Instance
 * Map's gesture; desktop keeps the wheel). Because raylib maps the first
 * touch onto the mouse, a pinch would otherwise begin with a press-tap that
 * moves the player. Touch presses are therefore held for a short window
 * (or until release) before the tap is emitted; a second finger inside the
 * window converts the gesture into a pinch and no tap ever fires. Mouse
 * presses (zero touch points) keep the immediate press-tap path, and the
 * emitted INPUT_TAP is identical either way — the wire command is built
 * downstream from the same screen/world pair. */
#define TAP_CONFIRM_WINDOW_S 0.12

static bool    s_gestures_blocked = false;
static bool    s_tap_pending      = false;
static double  s_tap_pending_age  = 0.0;
static Vector2 s_tap_pending_pos  = {0};
static bool    s_pinch_active     = false;   /* also latches until all fingers lift */
static float   s_pinch_prev_dist  = 0.0f;

/* ── Keyboard steering ───────────────────────────────────────────────────
 * A held direction key (arrows or WASD) sends a synthetic INPUT_TAP a short
 * distance ahead of the player. One held key gives a cardinal heading, two
 * give a true 45° diagonal; opposite keys on one axis cancel. Release stops
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
#define KEY_STEER_REACH_FACTOR    1.75f
#define KEY_STEER_REACH_MIN_CELLS 3.0f
#define KEY_STEER_DIAGONAL_NORM   0.70710678f

static int    s_steer_x   = 0;
static int    s_steer_y   = 0;
static double s_steer_age = 0.0;

void input_gestures_set_blocked(bool blocked) {
    s_gestures_blocked = blocked;
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
    float   cell = g_game_state.cell_size > 0.0f ? g_game_state.cell_size : 12.0f;

    if (0 == dir_x && 0 == dir_y) {
        return (Vector2){ self.x * cell, self.y * cell };
    }

    float reach = local_player_move_speed() * local_player_action_cooldown() *
                  KEY_STEER_REACH_FACTOR;
    if (reach < KEY_STEER_REACH_MIN_CELLS) { reach = KEY_STEER_REACH_MIN_CELLS; }
    if (0 != dir_x && 0 != dir_y) { reach *= KEY_STEER_DIAGONAL_NORM; }

    return (Vector2){
        (self.x + (float)dir_x * reach) * cell,
        (self.y + (float)dir_y * reach) * cell,
    };
}

static void emit_steer_tap(input_queue_t* q, int dir_x, int dir_y) {
    Vector2 world_pos = steer_world_target(dir_x, dir_y);
    push_tap(q, GetWorldToScreen2D(world_pos, camera_get()), world_pos, true);
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

void input_queue_on_tick(input_queue_t* q, double dt) {
    assert(q);

    int touches = GetTouchPointCount();

    if (touches >= 2) {
        /* Pinch: cancel any held tap so the gesture never moves the player. */
        s_tap_pending  = false;
        s_pinch_active = true;
        pinch_zoom_on_tick();
    } else {
        s_pinch_prev_dist = 0.0f;
        if (0 == touches) s_pinch_active = false;
    }

    bool pointer_tap = false;

    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && !s_pinch_active) {
        if (touches > 0) {
            /* Touch press: hold for the confirmation window. */
            s_tap_pending     = true;
            s_tap_pending_age = 0.0;
            s_tap_pending_pos = GetMousePosition();
        } else {
            emit_tap(q, GetMousePosition());
        }
        pointer_tap = true;
    }

    if (s_tap_pending) {
        s_tap_pending_age += dt;
        bool released = !IsMouseButtonDown(MOUSE_BUTTON_LEFT) && 0 == GetTouchPointCount();
        if (released || TAP_CONFIRM_WINDOW_S <= s_tap_pending_age) {
            s_tap_pending = false;
            emit_tap(q, s_tap_pending_pos);
        }
        pointer_tap = true;
    }

    int steer_x = axis_dir(KEY_LEFT, KEY_A, KEY_RIGHT, KEY_D);
    int steer_y = axis_dir(KEY_UP,   KEY_W, KEY_DOWN,  KEY_S);
    bool turned = steer_x != s_steer_x || steer_y != s_steer_y;
    /* A heading that turned to zero still owes one stop command. */
    bool refresh = (0 != steer_x || 0 != steer_y) &&
                   s_steer_age >= (double)local_player_action_cooldown();

    s_steer_age += dt;
    if ((turned || refresh) &&
        !pointer_tap && !s_gestures_blocked && !s_pinch_active) {
        emit_steer_tap(q, steer_x, steer_y);
        s_steer_x   = steer_x;
        s_steer_y   = steer_y;
        s_steer_age = 0.0;
    }

    switch (GetKeyPressed()) {
        case KEY_F3:
            input_push(q, (input_event_t){ .type = INPUT_KEY_DEBUG });
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

/* Helper — fills the common tick + sequence header. Allocating the
 * sequence in one place ensures it is monotonic regardless of which build
 * helper the caller invoked. */
static void stamp_header(input_command_t* cmd, input_kind_t kind) {
    cmd->kind         = kind;
    cmd->client_tick  = session_server_tick_estimate();
    cmd->sequence     = session_next_input_sequence();
}

input_command_t input_command_build_tap(float grid_x, float grid_y) {
    input_command_t cmd = {0};
    stamp_header(&cmd, INPUT_KIND_PLAYER_ACTION);
    cmd.target_x = grid_x;
    cmd.target_y = grid_y;
    return cmd;
}
