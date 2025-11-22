#include "input.h"
#include "game_state.h"
#include "message_parser.h"
#include "client.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Global input manager instance
InputManager g_input = {0};

int input_init(void) {
    printf("[INPUT] Initializing input system...\n");
    
    // Initialize input manager state
    memset(&g_input, 0, sizeof(InputManager));
    
    // Set default settings
    g_input.double_click_time = 500.0f; // 500ms
    g_input.drag_threshold = 5.0f;      // 5 pixels
    g_input.invert_zoom = false;
    g_input.zoom_target = 1.0f;
    g_input.zoom_speed = 0.1f;
    
    // Initialize mouse state
    g_input.mouse_screen_pos = (Vector2){0, 0};
    g_input.mouse_world_pos = (Vector2){0, 0};
    g_input.mouse_left_down = false;
    g_input.mouse_right_down = false;
    g_input.mouse_middle_down = false;
    
    // Initialize keyboard state
    g_input.key_shift_down = false;
    g_input.key_ctrl_down = false;
    g_input.key_alt_down = false;
    
    // Initialize camera control
    g_input.camera_dragging = false;
    g_input.camera_drag_start = (Vector2){0, 0};
    
    // Clear event queue
    g_input.event_count = 0;
    
    printf("[INPUT] Input system initialized successfully\n");
    return 0;
}

void input_update(void) {
    // Update mouse position
    g_input.mouse_screen_pos = GetMousePosition();
    g_input.mouse_world_pos = input_get_mouse_world_pos();
    
    // Update mouse button states
    g_input.mouse_left_down = IsMouseButtonDown(MOUSE_BUTTON_LEFT);
    g_input.mouse_right_down = IsMouseButtonDown(MOUSE_BUTTON_RIGHT);
    g_input.mouse_middle_down = IsMouseButtonDown(MOUSE_BUTTON_MIDDLE);
    
    // Update keyboard modifier states
    g_input.key_shift_down = IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT);
    g_input.key_ctrl_down = IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL);
    g_input.key_alt_down = IsKeyDown(KEY_LEFT_ALT) || IsKeyDown(KEY_RIGHT_ALT);
    
    // Handle mouse clicks
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        input_handle_mouse_click(MOUSE_BUTTON_LEFT, g_input.mouse_screen_pos);
    }
    if (IsMouseButtonPressed(MOUSE_BUTTON_RIGHT)) {
        input_handle_mouse_click(MOUSE_BUTTON_RIGHT, g_input.mouse_screen_pos);
    }
    if (IsMouseButtonPressed(MOUSE_BUTTON_MIDDLE)) {
        input_handle_mouse_click(MOUSE_BUTTON_MIDDLE, g_input.mouse_screen_pos);
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
    
    // Update camera dragging
    if (g_input.camera_dragging) {
        if (!g_input.mouse_middle_down) {
            // Stop dragging
            g_input.camera_dragging = false;
        } else {
            // Continue dragging - move camera
            Vector2 drag_delta = {
                g_input.mouse_screen_pos.x - g_input.camera_drag_start.x,
                g_input.mouse_screen_pos.y - g_input.camera_drag_start.y
            };
            input_pan_camera((Vector2){-drag_delta.x, -drag_delta.y});
            g_input.camera_drag_start = g_input.mouse_screen_pos;
        }
    }
    
    // Process queued events
    input_process_events();
}

void input_cleanup(void) {
    printf("[INPUT] Cleaning up input system...\n");
    
    // Clear input state
    memset(&g_input, 0, sizeof(InputManager));
    
    printf("[INPUT] Input system cleanup complete\n");
}

