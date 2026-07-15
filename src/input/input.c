#include "input_command.h"
#include "input.h"
#include "network/replication.h"
#include "domain/camera.h"

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

void input_gestures_set_blocked(bool blocked) {
    s_gestures_blocked = blocked;
}

static void emit_tap(input_queue_t* q, Vector2 screen_pos) {
    input_push(q, (input_event_t){
        .type = INPUT_TAP,
        .screen_position = screen_pos,
        .world_position = GetScreenToWorld2D(screen_pos, camera_get()),
    });
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

    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && !s_pinch_active) {
        if (touches > 0) {
            /* Touch press: hold for the confirmation window. */
            s_tap_pending     = true;
            s_tap_pending_age = 0.0;
            s_tap_pending_pos = GetMousePosition();
        } else {
            emit_tap(q, GetMousePosition());
        }
    }

    if (s_tap_pending) {
        s_tap_pending_age += dt;
        bool released = !IsMouseButtonDown(MOUSE_BUTTON_LEFT) && 0 == GetTouchPointCount();
        if (released || TAP_CONFIRM_WINDOW_S <= s_tap_pending_age) {
            s_tap_pending = false;
            emit_tap(q, s_tap_pending_pos);
        }
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
