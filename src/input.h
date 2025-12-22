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
    double timestamp;           // When the event occurred
} InputEvent;

/**
 * @brief Input manager state
 */
typedef struct {
    // Current mouse state
    Vector2 mouse_screen_pos;
    bool mouse_left_down;

    // Input event queue
    InputEvent event_queue[32];
    int event_count;


} InputManager;

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
 * @brief Toggle debug mode
 */
void input_toggle_debug_mode(void);

/**
 * @brief Set camera zoom level
 * @param zoom New zoom level (1.0 = normal, >1.0 = zoomed in, <1.0 = zoomed out)
 */
void input_set_camera_zoom(float zoom);

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
 * @brief Check if screen position is over UI element
 * @param screen_pos Position in screen coordinates
 * @return true if over UI, false otherwise
 */
bool input_is_over_ui(Vector2 screen_pos);



#endif // INPUT_H