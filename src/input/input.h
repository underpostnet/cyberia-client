#ifndef CYBERIA_INPUT_H
#define CYBERIA_INPUT_H

#include <raylib.h>
#include <stdbool.h>
#include <stdint.h>

#include "pointer_gesture.h"

/* Raw per-frame input event ring. Captures OS events (tap, debug key, zoom)
 * and hands them to the UI dispatch and prediction/replication stages. Raw
 * input never leaves the client: input_push_client_events converts it into
 * the client events of net/client_event.h. */

typedef enum input_type {
    INPUT_NONE,
    INPUT_TAP,
    INPUT_KEY_DEBUG,
    INPUT_KEY_ESCAPE,
    INPUT_ZOOM
} input_e;

typedef struct input_event {
    enum input_type type;
    Vector2 screen_position; /* For INPUT_TAP */
    bool zoom_in;            /* For INPUT_ZOOM */
    float wheel_delta;       /* For INPUT_ZOOM */
    Vector2 world_position;  /* For INPUT_TAP */
    /* For INPUT_TAP: steering (keys or joystick) made this tap. No
     * pointer is on the target pixel, so the UI layer does no hit test on it
     * and the presentation layer draws no tap effect for it. */
    bool synthetic;
    /* For INPUT_TAP: the joystick steers, so the tap selects and does not walk. */
    bool select_only;
} input_event_t;

#define Q_CAP 100
typedef struct input_event_queue {
    struct input_event evt[Q_CAP];
    uint32_t head;
    uint32_t count;
} input_queue_t;

/* ── Pointer router ──────────────────────────────────────────────────────
 * The one reader of the pointers (js/pointer_bridge.h). Each press of a
 * finger or the mouse gets a handle and an owner when it lands, and keeps both
 * until it lifts or is cancelled. Another pointer never changes either. A new
 * press goes to the first owner that takes it:
 *
 *   1. UI, while a full-screen surface blocks the gestures.
 *   2. CLAIMED, when the claim function takes it: a combat control.
 *   3. UI, when the interface covers it.
 *   4. WORLD: it taps at its press on release. Held and dragged while no other
 *      pointer steers, it becomes the JOYSTICK. While the joystick steers, its
 *      tap selects and does not walk.
 *   5. PINCH: two world presses that are not the joystick zoom the camera. A
 *      pinch press stays inert until it lifts.
 *
 * A cancelled press acts on nothing: no tap, no cast. The joystick still
 * sends its stop. Keys never depend on a pointer owner. */

/* True when the UI covers the screen pixel. */
typedef bool (*input_hit_fn)(int screen_x, int screen_y);

/* True when a surface takes the new press `pointer` (a handle) at `position`. */
typedef bool (*input_claim_fn)(int pointer, Vector2 position);

/* Capture one frame of raw input and route each pointer event. */
void input_queue_on_tick(input_queue_t* q, double dt, input_hit_fn ui_covers, input_claim_fn claim);

typedef enum input_pointer_state {
    INPUT_POINTER_GONE,      /* no such press, or it ended in an earlier frame */
    INPUT_POINTER_HELD,
    INPUT_POINTER_RELEASED,  /* lifted this frame */
    INPUT_POINTER_CANCELLED, /* lost this frame: nothing may act on it */
} input_pointer_state_e;

/* The state of a press this frame, and its last position. A claimer follows
 * its own press with it. */
input_pointer_state_e input_pointer_at(int pointer, Vector2* position);

/* The UI pointer: the UI press that sent the last UI tap. A UI press while it
 * is held belongs to the UI too, but sends no tap and never becomes the UI
 * pointer. These three read only the UI pointer, never a joystick, combat or
 * world press. */
bool input_pointer_down(void);
/* Its position while held and in the frame it ends; else the mouse position. */
Vector2 input_pointer_position(void);
bool input_pointer_touch(void);
/* True in the frame the UI pointer is cancelled. A UI surface then rolls its
 * press back: a cancelled pointer commits nothing on its end. */
bool input_pointer_cancelled(void);

/* The positions of the first two held UI presses, in press order. Returns how
 * many (0 to 2): two make the pinch of a full-screen surface. */
int input_ui_pointers(Vector2 positions[2]);

/* The joystick gesture, or NULL while no pointer steers. */
const pointer_gesture_t* input_joystick(void);

bool input_pop(input_queue_t* q, input_event_t* out);
void input_push(input_queue_t* q, input_event_t e);

/* Drain a (deep-copied) frame queue: each tap that walks pushes one
 * player_action client event and enters the prediction queue. Every other raw
 * event pushes nothing. */
void input_push_client_events(input_queue_t q);

/* True if the event is consumed and must leave the queue. */
typedef bool (*input_consume_fn)(const input_event_t* e);

/* Drains `q`, drops every event `consume` accepts, and refills `q` with the
 * rest in their original order. */
void input_queue_filter(input_queue_t* q, input_consume_fn consume);

/* While blocked, a full-screen UI surface (the Instance Map) owns every new
 * press: no gameplay pinch zoom, no new joystick, no steering. Wheel and
 * debug-key events still flow. */
void input_gestures_set_blocked(bool blocked);

#endif /* CYBERIA_INPUT_H */
