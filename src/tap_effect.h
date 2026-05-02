/**
 * @file tap_effect.h
 * @brief Screen-space tap feedback for the Cyberia C client.
 *
 * This module owns a small fixed pool of short-lived visual effects that can
 * be spawned at any screen position by existing input or UI systems.
 *
 * No input handling lives here. Typical usage:
 *   TapEffectParams fx = tap_effect_default_params();
 *   fx.color = (Color){255, 210, 96, 255};
 *   fx.scale = 1.25f;
 *   fx.style_mask = TAP_EFFECT_STYLE_PREMIUM;
 *   tap_effect_spawn((Vector2){mx, my}, &fx);
 *
 * Integration pattern:
 *   init:    tap_effect_init();
 *   update:  tap_effect_update(delta_time);
 *   draw:    tap_effect_draw();   // screen-space, outside BeginMode2D
 */

#ifndef TAP_EFFECT_H
#define TAP_EFFECT_H

#include <stdbool.h>
#include <raylib.h>

#define TAP_EFFECT_MAX_ENTRIES 32

typedef enum {
    TAP_EFFECT_STYLE_PIXEL_BURST = 1 << 0,
    TAP_EFFECT_STYLE_RING_PULSE  = 1 << 1,
    TAP_EFFECT_STYLE_MARKER_FLARE = 1 << 2,

    TAP_EFFECT_STYLE_PREMIUM =
        TAP_EFFECT_STYLE_PIXEL_BURST |
        TAP_EFFECT_STYLE_RING_PULSE |
        TAP_EFFECT_STYLE_MARKER_FLARE,

    TAP_EFFECT_STYLE_SUBTLE = TAP_EFFECT_STYLE_RING_PULSE,

    TAP_EFFECT_STYLE_ARCADE =
        TAP_EFFECT_STYLE_PIXEL_BURST |
        TAP_EFFECT_STYLE_RING_PULSE,
} TapEffectStyle;

typedef struct {
    Color color;           /* Base palette color for the effect. */
    float scale;           /* 1.0 = normal size, >1.0 = larger splash. */
    float duration;        /* Total lifetime in seconds. */
    float intensity;       /* 0.0..2.0 visual energy multiplier. */
    unsigned int style_mask; /* Bitmask of TapEffectStyle flags. */
} TapEffectParams;

void tap_effect_init(void);
void tap_effect_reset(void);
TapEffectParams tap_effect_default_params(void);
void tap_effect_spawn(Vector2 screen_position, const TapEffectParams* params);
void tap_effect_update(float dt);
void tap_effect_draw(void);

#endif /* TAP_EFFECT_H */
