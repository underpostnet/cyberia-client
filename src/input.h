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
    INPUT_EVENT_TAP,            // Player tapped a screen coordinate (fundamental game event)
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

    // Input event queue
    InputEvent event_queue[32];
    uint32_t event_count;
} InputManager;


/**
 * Processes all input events and updates input state.
 * Should be called before game logic update.
 */
void input_update(void);

/** Cleanup input system */
void input_cleanup(void);

/**
 * Handle window resize event
 * @param width New window width
 * @param height New window height
 */
void input_handle_window_resize(int width, int height);

/**
 * @brief Process input events and generate game actions
 *
 * This function processes queued input events and converts them
 * into appropriate game actions (network messages, state changes, etc.)
 */
void input_process_events(void);

#endif // INPUT_H