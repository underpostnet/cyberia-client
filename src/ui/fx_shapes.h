#ifndef CYBERIA_UI_FX_SHAPES_H
#define CYBERIA_UI_FX_SHAPES_H

#include <raylib.h>

/* Shared FX shape primitives — the yellow, black-bordered particle look used by
 * the tap effect, loot drops, and reward celebration. Pure drawing, no state,
 * screen/world agnostic (caller supplies pixel coordinates). */

extern const Color FX_SPARK_GOLD;   /* golden yellow — loot the player may collect */
extern const Color FX_SPARK_GRAY;   /* neutral gray — another player's loot */

/* Pixel-snapped filled rectangle with a solid black border `border` px larger on
 * every side (the tap-effect bar look). */
void fx_shape_bar(float x, float y, float w, float h, float border, Color body);

/* Filled square spark centered at (cx,cy) with a wide black border. `alpha`
 * (0..1) scales the opacity of both body and border. Pixel-snapped. */
void fx_shape_spark(float cx, float cy, float size_px, Color body, float alpha);

#endif /* CYBERIA_UI_FX_SHAPES_H */
