#include "input_command.h"
#include "input.h"
#include "network/session.h"
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

void input_queue_on_tick(input_queue_t* q, double dt) {
    assert(q);

    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        Vector2 mouse = GetMousePosition();
        Camera2D cam = camera_get();
        input_push(q, (input_event_t){
            .type = INPUT_TAP,
            .screen_position = mouse,
            .world_position = GetScreenToWorld2D(mouse, cam),
        });
    }

    switch (GetKeyPressed()) {
        case KEY_F3:
            input_push(q, (input_event_t){ .type = INPUT_KEY_DEBUG });
            break;
        default: break;
    }

    float wheel = GetMouseWheelMove();
    if (!FloatEquals(wheel, 0.0f)) {
        input_push(q, (input_event_t){ .type = INPUT_ZOOM, .zoom_in = wheel > 0 });
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
