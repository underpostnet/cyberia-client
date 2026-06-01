#include "input.h"

#include "util/log.h"

#include <assert.h>
#include <stdint.h>
#include <raymath.h>

// TODO: should remove
#include "domain/camera.h"
#include "input/input_command.h"


void input_push(struct input_event_queue* q, struct input_event e) {
    assert(q);
    assert(q->count < Q_CAP);
    q->evt[(q->head + q->count) % Q_CAP] = e;
    q->count++;
}

bool input_pop(struct input_event_queue* q, struct input_event* out) {
    assert(q);
    assert(out);
    if (0 == q->count) { return false; }
    *out = q->evt[q->head];
    q->head = (q->head + 1) % Q_CAP;
    q->count--;
    return true;
}

void input_queue_on_tick(input_queue_t* q, double dt) {
    assert(q);

    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        Vector2 mouse = GetMousePosition();
        Camera2D cam = camera_get();
        input_push(q, (struct input_event){
            .type = INPUT_TAP,
            .screen_position = mouse,
            .world_position = GetScreenToWorld2D(mouse, cam),
        });
    }

    switch (GetKeyPressed()) {
        case KEY_F3:
            input_push(q, (struct input_event){
                .type = INPUT_KEY_DEBUG
            });
            break;
        default: break;
    }

    float wheel = GetMouseWheelMove();
    if (!FloatEquals(wheel, 0.0f)) {
        input_push(q, (struct input_event){
            .type = INPUT_ZOOM,
            .zoom_in = wheel > 0
        });
    }
}


#define COMMAND_QUEUE_CAP 32

typedef struct {
    input_command_t items[COMMAND_QUEUE_CAP];
    int head;
    int count;
} command_queue_t;

static command_queue_t s_cmd_q = {0};

bool command_queue_push(const input_command_t* cmd) {
    if (s_cmd_q.count == COMMAND_QUEUE_CAP) {
        LOG_WARN("input command queue full, dropping oldest");
        s_cmd_q.head = (s_cmd_q.head + 1) % COMMAND_QUEUE_CAP;
        s_cmd_q.count--;
    }
    int idx = (s_cmd_q.head + s_cmd_q.count) % COMMAND_QUEUE_CAP;
    s_cmd_q.items[idx] = *cmd;
    s_cmd_q.count++;
    return true;
}

bool input_command_pop(input_command_t* out) {
    if (s_cmd_q.count == 0) return false;
    *out = s_cmd_q.items[s_cmd_q.head];
    s_cmd_q.head = (s_cmd_q.head + 1) % COMMAND_QUEUE_CAP;
    s_cmd_q.count--;
    return true;
}

void input_command_queue_clear(void) {
    s_cmd_q.head = 0;
    s_cmd_q.count = 0;
}
