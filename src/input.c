#include "input.h"

// TODO: URGENTLY remove dependencies
#include "ui/interaction_bubble.h"
#include "ui/inventory_bar.h"
#include "ui/inventory_modal.h"
#include "ui/modal_dialogue.h"
#include "ui/tap_effect.h"

// TODO: Remove deps
#include "network/client.h"
#include "game_render.h"
#include "js/interact_bridge.h"

// Tick-based input pipeline. input.c builds typed InputCommand instances
// and pushes them to prediction; uplink uses network_send_event_tap with
// tick+sequence headers for prediction reconciliation.
#include "input/input_command.h"
#include "prediction/prediction.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void input_handle_tap(InputEvent event);
static bool input_event_queue_push(InputEvent event);
static void input_handle_key_press(int key);
static bool input_send_tap(Vector2 target_pos);
static bool input_is_over_ui(Vector2 screen_pos);

InputManager g_input = {0};

void input_update(void) {
    Vector2 mouse_screen_pos = GetMousePosition();

    // Handle mouse clicks
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        InputEvent event = {
            .type = INPUT_EVENT_TAP,
            .screen_position = mouse_screen_pos,
            .world_position = GetScreenToWorld2D(mouse_screen_pos, g_game_state.camera),
            .timestamp = GetTime()
        };
        input_event_queue_push(event);
    }

    // Handle mouse wheel
    float wheel_move = GetMouseWheelMove();
    if (wheel_move != 0) {
        InputEvent event = {
            .type = wheel_move >= 0 ? INPUT_EVENT_ZOOM_IN : INPUT_EVENT_ZOOM_OUT,
            .timestamp = GetTime()
        };
        input_event_queue_push(event);
    }

    // Handle keyboard input
    int key = GetKeyPressed();
    if (key != 0) {
        input_handle_key_press(key);
    }
}

void input_event_queue_handle(void) {
    for (uint32_t i = 0; i < g_input.event_count; i++) {
        InputEvent event = g_input.event_queue[i];

        switch (event.type) {
            case INPUT_EVENT_TAP:
                input_handle_tap(event);
                break;
            case INPUT_EVENT_TOGGLE_DEBUG:
                game_state_toggle_debug_mode();
                break;

            case INPUT_EVENT_ZOOM_IN:
                game_state_set_camera_zoom(g_game_state.camera.zoom * 1.1f);
                break;

            case INPUT_EVENT_ZOOM_OUT:
                game_state_set_camera_zoom(g_game_state.camera.zoom * 0.9f);
                break;

            case INPUT_EVENT_NONE:
            default:
                break;
        }
    }

    // Clear processed events
    g_input.event_count = 0;
}

static bool input_event_queue_push(InputEvent event) {
    if (event.type == INPUT_EVENT_NONE || g_input.event_count >= 32 )
    {
        return false;
    }

    g_input.event_queue[g_input.event_count] = event;
    g_input.event_count++;

    return true;
}

static void input_handle_tap(InputEvent event) {
    Vector2 screen_pos = event.screen_position;
    int mx = (int)screen_pos.x;
    int my = (int)screen_pos.y;

    TapEffectParams tap_fx = tap_effect_default_params();
    tap_fx.scale = 1.15f;
    tap_fx.duration = 0.42f;
    tap_fx.intensity = 1.25f;
    tap_fx.style_mask = TAP_EFFECT_STYLE_PREMIUM;
    tap_effect_spawn(screen_pos, &tap_fx);

    printf("[INPUT] Mouse click: button=%d, pos=(%.1f, %.1f)\n",
           MOUSE_BUTTON_LEFT, screen_pos.x, screen_pos.y);

    // Dialogue modal consumes all clicks when open (highest priority —
    // the JS interact panel hides itself while dialogue is active)
    if (modal_dialogue_handle_click(mx, my)) return;

    // JS interact panel consumes clicks when visible
    if (js_interact_overlay_is_open()) return; /* JS DOM handles the click */

    // Inventory modal consumes all clicks when open
    if (inventory_modal_handle_click(mx, my)) return;

    // Interaction bubble column: clicking opens the JS interact overlay
    if (interaction_bubble_handle_click(mx, my)) return;

    // Inventory bar tap: open modal or scroll
    {
        int hit = inventory_bar_get_tapped_slot(mx, my);
        if (hit >= 0) {
            inventory_modal_open(hit);
            return;
        }
    }

    // Zoom buttons (above inventory bar)
    {
        int zh = game_render_zoom_btn_hit(mx, my);
        if (zh != 0) {
            float factor = (zh > 0) ? 1.1f : 0.9f;
            game_state_set_camera_zoom(g_game_state.camera.zoom * factor);
            return;
        }
    }

    // Check if click is over UI
    if (input_is_over_ui(screen_pos)) {
        printf("[INPUT] Click over UI, ignoring world interaction\n");
        return;
    }

    input_send_tap(event.world_position);
}

