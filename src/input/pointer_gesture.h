#ifndef CYBERIA_POINTER_GESTURE_H
#define CYBERIA_POINTER_GESTURE_H

#include <raylib.h>
#include <stdbool.h>

/* Pointer gesture arbitration. One pointer, a finger or the mouse, taps or
 * drags a floating joystick; two fingers pinch. Pure state: input.c samples
 * raylib and acts on the result. */

/* Joystick base radius in screen pixels. The thumb stays inside it. */
#define POINTER_JOYSTICK_RADIUS      72.0f
/* A gameplay press starts the joystick once it is held this long and travels
 * this far. Released sooner, it taps at its press, however far it travelled. */
#define POINTER_JOYSTICK_HOLD_S      0.15
#define POINTER_JOYSTICK_ACTIVATE_PX 35.0f

typedef enum pointer_state {
    POINTER_IDLE,
    POINTER_UI_PENDING, /* UI press: tap on release or after the confirm window */
    POINTER_PENDING,    /* gameplay press: tap on release, joystick on drag */
    POINTER_JOYSTICK,   /* the pointer steers; the base stays at the origin */
    POINTER_SUPPRESSED, /* pinch or lost pointer: nothing until every contact lifts */
} pointer_state_e;

typedef enum pointer_result {
    POINTER_NONE,
    POINTER_TAP,        /* one tap at the origin */
    POINTER_STEER_END,  /* the joystick stops steering */
} pointer_result_e;

/* One frame of the pointer: touch 0 while a finger is down, else the mouse. */
typedef struct pointer_sample {
    int     count;    /* contacts down: fingers, or 1 while the mouse button is down */
    int     id;       /* identifier of the pointer */
    Vector2 position; /* position of the pointer */
    bool    pressed;  /* the pointer went down this frame */
    bool    ui_owned; /* the UI owns the pressed position */
    bool    touch;    /* the pointer is a finger, so a press can grow into a pinch */
} pointer_sample_t;

typedef struct pointer_gesture {
    enum pointer_state state;
    int     id;       /* identifier of the pointer that started the gesture */
    double  age;      /* pending states: seconds since the press */
    Vector2 origin;   /* press position: the joystick base */
    Vector2 tap;      /* POINTER_TAP: the position tapped */
    Vector2 thumb;    /* pointer position, clamped to the radius */
    int     dir_x;    /* 8-way heading, -1..1, as the arrow keys give it */
    int     dir_y;    /* screen y grows down: up is -1 */
} pointer_gesture_t;

enum pointer_result pointer_gesture_update(pointer_gesture_t* g, pointer_sample_t s, double dt);

#endif /* CYBERIA_POINTER_GESTURE_H */
