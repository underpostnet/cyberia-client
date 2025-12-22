#ifndef RENDER_H
#define RENDER_H

void render_init(int width, int height);

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

#endif // RENDER_H
