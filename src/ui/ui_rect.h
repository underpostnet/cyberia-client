#ifndef CYBERIA_UI_UI_RECT_H
#define CYBERIA_UI_UI_RECT_H

#include <raylib.h>

/* Rectangle helpers shared by the UI modules. */

/* Scales `r` around its centre. */
static inline Rectangle ui_rect_scale(Rectangle r, float s) {
    float w = r.width * s;
    float h = r.height * s;
    return (Rectangle){ r.x + (r.width - w) * 0.5f, r.y + (r.height - h) * 0.5f, w, h };
}

/* Shrinks `r` by `px` on every side. */
static inline Rectangle ui_rect_inset(Rectangle r, float px) {
    return (Rectangle){ r.x + px, r.y + px, r.width - 2.0f * px, r.height - 2.0f * px };
}

/* Gives the centre point of `r`. */
static inline Vector2 ui_rect_center(Rectangle r) {
    return (Vector2){ r.x + r.width * 0.5f, r.y + r.height * 0.5f };
}

#endif /* CYBERIA_UI_UI_RECT_H */
