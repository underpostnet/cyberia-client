#ifndef CYBERIA_UI_FX_REWARD_H
#define CYBERIA_UI_FX_REWARD_H

#include <raylib.h>

/* Reward Celebration FX — a reusable, subtle celebration that any reward or
 * notification modal can enable to make obtaining rewards feel memorable.
 *
 * It surrounds a modal with large `star` icons and smaller sparkle particles
 * anchored on three concentric elliptical rings (inner, mid, outer — the mid
 * ring scaled to reach from near the screen edges in to the card on any
 * viewport size), each bobbing, pulsing, and spinning in place with random
 * dispersion so the halo reads as scattered stardust rather than crisp
 * circles. The effect loops while the modal is shown and smoothly fades out
 * when it stops being shown — so modals carry no animation logic of their
 * own.
 *
 * Screen-space and presentation-only. Contract per frame:
 *   1. the host loop calls fx_reward_update(dt) once;
 *   2. a visible reward modal calls fx_reward_show(bounds) to keep it alive and
 *      positioned around its screen rect;
 *   3. the modal calls fx_reward_draw() where it wants the FX layered — usually
 *      just behind its panel so the celebration frames the card without
 *      covering its text.
 * Skipping fx_reward_show for a frame begins the fade-out automatically. */

void fx_reward_init(void);
void fx_reward_reset(void);

/* Keep the celebration alive this frame, framed around modal_bounds (screen
 * pixels). Call every frame the modal is visible. */
void fx_reward_show(Rectangle modal_bounds);

/* Trigger the item-arrival flourish once, when a new reward appears: a slow
 * wave of particles streams in from the screen edges toward the rings and
 * bounces as it lands (elastic settle), lingers there bobbing like the
 * ambient anchors, then fades away. */
void fx_reward_trigger(Rectangle modal_bounds);

/* Advance stars, sparkles, and the show/hide fade. Call once per frame. */
void fx_reward_update(float dt);

/* Draw the celebration in screen space at the current intensity. */
void fx_reward_draw(void);

#endif /* CYBERIA_UI_FX_REWARD_H */
