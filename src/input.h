#ifndef INPUT_H
#define INPUT_H

#include <raylib.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "input/input_command.h"

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
    double timestamp;           // When the event occurred
} InputEvent;

/**
 * @brief Input manager state
 */
typedef struct {
    InputEvent event_queue[32];
    uint32_t event_count;
} InputManager;

void input_update(void);
void input_event_queue_handle(void);

/* ── Typed input-command queue ───────────────────────────────────────
 *
 * input_update + input_event_queue_handle convert raw events into typed
 * input commands and push them onto a small ring. Prediction drains the
 * ring inside its own fixed-timestep step.
 *
 *   input_command_pop          retrieve the oldest pending command
 *   input_command_queue_count  how many commands are waiting
 *   input_command_queue_clear  drop everything (used on reconnect)
 */
bool input_command_pop(input_command_t* out);
int  input_command_queue_count(void);
void input_command_queue_clear(void);

#endif // INPUT_H
