#ifndef MODAL_PLAYER_H
#define MODAL_PLAYER_H

#include "modal.h"
#include <stdbool.h>

/**
 * @file modal_player.h
 * @brief Player information modal component
 *
 * This module provides a modal display for player information including
 * connection status, map ID, position, and FPS. It uses the general-purpose
 * modal component and is rendered when dev UI is disabled.
 */

/**
 * @brief Player modal state structure
 */
typedef struct {
    Modal modal;

    // Display options
    bool show_connection;
    bool show_map;
    bool show_position;
    bool show_fps;

    // Cached values for smooth updates
    float cached_fps;
    double last_fps_update;

} ModalPlayer;

/**
 * @brief Initialize the player modal component
 * @return 0 on success, -1 on failure
 */
int modal_player_init(void);

/**
 * @brief Cleanup player modal resources
 */
void modal_player_cleanup(void);

/**
 * @brief Update player modal state
 * @param delta_time Time elapsed since last update
 *
 * Updates the modal content with current player information
 * from the game state.
 */
void modal_player_update(float delta_time);

/**
 * @brief Render the player modal
 * @param screen_width Current screen width
 * @param screen_height Current screen height
 *
 * Renders the player information modal when dev UI is disabled.
 * The modal displays:
 * - Connection status (Connected/Disconnected)
 * - Map ID
 * - Player position (x, y)
 * - FPS (frames per second)
 */
void modal_player_draw(int screen_width, int screen_height);
#endif // MODAL_PLAYER_H
