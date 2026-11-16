#include "input.h"
#include "pointer_gesture.h"
#include "js/pointer_bridge.h"
#include "net/client_event.h"
#include "sim/replication.h"
#include "view/camera.h"
#include "sim/local_player.h"
#include "content/presentation_runtime.h"
#include "sim/game_state.h"
#include "log.h"

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

/* ── Pointer router ──────────────────────────────────────────────────────
 * The table of the presses this frame, in press order: the held ones and the
 * ones that ended in this frame. A claimer reads the end of its press in the
 * frame it happens; the next frame drops it. */
#define POINTERS_MAX 10
#define DRAIN_BATCH  32

typedef enum owner {
    OWNER_CLAIMED,
    OWNER_UI,
    OWNER_WORLD,
    OWNER_JOYSTICK,
    OWNER_PINCH,
} owner_e;

typedef struct pointer {
    int                   id;     /* the browser pointerId */
    int                   handle; /* unique per press */
    owner_e               owner;
    input_pointer_state_e state;
    bool                  touch;
    Vector2               position;
    pointer_gesture_t     gesture; /* OWNER_WORLD and OWNER_JOYSTICK */
} pointer_t;

static pointer_t s_pointers[POINTERS_MAX];
static int       s_pointer_count    = 0;
static int       s_next_handle      = 1; /* 0 is no press */
static int       s_ui_handle        = 0; /* the UI pointer */
static int       s_pinch[2]         = { 0, 0 };
static float     s_pinch_prev_dist  = 0.0f;
static bool      s_gestures_blocked = false;
static bool      s_walked           = false; /* a tap this frame walks */

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
 * heading waits: it exists to keep the target ahead of the player, so it
 * repeats once per action cooldown.
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

static void push_tap(input_queue_t* q, Vector2 screen_pos, Vector2 world_pos,
                     bool synthetic, bool select_only) {
    input_push(q, (input_event_t){
        .type = INPUT_TAP,
        .screen_position = screen_pos,
        .world_position = world_pos,
        .synthetic = synthetic,
        .select_only = select_only,
    });
}

static void emit_tap(input_queue_t* q, Vector2 screen_pos, bool select_only) {
    push_tap(q, screen_pos, GetScreenToWorld2D(screen_pos, camera_get()), false, select_only);
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
    Vector2 self = prediction_self()->pos;
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
    push_tap(q, GetWorldToScreen2D(world_pos, camera_get()), world_pos, true, false);
    s_steer_x   = dir_x;
    s_steer_y   = dir_y;
    s_steer_age = 0.0;
}

static pointer_t* find_held(int id) {
    for (int i = 0; s_pointer_count > i; i++) {
        if (INPUT_POINTER_HELD == s_pointers[i].state && id == s_pointers[i].id) return &s_pointers[i];
    }
    return NULL;
}

static pointer_t* find_handle(int handle) {
    for (int i = 0; s_pointer_count > i; i++) {
        if (handle == s_pointers[i].handle) return &s_pointers[i];
    }
    return NULL;
}

/* The first held press of an owner. */
static pointer_t* first_held(owner_e owner) {
    for (int i = 0; s_pointer_count > i; i++) {
        if (INPUT_POINTER_HELD == s_pointers[i].state && owner == s_pointers[i].owner) return &s_pointers[i];
    }
    return NULL;
}

static bool ui_pointer_held(void) {
    const pointer_t* p = find_handle(s_ui_handle);
    return NULL != p && INPUT_POINTER_HELD == p->state;
}

/* A world press joins a pinch, pairs with a pending world press into one, or
 * starts its own gesture. */
static void start_world(pointer_t* p) {
    pointer_t* pending = first_held(OWNER_WORLD);
    if (NULL != first_held(OWNER_PINCH)) {
        p->owner = OWNER_PINCH;
    } else if (NULL != pending && pending != p) {
        p->owner = pending->owner = OWNER_PINCH;
    } else {
        p->gesture = pointer_gesture_start(p->position);
    }
}

