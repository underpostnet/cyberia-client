#include "input.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// TODO: Remove deps
#include "serial.h"
#include "client.h"
#include "inventory_bar.h"
#include "inventory_modal.h"
#include "modal_dialogue.h"
#include "dialogue_bubble.h"

// Global input manager instance
InputManager g_input = {0};

void input_update(void) {
    g_input.mouse_screen_pos = GetMousePosition();

    // Update mouse button states
    g_input.mouse_left_down = IsMouseButtonDown(MOUSE_BUTTON_LEFT);

    // Handle mouse clicks
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        input_handle_mouse_click(MOUSE_BUTTON_LEFT, g_input.mouse_screen_pos);
    }

    // Handle mouse wheel
    float wheel_move = GetMouseWheelMove();
    if (wheel_move != 0) {
        input_handle_mouse_wheel(wheel_move);
    }

    // Handle keyboard input
    int key = GetKeyPressed();
    if (key != 0) {
        input_handle_key_press(key);
    }

    // Process queued events
    input_process_events();
}

void input_cleanup(void) {
    // Clear input state
    memset(&g_input, 0, sizeof(InputManager));
}

void input_process_events(void) {
    for (int i = 0; i < g_input.event_count; i++) {
        InputEvent* event = &g_input.event_queue[i];

        switch (event->type) {
            case INPUT_EVENT_TAP:
                input_send_tap(event->world_position);
                break;
            case INPUT_EVENT_TOGGLE_DEBUG:
                input_toggle_debug_mode();
                break;

            case INPUT_EVENT_ZOOM_IN:
                input_set_camera_zoom(g_game_state.camera_zoom * 1.1f);
                break;

            case INPUT_EVENT_ZOOM_OUT:
                input_set_camera_zoom(g_game_state.camera_zoom * 0.9f);
                break;

            case INPUT_EVENT_NONE:
            default:
                break;
        }
    }

    // Clear processed events
    input_clear_events();
}

int input_add_event(const InputEvent* event) {
    if (!event || g_input.event_count >= 32) {
        return -1; // Queue full or invalid event
    }

    g_input.event_queue[g_input.event_count] = *event;
    g_input.event_count++;

    return 0;
}

void input_clear_events(void) {
    g_input.event_count = 0;
}

Vector2 input_get_mouse_world_pos(void) {
    return GetScreenToWorld2D(g_input.mouse_screen_pos, g_game_state.camera);
}

void input_handle_mouse_click(int button, Vector2 screen_pos) {
    printf("[INPUT] Mouse click: button=%d, pos=(%.1f, %.1f)\n",
           button, screen_pos.x, screen_pos.y);

    int mx = (int)screen_pos.x;
    int my = (int)screen_pos.y;

    // Dialogue modal consumes all clicks when open (highest priority)
    if (modal_dialogue_is_open()) {
        modal_dialogue_handle_click(mx, my, true);
        return;
    }

    // Inventory modal consumes all clicks when open
    if (inventory_modal_is_open()) {
        inventory_modal_handle_click(mx, my, true);
        return;
    }

    // Dialogue bubble column: clicking an icon opens dialogue modal
    if (button == MOUSE_BUTTON_LEFT) {
        dialogue_bubble_handle_click(mx, my, true);
        if (modal_dialogue_is_open()) return;
    }

    // Inventory bar tap: open modal or scroll
    if (button == MOUSE_BUTTON_LEFT) {
        int hit = inventory_bar_get_tapped_slot(mx, my);
        if (hit >= 0) {
            inventory_modal_open(hit);
            return;
        }
    }

    Vector2 world_pos = GetScreenToWorld2D(screen_pos, g_game_state.camera);

    // Check if click is over UI
    if (input_is_over_ui(screen_pos)) {
        printf("[INPUT] Click over UI, ignoring world interaction\n");
        return;
    }

    if (button == MOUSE_BUTTON_LEFT) {
        // Every left click is a TAP — the fundamental game event.
        // The server independently computes skills (probability-based) and
        // movement (agility-based rendering effect) from each TAP.
        InputEvent event = {0};
        event.type = INPUT_EVENT_TAP;
        event.screen_position = screen_pos;
        event.world_position = world_pos;
        event.timestamp = GetTime();
        input_add_event(&event);
    }
}

void input_handle_mouse_wheel(float wheel_move) {
    InputEvent event = {0};
    event.timestamp = GetTime();
    if (wheel_move > 0) {
        event.type = INPUT_EVENT_ZOOM_IN;
    } else if (wheel_move < 0) {
        event.type = INPUT_EVENT_ZOOM_OUT;
    }
    input_add_event(&event);
}

