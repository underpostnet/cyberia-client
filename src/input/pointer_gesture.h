#ifndef CYBERIA_POINTER_GESTURE_H
#define CYBERIA_POINTER_GESTURE_H

#include <raylib.h>
#include <stdbool.h>

/* The gesture of one press on the world: a tap at its press, or a floating
 * joystick once it is held and dragged. Pure state: input.c owns the pointers
 * and acts on the result. */

/* Joystick base radius in screen pixels. The thumb stays inside it. */
#define POINTER_JOYSTICK_RADIUS      72.0f
/* A press becomes the joystick once it is held this long and travels this
 * far. Released sooner, it taps at its press, however far it travelled. */
#define POINTER_JOYSTICK_HOLD_S      0.15
#define POINTER_JOYSTICK_ACTIVATE_PX 35.0f

typedef struct pointer_gesture {
    bool    steering; /* the press is the joystick; its base stays at the origin */
    double  age;      /* seconds since the press */
    Vector2 origin;   /* press position: the tap position and the joystick base */
    Vector2 thumb;    /* pointer position, clamped to the radius */
    int     dir_x;    /* 8-way heading, -1..1, as the arrow keys give it */
    int     dir_y;    /* screen y grows down: up is -1 */
} pointer_gesture_t;

/* The gesture of a new press at `origin`. */
pointer_gesture_t pointer_gesture_start(Vector2 origin);

/* One frame of the held press at `position`. A press that may steer becomes
 * the joystick past the hold time and the travel threshold. */
void pointer_gesture_hold(pointer_gesture_t* g, Vector2 position, double dt, bool may_steer);

#endif /* CYBERIA_POINTER_GESTURE_H */
