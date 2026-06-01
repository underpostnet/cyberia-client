#ifndef INPUT_H
#define INPUT_H

#include <raylib.h>
#include <stdbool.h>
#include <stdint.h>

// TODO: should remove
#include "input/input_command.h"

typedef enum input_type {
    INPUT_NONE,
    INPUT_TAP,
    INPUT_KEY_DEBUG,
    INPUT_ZOOM
} input_e;

typedef struct input_event {
    enum input_type type;
    Vector2 screen_position; // For INPUT_TAP
    bool zoom_in; // For INPUT_ZOOM
    Vector2 world_position;  // For INPUT_TAP
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

//TODO: Remove
bool command_queue_push(const input_command_t* cmd);
bool input_command_pop(input_command_t* out);
void input_command_queue_clear(void);

#endif // INPUT_H