void input_handle_key_press(int key) {
    printf("[INPUT] Key pressed: %d\n", key);

    InputEvent event = {0};
    event.timestamp = GetTime();

    switch (key) {
        case KEY_H:
            event.type = INPUT_EVENT_TOGGLE_HUD;
            input_add_event(&event);
            break;

        case KEY_F3:
            event.type = INPUT_EVENT_TOGGLE_DEBUG;
            input_add_event(&event);
            break;

        case KEY_ESCAPE:
            event.type = INPUT_EVENT_CANCEL_ACTION;
            input_add_event(&event);
            break;

        default:
            // Ignore other keys for now
            break;
    }
}

void input_handle_window_resize(int width, int height) {
    printf("[INPUT] Window resized to %dx%d\n", width, height);

    // Update camera offset to keep it centered
    g_game_state.camera.offset.x = width / 2.0f;
    g_game_state.camera.offset.y = height / 2.0f;
}

int input_send_tap(Vector2 target_pos) {
    printf("[INPUT] TAP at world (%.2f, %.2f)\n", target_pos.x, target_pos.y);

    float grid_x = target_pos.x / (g_game_state.cell_size > 0 ? g_game_state.cell_size : 12.0f);
    float grid_y = target_pos.y / (g_game_state.cell_size > 0 ? g_game_state.cell_size : 12.0f);

    char* json_str = serial_create_player_action(grid_x, grid_y);
    if (json_str) {
        int result = client_send(json_str);
        free(json_str);
        return result;
    }

    return -1;
}

void input_toggle_debug_mode(void) {
    g_game_state.dev_ui = !g_game_state.dev_ui;
    printf("[INPUT] Debug mode %s\n", g_game_state.dev_ui ? "enabled" : "disabled");
}

void input_set_camera_zoom(float zoom) {
    // Clamp zoom to reasonable range
    if (zoom < 0.1f) zoom = 0.1f;
    if (zoom > 5.0f) zoom = 5.0f;

    g_game_state.camera_zoom = zoom;
    g_game_state.camera.zoom = zoom;

    printf("[INPUT] Camera zoom set to %.2f\n", zoom);
}


bool input_find_entity_at_position(Vector2 screen_pos, char* entity_id, size_t id_size) {
    Vector2 world_pos = GetScreenToWorld2D(screen_pos, g_game_state.camera);
    float cell_size = g_game_state.cell_size > 0 ? g_game_state.cell_size : 12.0f;

    // Check main player
    Rectangle player_rect = {
        g_game_state.player.base.interp_pos.x * cell_size,
        g_game_state.player.base.interp_pos.y * cell_size,
        g_game_state.player.base.dims.x * cell_size,
        g_game_state.player.base.dims.y * cell_size
    };

    if (CheckCollisionPointRec(world_pos, player_rect)) {
        strncpy(entity_id, g_game_state.player.base.id, id_size - 1);
        entity_id[id_size - 1] = '\0';
        return true;
    }

    // Check other players
    for (int i = 0; i < g_game_state.other_player_count; i++) {
        PlayerState* player = &g_game_state.other_players[i];
        Rectangle rect = {
            player->base.interp_pos.x * cell_size,
            player->base.interp_pos.y * cell_size,
            player->base.dims.x * cell_size,
            player->base.dims.y * cell_size
        };

        if (CheckCollisionPointRec(world_pos, rect)) {
            strncpy(entity_id, player->base.id, id_size - 1);
            entity_id[id_size - 1] = '\0';
            return true;
        }
    }

    // Check bots
    for (int i = 0; i < g_game_state.bot_count; i++) {
        BotState* bot = &g_game_state.bots[i];
        Rectangle rect = {
            bot->base.interp_pos.x * cell_size,
            bot->base.interp_pos.y * cell_size,
            bot->base.dims.x * cell_size,
            bot->base.dims.y * cell_size
        };

        if (CheckCollisionPointRec(world_pos, rect)) {
            strncpy(entity_id, bot->base.id, id_size - 1);
            entity_id[id_size - 1] = '\0';
            return true;
        }
    }
    return false;
}

bool input_is_over_ui(Vector2 screen_pos) {
    // Dialogue modal blocks all world interaction when open
    if (modal_dialogue_is_open()) return true;
    // Inventory modal blocks all world interaction when open
    if (inventory_modal_is_open()) return true;
    // Dialogue bubble column occupies left strip
    if (dialogue_bubble_slot_count() > 0 &&
        screen_pos.x < DBUBBLE_MARGIN_X + DBUBBLE_ICON_SIZE + DBUBBLE_MARGIN_X) return true;
    // Inventory bar occupies the bottom strip
    if (screen_pos.y > GetScreenHeight() - INV_BAR_HEIGHT) return true;
    return false;
}
