#ifndef CYBERIA_UI_CHAT_PANE_H
#define CYBERIA_UI_CHAT_PANE_H

#include <raylib.h>
#include <stdbool.h>

#include "chat_log.h"
#include "ui_scroll.h"

/* One conversation inside a host's content rect: the history from chat_log, a
 * DOM text input (js/text_input_bridge.h), an emoji menu and a Send button.
 * The host calls update, draw and press with the same content rect.
 * Every pane shares the one DOM input; the pane that shows it owns it. */

typedef struct {
    ChatConversation conversation;
    UIScroll         scroll;
    float            content_h;
    int              seen_count;
    double           seen_ts;
    bool             stick;       /* scroll to the newest line on the next draw */
    bool             emoji_open;  /* the emoji menu shows over the history */
} ChatPane;

/* Point the pane at a conversation. A new one starts at its newest line. */
void chat_pane_bind(ChatPane* pane, ChatConversation conversation);

/* Advance scroll, mark the lines read, place or hide the text input, and send
 * an Enter-submitted line. `input_visible` false hides the input, for example
 * while the content animates or another surface covers it. */
void chat_pane_update(ChatPane* pane, Rectangle content, bool input_visible, float dt);

void chat_pane_draw(ChatPane* pane, Rectangle content);

/* Route a press inside the content rect: the emoji menu, Send, or a history
 * scroll. A press outside the open emoji menu closes it. */
void chat_pane_press(ChatPane* pane, Rectangle content, int mx, int my);

bool chat_pane_wheel(ChatPane* pane, Rectangle content, float wheel_delta);

/* Hide the text input when this pane owns it. Call when the pane stops
 * showing. */
void chat_pane_hide(ChatPane* pane);

#endif /* CYBERIA_UI_CHAT_PANE_H */
