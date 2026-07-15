#ifndef CYBERIA_INPUT_H
#define CYBERIA_INPUT_H

#include <raylib.h>
#include <stdbool.h>
#include <stdint.h>

/* Raw per-frame input event ring. Captures OS events (tap, debug key, zoom)
 * and hands them to the UI dispatch and prediction/replication stages. The
 * typed wire command lives separately in input/input_command.h. */

typedef enum input_type {
    INPUT_NONE,
    INPUT_TAP,
    INPUT_KEY_DEBUG,
    INPUT_ZOOM
} input_e;

typedef struct input_event {
    enum input_type type;
    Vector2 screen_position; /* For INPUT_TAP */
    bool zoom_in;            /* For INPUT_ZOOM */
    float wheel_delta;       /* For INPUT_ZOOM */
    Vector2 world_position;  /* For INPUT_TAP */
} input_event_t;

#define Q_CAP 100
typedef struct input_event_queue {
    struct input_event evt[Q_CAP];
    uint32_t head;
    uint32_t count;
} input_queue_t;

void input_queue_on_tick(input_queue_t* q, double dt);
bool input_pop(input_queue_t* q, input_event_t* out);
void input_push(input_queue_t* q, input_event_t e);

/* While blocked, the capture layer runs no gameplay pinch zoom — a
 * full-screen UI surface (the Instance Map) owns touch gestures instead.
 * Taps, wheel, and debug-key events still flow. */
void input_gestures_set_blocked(bool blocked);

#endif /* CYBERIA_INPUT_H */
