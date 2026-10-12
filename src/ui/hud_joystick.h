#ifndef CYBERIA_UI_HUD_JOYSTICK_H
#define CYBERIA_UI_HUD_JOYSTICK_H

#include "input/pointer_gesture.h"

/* Floating joystick, in screen space. Draws only while the gesture steers: a
 * base at the press position and the thumb under the pointer. */
void hud_joystick_draw(const pointer_gesture_t* g);

#endif /* CYBERIA_UI_HUD_JOYSTICK_H */