static void input_handle_key_press(int key) {
    printf("[INPUT] Key pressed: %d\n", key);

    InputEvent event = { .timestamp = GetTime(), };

    switch (key) {
        case KEY_H:
            event.type = INPUT_EVENT_TOGGLE_HUD;
            input_event_queue_push(event);
            break;

        case KEY_F3:
            event.type = INPUT_EVENT_TOGGLE_DEBUG;
            input_event_queue_push(event);
            break;

        case KEY_ESCAPE:
            event.type = INPUT_EVENT_CANCEL_ACTION;
            input_event_queue_push(event);
            break;

        default:
            // Ignore other keys for now
            break;
    }
}

static bool input_send_tap(Vector2 target_pos) {
    // FrozenInteractionState — server says we're frozen, drop the tap.
    if (g_game_state.frozen) return 0;
    // Ghost / respawning state — server rejects all movement while dead.
    // Drop silently here to avoid filling the uplink with ignored commands.
    if (g_game_state.player.base.respawn_in > 0.0f) return 0;

    printf("[INPUT] TAP at world (%.2f, %.2f)\n", target_pos.x, target_pos.y);

    float cell = g_game_state.cell_size > 0 ? g_game_state.cell_size : 12.0f;
    float grid_x = target_pos.x / cell;
    float grid_y = target_pos.y / cell;

    // Canonical input pipeline:
    //   1. Build a typed InputCommand stamped with client_tick + sequence.
    //   2. Apply optimistically to prediction (local feedback + replay).
    //   3. Send the *same* tick + sequence on the wire so the server's
    //      snapshot echoes back lastAckedSequence and the client's replay
    //      buffer drains.  Skipping this here was the cause of the
    //      "tap → freeze / oscillation" symptom: the buffer grew unbounded
    //      and prediction_reconcile replayed every tap on every snapshot.
    input_command_t cmd = input_command_build_tap(grid_x, grid_y);
    prediction_apply(&cmd);

    g_game_state.player.tap_target = (Vector2){grid_x, grid_y};
    g_game_state.player.has_tap_target = true;

    return network_send_event_tap((Vector2){grid_x, grid_y}, cmd.client_tick, cmd.sequence);
}

static bool input_is_over_ui(Vector2 screen_pos) {
    // Dialogue modal blocks all world interaction when open
    if (modal_dialogue_is_open()) return true;
    // JS interact overlay blocks world interaction when open
    if (js_interact_overlay_is_open()) return true;
    // Inventory modal blocks all world interaction when open
    if (inventory_modal_is_open()) return true;
    // Interaction bubble column occupies left strip
    if (interaction_bubble_slot_count() > 0 &&
        screen_pos.x < IBUBBLE_MARGIN_X + IBUBBLE_ICON_SIZE + IBUBBLE_MARGIN_X)
    {
        return true;
    }
    // Inventory bar occupies the bottom strip
    if (screen_pos.y > GetScreenHeight() - INV_BAR_HEIGHT) return true;
    // Zoom buttons above the inventory bar
    if (0 != game_render_zoom_btn_hit((int)screen_pos.x, (int)screen_pos.y)) return true;
    return false;
}
