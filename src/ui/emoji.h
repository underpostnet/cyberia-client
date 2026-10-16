#ifndef CYBERIA_UI_EMOJI_H
#define CYBERIA_UI_EMOJI_H

#include <raylib.h>
#include <stdbool.h>
#include <stddef.h>

/* emoji — inline chat emoji. A word `:<name>:` whose name is in
 * CYBERIA_CHAT_EMOJI draws as the ui-icon `emoji-<name>`; every other word
 * draws as typed. The server keeps the token as text, so the length limit and
 * the cleaning see what the player typed. */

/* The icon of a token word, or NULL when the word is no emoji. `out` holds
 * "emoji-<name>". */
const char* emoji_icon_of(const char* word, size_t length, char* out, size_t out_size);

/* text_wrap with emoji: word-wrapped to `maxw`, left-aligned at x. Draws when
 * `draw`, else only measures. Returns the height used. */
int emoji_text_wrap(const char* text, int x, int y, int maxw, int size, Color color, bool draw);

/* The width of `text` on one line, emoji included. */
int emoji_text_width(const char* text, int size);

#endif /* CYBERIA_UI_EMOJI_H */