static void press(input_queue_t* q, const pointer_event_t* e, input_hit_fn ui_covers, input_claim_fn claim) {
    if (POINTERS_MAX == s_pointer_count) return;
    pointer_t* p = &s_pointers[s_pointer_count++];
    *p = (pointer_t){ .id = e->id, .handle = s_next_handle++, .state = INPUT_POINTER_HELD,
                      .touch = 0 != e->touch, .position = { e->x, e->y } };
    if (s_gestures_blocked)                     p->owner = OWNER_UI;
    else if (claim(p->handle, p->position))     p->owner = OWNER_CLAIMED;
    else if (ui_covers((int)e->x, (int)e->y))   p->owner = OWNER_UI;
    else                                        p->owner = OWNER_WORLD;

    if (OWNER_WORLD == p->owner) start_world(p);
    if (OWNER_UI != p->owner || ui_pointer_held()) return;
    s_ui_handle = p->handle;
    emit_tap(q, p->position, false);
}

/* The end of a press: a world press taps at its origin on release, and the
 * joystick owes one stop. */
static void end(input_queue_t* q, pointer_t* p, bool cancelled) {
    p->state = cancelled ? INPUT_POINTER_CANCELLED : INPUT_POINTER_RELEASED;
    if (OWNER_JOYSTICK == p->owner && (0 != s_steer_x || 0 != s_steer_y)) emit_steer_tap(q, 0, 0);
    if (OWNER_WORLD != p->owner || cancelled) return;
    bool select_only = NULL != first_held(OWNER_JOYSTICK);
    emit_tap(q, p->gesture.origin, select_only);
    s_walked = s_walked || !select_only;
}

static void route(input_queue_t* q, const pointer_event_t* e, input_hit_fn ui_covers, input_claim_fn claim) {
    pointer_t* p = find_held(e->id);
    switch ((pointer_event_kind_e)e->kind) {
        case POINTER_EVENT_DOWN:
            /* A held id that presses again lost its release. */
            if (NULL != p) end(q, p, true);
            press(q, e, ui_covers, claim);
            break;
        case POINTER_EVENT_MOVE:
            if (NULL != p) p->position = (Vector2){ e->x, e->y };
            break;
        case POINTER_EVENT_UP:
            if (NULL == p) break;
            p->position = (Vector2){ e->x, e->y };
            end(q, p, false);
            break;
        case POINTER_EVENT_CANCEL:
            if (NULL != p) end(q, p, true);
            break;
    }
}

/* Drop the presses that ended in the last frame, then route the new events. */
static void sample(input_queue_t* q, input_hit_fn ui_covers, input_claim_fn claim) {
    int kept = 0;
    for (int i = 0; s_pointer_count > i; i++) {
        if (INPUT_POINTER_HELD == s_pointers[i].state) s_pointers[kept++] = s_pointers[i];
    }
    s_pointer_count = kept;

    pointer_event_t events[DRAIN_BATCH];
    int n;
    do {
        n = pointer_bridge_drain(events, DRAIN_BATCH);
        for (int i = 0; n > i; i++) route(q, &events[i], ui_covers, claim);
    } while (DRAIN_BATCH == n);
}

/* Each held world press may become the joystick; the joystick follows its pointer. */
static void hold_world(double dt) {
    bool may_start = NULL == first_held(OWNER_JOYSTICK) && !s_gestures_blocked;
    for (int i = 0; s_pointer_count > i; i++) {
        pointer_t* p = &s_pointers[i];
        if (INPUT_POINTER_HELD != p->state || (OWNER_WORLD != p->owner && OWNER_JOYSTICK != p->owner)) continue;
        pointer_gesture_hold(&p->gesture, p->position, dt, OWNER_JOYSTICK == p->owner || may_start);
        if (OWNER_WORLD != p->owner || !p->gesture.steering) continue;
        p->owner = OWNER_JOYSTICK;
        may_start = false;
    }
}

