#ifndef CYBERIA_UI_EASE_H
#define CYBERIA_UI_EASE_H

/* Normalised easing curves. t in [0,1]. */

/* Decelerates to the target, overshoots, then settles. */
static inline float ease_out_back(float t) {
    const float c1 = 1.70158f;
    const float c3 = c1 + 1.0f;
    float u = t - 1.0f;
    return 1.0f + c3 * u * u * u + c1 * u * u;
}

#endif /* CYBERIA_UI_EASE_H */
