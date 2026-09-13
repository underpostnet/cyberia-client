#ifndef CYBERIA_UI_CHAT_PANE_H
#define CYBERIA_UI_CHAT_PANE_H

#include <raylib.h>
#include <stdbool.h>

/* One-to-one chat inside a modal's content rect: the history from notify_store,
 * a DOM text input (js/text_input_bridge.h), a Send button and quick-chat
 * presets. The host calls update, draw and press with the same content rect. */

/* Content height the pane asks for in an anchored card. */
#define CHAT_PANE_HEIGHT 280.0f

/* Advance scroll, place or hide the text input, and send an Enter-submitted
 * line. `input_visible` false hides the input, for example while the content
 * animates or another surface covers it. */
void chat_pane_update(Rectangle content, const char* entity_id, bool input_visible, float dt);

/* `entity_name` labels received lines. */
void chat_pane_draw(Rectangle content, const char* entity_id, const char* entity_name);

/* Route a press inside the content rect: Send, a preset, or a history scroll. */
void chat_pane_press(Rectangle content, const char* entity_id, int mx, int my);

bool chat_pane_wheel(Rectangle content, float wheel_delta);

/* Hide the text input. Call when the pane stops showing. */
void chat_pane_hide(void);

#endif /* CYBERIA_UI_CHAT_PANE_H */
