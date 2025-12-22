#ifndef MODAL_PLAYER_H
#define MODAL_PLAYER_H

#include "modal.h"
#include <raylib.h>
#include <stdbool.h>

/**
 * @file modal_player.h
 * @brief Player information modal component
 *
 * This module provides a modal display for player information including
 * connection status, map ID, position, and FPS. It uses the general-purpose
 * modal component and is only rendered when dev_ui is false.
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
 * Renders the player information modal only when dev_ui is false.
 * The modal displays:
 * - Connection status (Connected/Disconnected)
 * - Map ID
 * - Player position (x, y)
 * - FPS (frames per second)
 */
void modal_player_draw(int screen_width, int screen_height);

// /**
//  * @brief Set which information to display in the player modal
//  * @param show_connection Show connection status
//  * @param show_map Show map ID
//  * @param show_position Show player position
//  * @param show_fps Show FPS counter
//  */
// void modal_player_set_display_options(bool show_connection, bool show_map,
//                                       bool show_position, bool show_fps);

// /**
//  * @brief Set player modal position and style
//  * @param position_mode Position mode (e.g., MODAL_POS_TOP_RIGHT)
//  * @param margin_top Top margin
//  * @param margin_right Right margin
//  */
// void modal_player_set_position(int position_mode, int margin_top, int margin_right);

#endif // MODAL_PLAYER_H