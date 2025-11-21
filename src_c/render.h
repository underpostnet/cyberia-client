#ifndef RENDER_H
#define RENDER_H

// Rendering subsystem initialization
int render_init(const char* title, int width, int height);

// Main rendering loop iteration (called each frame)
void render_update(void);

// Update the text to be displayed (from WebSocket messages)
void render_set_text(const char* text);

// Cleanup rendering subsystem
void render_cleanup(void);

// Check if window should close
int render_should_close(void);

#endif // RENDER_H