#ifndef CYBERIA_JS_POINTER_BRIDGE_H
#define CYBERIA_JS_POINTER_BRIDGE_H

#include <stdint.h>

/* The pointer events of the canvas, mouse, touch and pen alike (Web only, via
 * inline EM_JS). input.c is their only reader. Each finger has its own browser
 * pointerId from its press to its release, so no touch array order applies.
 *
 * The canvas captures each pointer it presses. The bridge cancels a held
 * pointer when its contact is lost: a browser pointercancel, a lost pointer
 * capture, a focus loss, or a right button press while the left button holds
 * the mouse pointer. Lifecycle events are never dropped one by one: a queue
 * past its bound cancels every pointer C holds. */

typedef enum pointer_event_kind {
    POINTER_EVENT_DOWN,
    POINTER_EVENT_MOVE,
    POINTER_EVENT_UP,
    POINTER_EVENT_CANCEL,
} pointer_event_kind_e;

/* One event. Positions are canvas CSS pixels, which equal raylib screen pixels. */
typedef struct pointer_event {
    int32_t kind;  /* pointer_event_kind_e */
    int32_t id;    /* the browser pointerId */
    int32_t touch; /* 1 for a finger or a pen, 0 for the mouse */
    float   x;
    float   y;
} pointer_event_t;

/* Moves up to `max` events, oldest first, into `out`. Returns how many. The
 * moves of one pointer between two drains merge into the newest. */
int pointer_bridge_drain(pointer_event_t* out, int max);

#endif /* CYBERIA_JS_POINTER_BRIDGE_H */