void input_process_events(void) {
    for (int i = 0; i < g_input.event_count; i++) {
        InputEvent* event = &g_input.event_queue[i];
        
        switch (event->type) {
            case INPUT_EVENT_MOVE_TO:
                input_send_player_move(event->world_position);
                break;
                
            case INPUT_EVENT_INTERACT:
                input_send_interaction(event->target_id);
                break;
                
            case INPUT_EVENT_USE_SKILL:
                // TODO: Implement skill activation
                printf("[INPUT] Skill activation not implemented yet\n");
                break;
                
            case INPUT_EVENT_TOGGLE_HUD:
                input_toggle_hud();
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
                
            case INPUT_EVENT_CANCEL_ACTION:
                // TODO: Implement action cancellation
                printf("[INPUT] Action cancellation not implemented yet\n");
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
    if (!g_game_state.camera_initialized) {
        return g_input.mouse_screen_pos;
    }
    
    return GetScreenToWorld2D(g_input.mouse_screen_pos, g_game_state.camera);
}

Vector2 input_get_mouse_screen_pos(void) {
    return g_input.mouse_screen_pos;
}

bool input_is_key_pressed(int key) {
    return IsKeyDown(key);
}

bool input_is_key_just_pressed(int key) {
    return IsKeyPressed(key);
}

bool input_is_mouse_button_pressed(int button) {
    switch (button) {
        case MOUSE_BUTTON_LEFT: return g_input.mouse_left_down;
        case MOUSE_BUTTON_RIGHT: return g_input.mouse_right_down;
        case MOUSE_BUTTON_MIDDLE: return g_input.mouse_middle_down;
        default: return false;
    }
}

bool input_is_mouse_button_just_pressed(int button) {
    return IsMouseButtonPressed(button);
}

void input_handle_mouse_click(int button, Vector2 screen_pos) {
    printf("[INPUT] Mouse click: button=%d, pos=(%.1f, %.1f)\n", 
           button, screen_pos.x, screen_pos.y);
    
    Vector2 world_pos = GetScreenToWorld2D(screen_pos, g_game_state.camera);
    
    // Check if click is over UI
    if (input_is_over_ui(screen_pos)) {
        printf("[INPUT] Click over UI, ignoring world interaction\n");
        return;
    }
    
    if (button == MOUSE_BUTTON_LEFT) {
        // Left click - move or interact
        char entity_id[MAX_ID_LENGTH];
        if (input_find_entity_at_position(screen_pos, entity_id, sizeof(entity_id))) {
            // Clicked on entity - interact
            InputEvent event = {0};
            event.type = INPUT_EVENT_INTERACT;
            event.screen_position = screen_pos;
            event.world_position = world_pos;
            strncpy(event.target_id, entity_id, MAX_ID_LENGTH - 1);
            event.shift_held = g_input.key_shift_down;
            event.ctrl_held = g_input.key_ctrl_down;
            event.alt_held = g_input.key_alt_down;
            event.timestamp = GetTime();
            
            input_add_event(&event);
        } else {
            // Clicked on empty space - move
            InputEvent event = {0};
            event.type = INPUT_EVENT_MOVE_TO;
            event.screen_position = screen_pos;
            event.world_position = world_pos;
            event.shift_held = g_input.key_shift_down;
            event.ctrl_held = g_input.key_ctrl_down;
            event.alt_held = g_input.key_alt_down;
            event.timestamp = GetTime();
            
            input_add_event(&event);
        }
        
        g_input.last_click_pos = screen_pos;
        g_input.last_click_time = GetTime();
    }
    else if (button == MOUSE_BUTTON_MIDDLE) {
        // Middle click - start camera dragging
        g_input.camera_dragging = true;
        g_input.camera_drag_start = screen_pos;
    }
    else if (button == MOUSE_BUTTON_RIGHT) {
        // Right click - context menu or special action
        printf("[INPUT] Right click - context menu not implemented\n");
    }
}

void input_handle_mouse_wheel(float wheel_move) {
    if (g_input.invert_zoom) {
        wheel_move = -wheel_move;
    }
    
    if (wheel_move > 0) {
        InputEvent event = {0};
        event.type = INPUT_EVENT_ZOOM_IN;
        event.timestamp = GetTime();
        input_add_event(&event);
    } else if (wheel_move < 0) {
        InputEvent event = {0};
        event.type = INPUT_EVENT_ZOOM_OUT;
        event.timestamp = GetTime();
        input_add_event(&event);
    }
}

void input_handle_key_press(int key) {
    printf("[INPUT] Key pressed: %d\n", key);
    
    InputEvent event = {0};
    event.timestamp = GetTime();
    event.shift_held = g_input.key_shift_down;
    event.ctrl_held = g_input.key_ctrl_down;
    event.alt_held = g_input.key_alt_down;
    
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

void input_handle_key_release(int key) {
    // TODO: Handle key release events if needed
}

void input_handle_window_resize(int width, int height) {
    printf("[INPUT] Window resized to %dx%d\n", width, height);
    
    // Update camera offset to keep it centered
    if (g_game_state.camera_initialized) {
        game_state_lock();
        g_game_state.camera.offset.x = width / 2.0f;
        g_game_state.camera.offset.y = height / 2.0f;
        game_state_unlock();
    }
}

int input_send_player_move(Vector2 target_pos) {
    printf("[INPUT] Sending player move to (%.2f, %.2f)\n", target_pos.x, target_pos.y);
    
    char json_buffer[256];
    float grid_x = target_pos.x / (g_game_state.cell_size > 0 ? g_game_state.cell_size : 12.0f);
    float grid_y = target_pos.y / (g_game_state.cell_size > 0 ? g_game_state.cell_size : 12.0f);
    
    if (create_player_action_json(grid_x, grid_y, json_buffer, sizeof(json_buffer)) == 0) {
        return client_send(json_buffer);
    }
    
    return -1;
}

int input_send_interaction(const char* target_id) {
    printf("[INPUT] Sending interaction with target: %s\n", target_id ? target_id : "unknown");
    
    // TODO: Implement interaction message format
    return 0;
}

int input_send_skill_activation(const char* skill_id, Vector2 target_pos) {
    printf("[INPUT] Sending skill activation: %s at (%.2f, %.2f)\n", 
           skill_id ? skill_id : "unknown", target_pos.x, target_pos.y);
    
    // TODO: Implement skill activation message format
    return 0;
}

void input_toggle_hud(void) {
    // TODO: Implement HUD toggle when HUD system is ready
    printf("[INPUT] HUD toggle not implemented yet\n");
}

void input_toggle_debug_mode(void) {
    game_state_lock();
    g_game_state.dev_ui = !g_game_state.dev_ui;
    printf("[INPUT] Debug mode %s\n", g_game_state.dev_ui ? "enabled" : "disabled");
    game_state_unlock();
}

void input_set_camera_zoom(float zoom) {
    // Clamp zoom to reasonable range
    if (zoom < 0.1f) zoom = 0.1f;
    if (zoom > 5.0f) zoom = 5.0f;
    
    game_state_lock();
    g_game_state.camera_zoom = zoom;
    if (g_game_state.camera_initialized) {
        g_game_state.camera.zoom = zoom;
    }
    game_state_unlock();
    
    printf("[INPUT] Camera zoom set to %.2f\n", zoom);
}

void input_pan_camera(Vector2 offset) {
    if (!g_game_state.camera_initialized) return;
    
    game_state_lock();
    g_game_state.camera.target.x += offset.x / g_game_state.camera.zoom;
    g_game_state.camera.target.y += offset.y / g_game_state.camera.zoom;
    game_state_unlock();
}

bool input_find_entity_at_position(Vector2 screen_pos, char* entity_id, size_t id_size) {
    Vector2 world_pos = GetScreenToWorld2D(screen_pos, g_game_state.camera);
    float cell_size = g_game_state.cell_size > 0 ? g_game_state.cell_size : 12.0f;
    
    game_state_lock();
    
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
        game_state_unlock();
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
            game_state_unlock();
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
            game_state_unlock();
            return true;
        }
    }
    
    game_state_unlock();
    return false;
}

bool input_find_object_at_position(Vector2 screen_pos, char* object_id, size_t id_size) {
    Vector2 world_pos = GetScreenToWorld2D(screen_pos, g_game_state.camera);
    float cell_size = g_game_state.cell_size > 0 ? g_game_state.cell_size : 12.0f;
    
    game_state_lock();
    
    // Check obstacles
    for (int i = 0; i < g_game_state.obstacle_count; i++) {
        WorldObject* obj = &g_game_state.obstacles[i];
        Rectangle rect = {
            obj->pos.x * cell_size,
            obj->pos.y * cell_size,
            obj->dims.x * cell_size,
            obj->dims.y * cell_size
        };
        
        if (CheckCollisionPointRec(world_pos, rect)) {
            strncpy(object_id, obj->id, id_size - 1);
            object_id[id_size - 1] = '\0';
            game_state_unlock();
            return true;
        }
    }
    
    // Check portals
    for (int i = 0; i < g_game_state.portal_count; i++) {
        WorldObject* obj = &g_game_state.portals[i];
        Rectangle rect = {
            obj->pos.x * cell_size,
            obj->pos.y * cell_size,
            obj->dims.x * cell_size,
            obj->dims.y * cell_size
        };
        
        if (CheckCollisionPointRec(world_pos, rect)) {
            strncpy(object_id, obj->id, id_size - 1);
            object_id[id_size - 1] = '\0';
            game_state_unlock();
            return true;
        }
    }
    
    game_state_unlock();
    return false;
}

bool input_is_over_ui(Vector2 screen_pos) {
    // TODO: Implement proper UI hit testing when UI system is ready
    // For now, just check if in the bottom HUD area
    return screen_pos.y > (g_game_state.camera_initialized ? 
                           GetScreenHeight() - 60 : screen_pos.y);
}

bool input_get_tooltip_at_position(Vector2 screen_pos, char* tooltip_buffer, size_t buffer_size) {
    // TODO: Implement tooltip system
    return false;
}

void input_handle_touch_start(Vector2 touch_pos, int touch_id) {
    // TODO: Implement touch input for mobile
    printf("[INPUT] Touch start not implemented yet\n");
}

void input_handle_touch_move(Vector2 touch_pos, int touch_id) {
    // TODO: Implement touch input for mobile
}

void input_handle_touch_end(Vector2 touch_pos, int touch_id) {
    // TODO: Implement touch input for mobile
}

void input_set_double_click_time(float time_ms) {
    g_input.double_click_time = time_ms;
}

void input_set_drag_threshold(float threshold) {
    g_input.drag_threshold = threshold;
}

void input_set_zoom_invert(bool invert) {
    g_input.invert_zoom = invert;
}

void input_set_zoom_speed(float speed) {
    g_input.zoom_speed = speed;
}