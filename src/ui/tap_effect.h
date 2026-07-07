// Screen-space tap feedback for the Cyberia C client.
//
// Owns a small fixed pool of short-lived crosses that can be spawned at any
// screen position by existing input or UI systems. No input handling lives here.
//
// Integration pattern:
//   init:    tap_effect_init();
//   update:  tap_effect_update(delta_time);
//   draw:    tap_effect_draw();   // screen-space, outside BeginMode2D

#ifndef TAP_EFFECT_H
#define TAP_EFFECT_H

#include <raylib.h>
#include <stdbool.h>

#define TAP_EFFECT_MAX_ENTRIES 32

typedef struct {
    Color color;      // Base palette color for the cross.
    float scale;      // 1.0 = normal size, >1.0 = larger cross.
    float duration;   // Total lifetime in seconds.
    float intensity;  // 0.0..2.0 visual energy multiplier.
} TapEffectParams;

void tap_effect_init(void);
void tap_effect_reset(void);
TapEffectParams tap_effect_default_params(void);
void tap_effect_spawn(Vector2 screen_position, const TapEffectParams* params);
void tap_effect_update(float dt);
void tap_effect_draw(void);

#endif /* TAP_EFFECT_H */
