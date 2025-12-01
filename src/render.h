#ifndef RENDER_H
#define RENDER_H

/**
 * @file render.h
 * @brief Rendering subsystem interface for the client
 *
 * This module handles all rendering operations using raylib.
 * The renderer displays a red circle in the center of the canvas
 * with a blue border around the edges for responsive testing.
 */

/**
 * @brief Initialize the rendering subsystem
 * @param title Window title
 * @param width Initial window width
 * @param height Initial window height
 * @return 0 on success, -1 on failure
 */
int render_init(const char* title, int width, int height);

/**
 * @brief Main rendering loop iteration (called each frame)
 *
 * This function should be called every frame to update and render
 * the scene. It handles canvas resizing and responsive rendering.
 */
void render_update(void);

/**
 * @brief Cleanup rendering subsystem
 *
 * Releases all rendering resources and closes the window.
 */
void render_cleanup(void);

/**
 * @brief Check if window should close
 * @return 1 if window should close, 0 otherwise
 */
int render_should_close(void);

#endif // RENDER_H
