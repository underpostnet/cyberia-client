#include "input.h"

#include "config.h"
#include "domain/camera.h"
#include "domain/local_player.h"
#include "game_state.h"
#include "input/input_command.h"
#include "network/game_client.h"
#include "ui/tap_effect.h"
#include "ui/ui_dispatch.h"
#include "util/log.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Input module — produces typed InputCommand frames from raw hardware
 * events. It is now a strict leaf: the UI absorbs taps through the
 * ui_dispatch facade, and prediction consumes commands during its own
 * fixed-timestep step (not inline from input handling).
 *
 * Dependency direction:
 *
 *   input.c → input_command_t queue
 *           → camera (for zoom button delta)
 *           → ui_dispatch_tap (single facade, no per-module includes)
 *           → network_send_event_tap (uplink)
 *
 * Prediction reads the queue inside prediction_step / input_command_drain.
 */

InputManager g_input = {0};

#define COMMAND_QUEUE_CAP 32

typedef struct {
    input_command_t items[COMMAND_QUEUE_CAP];
    int head;
    int count;
} command_queue_t;

static command_queue_t s_cmd_q = {0};

static bool input_event_queue_push(InputEvent event) {
    if (event.type == INPUT_EVENT_NONE || g_input.event_count >= 32) return false;
    g_input.event_queue[g_input.event_count++] = event;
    return true;
}

static bool command_queue_push(const input_command_t* cmd) {
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

int input_command_queue_count(void) { return s_cmd_q.count; }

void input_command_queue_clear(void) {
    s_cmd_q.head = 0;
    s_cmd_q.count = 0;
}

static void handle_tap_event(InputEvent event) {
    int mx = (int)event.screen_position.x;
    int my = (int)event.screen_position.y;

    TapEffectParams fx = tap_effect_default_params();
    fx.scale = 1.15f;
    fx.duration = 0.42f;
    fx.intensity = 1.25f;
    fx.style_mask = TAP_EFFECT_STYLE_PREMIUM;
    tap_effect_spawn(event.screen_position, &fx);

    /* Ask the UI layer first; if it claimed the tap, the world never sees
     * it. This is the single point of contact between input and UI. */
    if (ui_dispatch_tap(mx, my))            return;
    if (ui_dispatch_covers_point(mx, my))   return;

    /* FrozenInteractionState — server says we're frozen, drop the tap. */
    if (local_player_is_frozen()) return;
    if (g_game_state.player.base.respawn_in > 0.0f) return;

    float cell = g_game_state.cell_size > 0.0f ? g_game_state.cell_size : 12.0f;
    float gx = event.world_position.x / cell;
    float gy = event.world_position.y / cell;

    input_command_t cmd = input_command_build_tap(gx, gy);
    command_queue_push(&cmd);

    g_game_state.player.tap_target     = (Vector2){gx, gy};
    g_game_state.player.has_tap_target = true;

    /* The server uplink stays in input.c because the wire frame must be
     * stamped with the exact same client_tick + sequence the local
     * prediction will replay against. */
    network_send_event_tap((Vector2){gx, gy}, cmd.client_tick, cmd.sequence);
}

static void handle_key_press(int key) {
    InputEvent event = { .timestamp = GetTime() };
    switch (key) {
        case KEY_H:      event.type = INPUT_EVENT_TOGGLE_HUD;    input_event_queue_push(event); break;
        case KEY_F3:     event.type = INPUT_EVENT_TOGGLE_DEBUG;  input_event_queue_push(event); break;
        case KEY_ESCAPE: event.type = INPUT_EVENT_CANCEL_ACTION; input_event_queue_push(event); break;
        default: break;
    }
}

void input_update(void) {
    Vector2 mouse = GetMousePosition();

    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        Camera2D cam = camera_get();
        InputEvent ev = {
            .type            = INPUT_EVENT_TAP,
            .screen_position = mouse,
            .world_position  = GetScreenToWorld2D(mouse, cam),
            .timestamp       = GetTime(),
        };
        input_event_queue_push(ev);
    }

    float wheel = GetMouseWheelMove();
    if (wheel != 0.0f) {
        InputEvent ev = {
            .type      = (wheel >= 0.0f) ? INPUT_EVENT_ZOOM_IN : INPUT_EVENT_ZOOM_OUT,
            .timestamp = GetTime(),
        };
        input_event_queue_push(ev);
    }

    int key = GetKeyPressed();
    if (key != 0) handle_key_press(key);
}

void input_event_queue_handle(void) {
    for (uint32_t i = 0; i < g_input.event_count; i++) {
        InputEvent ev = g_input.event_queue[i];
        switch (ev.type) {
            case INPUT_EVENT_TAP:           handle_tap_event(ev);                   break;
            case INPUT_EVENT_TOGGLE_DEBUG:  game_state_toggle_dev_ui();             break;
            case INPUT_EVENT_ZOOM_IN:       camera_zoom_by(1.1f);                   break;
            case INPUT_EVENT_ZOOM_OUT:      camera_zoom_by(0.9f);                   break;
            case INPUT_EVENT_TOGGLE_HUD:    /* handled by HUD module if present */  break;
            case INPUT_EVENT_CANCEL_ACTION: /* reserved */                          break;
            case INPUT_EVENT_NONE:
            default: break;
        }
    }
    g_input.event_count = 0;
}