/* The first two held pinch presses zoom by the change of their distance. */
static void pinch_zoom(void) {
    const pointer_t* pair[2] = { NULL, NULL };
    int n = 0;
    for (int i = 0; s_pointer_count > i && 2 > n; i++) {
        if (INPUT_POINTER_HELD == s_pointers[i].state && OWNER_PINCH == s_pointers[i].owner) pair[n++] = &s_pointers[i];
    }
    if (2 > n) {
        s_pinch[0] = s_pinch[1] = 0;
        return;
    }
    float d    = Vector2Distance(pair[0]->position, pair[1]->position);
    bool  same = s_pinch[0] == pair[0]->handle && s_pinch[1] == pair[1]->handle;
    if (same && 1.0f < s_pinch_prev_dist && 1.0f < d && !s_gestures_blocked) camera_zoom_by(d / s_pinch_prev_dist);
    s_pinch[0]        = pair[0]->handle;
    s_pinch[1]        = pair[1]->handle;
    s_pinch_prev_dist = d;
}

input_pointer_state_e input_pointer_at(int pointer, Vector2* position) {
    assert(position);
    const pointer_t* p = find_handle(pointer);
    if (NULL == p) return INPUT_POINTER_GONE;
    *position = p->position;
    return p->state;
}

bool input_pointer_down(void) { return ui_pointer_held(); }

Vector2 input_pointer_position(void) {
    const pointer_t* p = find_handle(s_ui_handle);
    return NULL != p ? p->position : GetMousePosition();
}

bool input_pointer_touch(void) {
    const pointer_t* p = find_handle(s_ui_handle);
    return NULL != p && p->touch;
}

bool input_pointer_cancelled(void) {
    const pointer_t* p = find_handle(s_ui_handle);
    return NULL != p && INPUT_POINTER_CANCELLED == p->state;
}

int input_ui_pointers(Vector2 positions[2]) {
    int n = 0;
    for (int i = 0; s_pointer_count > i && 2 > n; i++) {
        if (INPUT_POINTER_HELD == s_pointers[i].state && OWNER_UI == s_pointers[i].owner) positions[n++] = s_pointers[i].position;
    }
    return n;
}

const pointer_gesture_t* input_joystick(void) {
    const pointer_t* p = first_held(OWNER_JOYSTICK);
    return NULL != p ? &p->gesture : NULL;
}

void input_queue_on_tick(input_queue_t* q, double dt, input_hit_fn ui_covers, input_claim_fn claim) {
    assert(q);
    assert(ui_covers);
    assert(claim);

    s_walked = false;
    sample(q, ui_covers, claim);
    hold_world(dt);
    pinch_zoom();

    /* The joystick steers while it is held; the keys steer otherwise. A world
     * press that may still tap, or a pinch, holds the keys back. */
    const pointer_gesture_t* joystick = input_joystick();
    int steer_x = NULL != joystick ? joystick->dir_x : axis_dir(KEY_LEFT, KEY_A, KEY_RIGHT, KEY_D);
    int steer_y = NULL != joystick ? joystick->dir_y : axis_dir(KEY_UP,   KEY_W, KEY_DOWN,  KEY_S);
    bool turned = steer_x != s_steer_x || steer_y != s_steer_y;
    /* A heading that turned to zero still owes one stop command. */
    bool refresh = (0 != steer_x || 0 != steer_y) &&
                   s_steer_age >= (double)local_player_action_cooldown();
    bool held_back = NULL == joystick && (NULL != first_held(OWNER_WORLD) || NULL != first_held(OWNER_PINCH));

    s_steer_age += dt;
    if ((turned || refresh) && !s_walked && !s_gestures_blocked && !held_back) {
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
        if (INPUT_TAP != evt.type || evt.select_only) continue;
        float cell = world_cell_size();
        client_event_payload_t p = {
            .target_x = evt.world_position.x / cell,
            .target_y = evt.world_position.y / cell,
        };
        client_event_push(CLIENT_EVENT_PLAYER_ACTION, p);
    }
}
