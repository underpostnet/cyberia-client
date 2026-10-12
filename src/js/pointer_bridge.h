#ifndef CYBERIA_JS_POINTER_BRIDGE_H
#define CYBERIA_JS_POINTER_BRIDGE_H

/* Count of pointer presses on the canvas, mouse and touch alike (Web only, via
 * inline EM_JS). A frame that sees the count rise saw a press, also when a
 * release and the next press fell inside that one frame: raylib reads that as
 * one held contact. */
int pointer_bridge_press_count(void);

#endif /* CYBERIA_JS_POINTER_BRIDGE_H */
