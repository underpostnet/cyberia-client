// Screen-space tap feedback for the Cyberia C client.
//
// Owns a small fixed pool of short-lived crosses that can be spawned at any
// screen position by existing input or UI systems. No input handling lives here.
//
// Integration pattern:
//   init:    fx_tap_init();
//   update:  fx_tap_update(delta_time);
//   draw:    fx_tap_draw();   // screen-space, outside BeginMode2D

#ifndef FX_TAP_H
#define FX_TAP_H

#include <raylib.h>
#include <stdbool.h>

#define FX_TAP_MAX_ENTRIES 32

typedef struct {
    Color color;      // Base palette color for the cross.
    float scale;      // 1.0 = normal size, >1.0 = larger cross.
    float duration;   // Total lifetime in seconds.
    float intensity;  // 0.0..2.0 visual energy multiplier.
} FxTapParams;

void fx_tap_init(void);
void fx_tap_reset(void);
FxTapParams fx_tap_default_params(void);
void fx_tap_spawn(Vector2 screen_position, const FxTapParams* params);
void fx_tap_update(float dt);
void fx_tap_draw(void);

#endif /* FX_TAP_H */
