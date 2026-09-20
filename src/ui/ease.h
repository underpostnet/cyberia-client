#ifndef CYBERIA_UI_EASE_H
#define CYBERIA_UI_EASE_H

#include <math.h>

/* Normalised easing curves. t in [0,1]. */

/* Decelerates to the target, overshoots, then settles. */
static inline float ease_out_back(float t) {
    const float c1 = 1.70158f;
    const float c3 = c1 + 1.0f;
    float u = t - 1.0f;
    return 1.0f + c3 * u * u * u + c1 * u * u;
}

/* Accelerates, then decelerates. Smoothstep. */
static inline float ease_smoothstep(float t) {
    return t * t * (3.0f - 2.0f * t);
}

/* Decelerates to the target. */
static inline float ease_out_cubic(float t) {
    float u = 1.0f - t;
    return 1.0f - u * u * u;
}

/* Accelerates away from the start. */
static inline float ease_in_cubic(float t) {
    return t * t * t;
}

/* Fraction of the remaining distance to cover in `dt` seconds at `rate`.
 * Frame-rate independent: two half steps give the same result as one. */
static inline float ease_exp_factor(float rate, float dt) {
    return 1.0f - expf(-rate * dt);
}

/* Moves `value` toward `target` by `factor` of the distance. */
static inline float ease_approach(float value, float target, float factor) {
    return value + (target - value) * factor;
}

#endif /* CYBERIA_UI_EASE_H */
