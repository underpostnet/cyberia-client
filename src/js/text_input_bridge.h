#ifndef CYBERIA_JS_TEXT_INPUT_BRIDGE_H
#define CYBERIA_JS_TEXT_INPUT_BRIDGE_H

#include <stdbool.h>
#include <stddef.h>

/* One DOM <input> over the canvas. A canvas cannot open the mobile keyboard,
 * compose IME text or paste, so text entry stays in the browser. C polls the
 * input; JS never calls into wasm. */

/* Create the input and its keyboard guard. Call before InitWindow: the guard
 * must run before the GLFW window listeners, which also use the capture phase. */
void text_input_bridge_init(void);

/* Show the input on a screen rect. CSS pixels equal raylib screen pixels. */
void text_input_bridge_show(int x, int y, int width, int height, int max_len,
                            const char* placeholder);

/* Hide and blur the input. The typed text stays. */
void text_input_bridge_hide(void);

/* True once for each Enter press in the input. */
bool text_input_bridge_submitted(void);

/* Copy the input text into `out`, NUL-terminated, and clear the input.
 * Returns the byte length. */
size_t text_input_bridge_take(char* out, size_t cap);

#endif /* CYBERIA_JS_TEXT_INPUT_BRIDGE_H */
