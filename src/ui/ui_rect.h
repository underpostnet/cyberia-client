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

/* Pixel-retro bevel: a black frame, the fill inset by 2 px, then 2 px top and
 * bottom edges inset by `edge_inset` from the sides. Roundness 0 is square.
 * Returns the inner rectangle, for the outline or stripe that goes on top. */
static inline Rectangle draw_pixel_bevel(Rectangle r, float roundness, float edge_inset,
                                         Color fill, Color top, Color bottom) {
    Rectangle inner = ui_rect_inset(r, 2.0f);
    DrawRectangleRounded(r, roundness, 4, BLACK);
    DrawRectangleRounded(inner, roundness, 4, fill);
    DrawRectangle((int)(inner.x + edge_inset), (int)inner.y,
                  (int)(inner.width - 2.0f * edge_inset), 2, top);
    DrawRectangle((int)(inner.x + edge_inset), (int)(inner.y + inner.height - 2.0f),
                  (int)(inner.width - 2.0f * edge_inset), 2, bottom);
    return inner;
}

#endif /* CYBERIA_UI_UI_RECT_H */
