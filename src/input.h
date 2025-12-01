#ifndef INPUT_H
#define INPUT_H

#include "game_state.h"
#include <raylib.h>
#include <stdbool.h>
#include <stddef.h>

/**
 * @file input.h
 * @brief Input handling system for player controls
 * 
 * This module handles all player input including mouse clicks, keyboard input,
 * and touch input for mobile platforms. It translates input events into
 * game actions and network messages.
 * Migrated from Python EntityPlayerInput and related input handling.
 */

/**
 * @brief Input event types
 */
typedef enum {
    INPUT_EVENT_NONE = 0,
    INPUT_EVENT_MOVE_TO,        // Player clicked to move to a position
    INPUT_EVENT_INTERACT,       // Player clicked on an interactive object
    INPUT_EVENT_USE_SKILL,      // Player activated a skill/ability
    INPUT_EVENT_TOGGLE_HUD,     // Player toggled HUD visibility
    INPUT_EVENT_TOGGLE_DEBUG,   // Player toggled debug mode
    INPUT_EVENT_ZOOM_IN,        // Player zoomed camera in
    INPUT_EVENT_ZOOM_OUT,       // Player zoomed camera out
    INPUT_EVENT_CANCEL_ACTION   // Player cancelled current action
} InputEventType;

/**
 * @brief Input event structure
 */
typedef struct {
    InputEventType type;
    Vector2 world_position;     // World coordinates for position events
    Vector2 screen_position;    // Screen coordinates
    char target_id[MAX_ID_LENGTH];  // ID of target object/entity
    int skill_index;            // Skill/item index for skill events
    bool shift_held;            // Was shift key held during event
    bool ctrl_held;             // Was ctrl key held during event
    bool alt_held;              // Was alt key held during event
    double timestamp;           // When the event occurred
} InputEvent;

/**
 * @brief Input manager state
 */
typedef struct {
    // Current mouse state
    Vector2 mouse_screen_pos;
    Vector2 mouse_world_pos;
    bool mouse_left_down;
    bool mouse_right_down;
    bool mouse_middle_down;
    
    // Keyboard state
    bool key_shift_down;
    bool key_ctrl_down;
    bool key_alt_down;
    
    // Camera control
    Vector2 camera_drag_start;
    bool camera_dragging;
    float zoom_target;
    float zoom_speed;
    
    // Input event queue
    InputEvent event_queue[32];
    int event_count;
    
    // Interaction state
    char hovered_object_id[MAX_ID_LENGTH];
    Vector2 last_click_pos;
    double last_click_time;
    
    // Settings
    float double_click_time;    // Maximum time between clicks for double-click
    float drag_threshold;       // Minimum distance to start dragging
    bool invert_zoom;          // Invert zoom direction
    
} InputManager;

// Global input manager instance
extern InputManager g_input;

/**
 * @brief Initialize input system
 * @return 0 on success, -1 on failure
 */
int input_init(void);

/**
 * @brief Update input system (call each frame)
 * 
 * Processes all input events and updates input state.
 * Should be called before game logic update.
 */
void input_update(void);

/**
 * @brief Cleanup input system
 */
void input_cleanup(void);

/**
 * @brief Process input events and generate game actions
 * 
 * This function processes queued input events and converts them
 * into appropriate game actions (network messages, state changes, etc.)
 */
void input_process_events(void);

/**
 * @brief Add an input event to the queue
 * @param event Event to add
 * @return 0 on success, -1 if queue is full
 */
int input_add_event(const InputEvent* event);

/**
 * @brief Clear all events from the input queue
 */
void input_clear_events(void);

/**
 * @brief Get current mouse position in world coordinates
 * @return World position vector
 */
Vector2 input_get_mouse_world_pos(void);

/**
 * @brief Get current mouse position in screen coordinates
 * @return Screen position vector
 */
Vector2 input_get_mouse_screen_pos(void);

/**
 * @brief Check if a key is currently pressed
 * @param key Raylib KeyboardKey enum value
 * @return true if pressed, false otherwise
 */
bool input_is_key_pressed(int key);

/**
 * @brief Check if a key was just pressed this frame
 * @param key Raylib KeyboardKey enum value
 * @return true if just pressed, false otherwise
 */
bool input_is_key_just_pressed(int key);

/**
 * @brief Check if mouse button is currently pressed
 * @param button Raylib MouseButton enum value
 * @return true if pressed, false otherwise
 */
bool input_is_mouse_button_pressed(int button);

/**
 * @brief Check if mouse button was just pressed this frame
 * @param button Raylib MouseButton enum value
 * @return true if just pressed, false otherwise
 */
bool input_is_mouse_button_just_pressed(int button);

// ============================================================================
// Input Event Handlers
// ============================================================================

