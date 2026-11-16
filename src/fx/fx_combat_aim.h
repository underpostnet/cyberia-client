#ifndef CYBERIA_FX_FX_COMBAT_AIM_H
#define CYBERIA_FX_FX_COMBAT_AIM_H

#include <raylib.h>

/* Combat aim FX: the guide a skill flies along, the square reticle on what it
 * aims at, and the ground anchor of the own player. Pure drawing, no state.
 * They share the palette of the combat controls, and each state also has its
 * own shape, so no state rests on colour alone. */

extern const Color FX_AIM_ACCENT;  /* gold: a manual aim, as an aiming control */
extern const Color FX_AIM_LOCK;    /* cyan: an auto aim, as a ready control */
extern const Color FX_AIM_CANCEL;  /* red: an aim that the release cancels */

typedef enum {
    FX_RETICLE_CANDIDATE, /* the target a tap takes: thin brackets, slow pulse, a dot */
    FX_RETICLE_LOCKED,    /* the target the server took: bold brackets that close in, a square */
    FX_RETICLE_MANUAL,    /* the end of a manual aim: short brackets, a cross */
} fx_reticle_style_e;

/* The guide of a skill in screen pixels: a translucent body `width` wide from
 * `start` to `length` along unit `dir` from `from`, its outline and core, a
 * pulse that runs out along it, and a chevron at its end. The alpha of `tone`
 * fades every part. `t` is seconds. */
void fx_combat_guide(Vector2 from, Vector2 dir, float start, float length, float width, Color tone, float t);

/* A square reticle in screen pixels centred on `at`, `half` from its centre to
 * an edge: four corner brackets and a centre mark. `age` is the seconds since
 * it took its target: a locked reticle closes in from wider. */
void fx_combat_reticle(Vector2 at, float half, fx_reticle_style_e style, float age, float t);

/* The ground anchor of the own player in world units: a flat ring `rx` wide
 * centred under the feet at `feet`, drawn in art pixels `px` wide, with a
 * notch on each side. */
void fx_player_anchor(Vector2 feet, float rx, float px, float t);

#endif /* CYBERIA_FX_FX_COMBAT_AIM_H */
