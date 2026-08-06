#ifndef CYBERIA_UI_FX_ASSEMBLE_H
#define CYBERIA_UI_FX_ASSEMBLE_H

#include <raylib.h>

/* Assembly Synthesis FX — the electric counterpart to fx_reward.
 *
 * Where the reward celebration anchors a warm gold halo around a modal,
 * this one is directional: cyan fragments stream inward from a wide,
 * viewport-scaled ring and dissolve at the card's edge, so the effect reads
 * as raw components being drawn into the assembler rather than a static
 * decoration. Long electric rays sweep in alongside them, filling the space
 * around the card while the synthesis charges.
 *
 * Screen-space and presentation-only. Contract per frame:
 *   1. the host loop calls fx_assemble_update(dt) once;
 *   2. a charging assembly calls fx_assemble_show(bounds) to keep it alive and
 *      positioned around its card;
 *   3. the card calls fx_assemble_draw() last, so the field reads over the
 *      panel rather than behind it. Streams dissolve just outside the card
 *      edge, so they frame it without covering its progress bar or slots.
 * Skipping fx_assemble_show for a frame begins the fade-out automatically. */

void fx_assemble_init(void);
void fx_assemble_reset(void);

/* Keep the synthesis field alive this frame, converging on `card_bounds`. */
void fx_assemble_show(Rectangle card_bounds);

void fx_assemble_update(float dt);
void fx_assemble_draw(void);

#endif /* CYBERIA_UI_FX_ASSEMBLE_H */