/**
 * @brief Handle mouse click event
 * @param button Mouse button that was clicked
 * @param screen_pos Click position in screen coordinates
 */
void input_handle_mouse_click(int button, Vector2 screen_pos);

/**
 * @brief Handle mouse wheel scroll event
 * @param wheel_move Scroll amount (positive = up, negative = down)
 */
void input_handle_mouse_wheel(float wheel_move);

/**
 * @brief Handle keyboard key press event
 * @param key Key that was pressed
 */
void input_handle_key_press(int key);

/**
 * @brief Handle keyboard key release event
 * @param key Key that was released
 */
void input_handle_key_release(int key);

/**
 * @brief Handle window resize event
 * @param width New window width
 * @param height New window height
 */
void input_handle_window_resize(int width, int height);

// ============================================================================
// Game Action Functions
// ============================================================================

/**
 * @brief Send player move command to server
 * @param target_pos Target position in world coordinates
 * @return 0 on success, -1 on failure
 */
int input_send_player_move(Vector2 target_pos);

/**
 * @brief Send interaction command to server
 * @param target_id ID of object/entity to interact with
 * @return 0 on success, -1 on failure
 */
int input_send_interaction(const char* target_id);

/**
 * @brief Send skill activation command to server
 * @param skill_id Skill/item ID to activate
 * @param target_pos Target position (if applicable)
 * @return 0 on success, -1 on failure
 */
int input_send_skill_activation(const char* skill_id, Vector2 target_pos);

/**
 * @brief Toggle HUD visibility
 */
void input_toggle_hud(void);

/**
 * @brief Toggle debug mode
 */
void input_toggle_debug_mode(void);

/**
 * @brief Set camera zoom level
 * @param zoom New zoom level (1.0 = normal, >1.0 = zoomed in, <1.0 = zoomed out)
 */
void input_set_camera_zoom(float zoom);

/**
 * @brief Pan camera by offset
 * @param offset Offset to pan camera by
 */
void input_pan_camera(Vector2 offset);

// ============================================================================
// Hit Testing and Object Selection
// ============================================================================

/**
 * @brief Find entity at screen position
 * @param screen_pos Position in screen coordinates
 * @param entity_id Output buffer for entity ID
 * @param id_size Size of entity_id buffer
 * @return true if entity found, false otherwise
 */
bool input_find_entity_at_position(Vector2 screen_pos, char* entity_id, size_t id_size);

/**
 * @brief Find world object at screen position
 * @param screen_pos Position in screen coordinates
 * @param object_id Output buffer for object ID
 * @param id_size Size of object_id buffer
 * @return true if object found, false otherwise
 */
bool input_find_object_at_position(Vector2 screen_pos, char* object_id, size_t id_size);

/**
 * @brief Check if screen position is over UI element
 * @param screen_pos Position in screen coordinates
 * @return true if over UI, false otherwise
 */
bool input_is_over_ui(Vector2 screen_pos);

/**
 * @brief Get tooltip text for position (if any)
 * @param screen_pos Position in screen coordinates
 * @param tooltip_buffer Output buffer for tooltip text
 * @param buffer_size Size of tooltip buffer
 * @return true if tooltip available, false otherwise
 */
bool input_get_tooltip_at_position(Vector2 screen_pos, char* tooltip_buffer, size_t buffer_size);

// ============================================================================
// Touch Input (Mobile Support)
// ============================================================================

/**
 * @brief Handle touch start event
 * @param touch_pos Touch position in screen coordinates
 * @param touch_id Touch identifier
 */
void input_handle_touch_start(Vector2 touch_pos, int touch_id);

/**
 * @brief Handle touch move event
 * @param touch_pos Touch position in screen coordinates
 * @param touch_id Touch identifier
 */
void input_handle_touch_move(Vector2 touch_pos, int touch_id);

/**
 * @brief Handle touch end event
 * @param touch_pos Touch position in screen coordinates
 * @param touch_id Touch identifier
 */
void input_handle_touch_end(Vector2 touch_pos, int touch_id);

// ============================================================================
// Input Settings
// ============================================================================

/**
 * @brief Set double-click timing
 * @param time_ms Maximum time between clicks for double-click (in milliseconds)
 */
void input_set_double_click_time(float time_ms);

/**
 * @brief Set drag threshold
 * @param threshold Minimum pixel distance to start dragging
 */
void input_set_drag_threshold(float threshold);

/**
 * @brief Set zoom inversion
 * @param invert If true, invert zoom direction
 */
void input_set_zoom_invert(bool invert);

/**
 * @brief Set zoom speed multiplier
 * @param speed Zoom speed multiplier (default 1.0)
 */
void input_set_zoom_speed(float speed);

#endif // INPUT_H