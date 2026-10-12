#include "hud_joystick.h"
#include "input/pointer_gesture.h"

#include <assert.h>
#include <math.h>
#include <raylib.h>
#include <raymath.h>

/* Pixel-art joystick in cool grays: square pixels, a thick dark outline, a lit
 * wedge and rim toward the heading, and a shaded thumb. */
#define JOY_PX           4      /* side of one art pixel, in screen pixels */
#define JOY_THUMB_RATIO  0.4f   /* thumb radius per base radius */
#define JOY_WELL_RATIO   0.3f   /* dark center well per base radius */
#define JOY_SECTOR_COS   0.92387953f /* cos 22.5°: half of one heading sector */

static const Color JOY_OUTLINE    = {  16,  18,  24, 220 };
static const Color JOY_RIM        = { 118, 124, 138, 150 };
static const Color JOY_RIM_LIT    = { 206, 212, 224, 235 };
static const Color JOY_WEDGE      = { 170, 178, 194,  60 };
static const Color JOY_FILL       = {  42,  46,  56, 120 };
static const Color JOY_WELL       = {  22,  25,  31, 110 };
static const Color JOY_SHADOW     = {   6,   8,  12, 110 };
static const Color JOY_THUMB_DARK = {  92,  98, 112, 245 };
static const Color JOY_THUMB_BODY = { 142, 148, 162, 245 };
static const Color JOY_SHINE      = { 214, 218, 228, 245 };

/* Art pixels on one side of a disc row: those with their center inside `r`. */
static int row_cells(float r, float y) {
    if (r <= fabsf(y)) return 0;
    return (int)(sqrtf(r * r - y * y) / JOY_PX + 0.5f);
}

/* Art pixels between two radii, on a grid local to `c`. An inner radius of 0
 * gives a disc. */
static void pixel_ring(Vector2 c, float r_out, float r_in, Color color) {
    int rows = (int)ceilf(r_out / JOY_PX);
    for (int row = -rows; row < rows; row++) {
        float y   = ((float)row + 0.5f) * JOY_PX;
        int   out = row_cells(r_out, y);
        int   in  = row_cells(r_in, y);
        if (out <= in) continue;
        int top = (int)c.y + row * JOY_PX;
        int w   = (out - in) * JOY_PX;
        DrawRectangle((int)c.x - out * JOY_PX, top, w, JOY_PX, color);
        DrawRectangle((int)c.x + in * JOY_PX, top, w, JOY_PX, color);
    }
}

/* The art pixels of a ring inside the 45° sector of a heading. */
static void pixel_sector(Vector2 c, float r_out, float r_in, int dir_x, int dir_y, Color color) {
    Vector2 heading = Vector2Normalize((Vector2){ (float)dir_x, (float)dir_y });
    int cells = (int)ceilf(r_out / JOY_PX);
    for (int row = -cells; row < cells; row++) {
        for (int col = -cells; col < cells; col++) {
            Vector2 p = { ((float)col + 0.5f) * JOY_PX, ((float)row + 0.5f) * JOY_PX };
            float   d = Vector2Length(p);
            if (r_out <= d || d < r_in) continue;
            if (Vector2DotProduct(p, heading) < d * JOY_SECTOR_COS) continue;
            DrawRectangle((int)c.x + col * JOY_PX, (int)c.y + row * JOY_PX, JOY_PX, JOY_PX, color);
        }
    }
}

void hud_joystick_draw(const pointer_gesture_t* g) {
    assert(g);
    if (POINTER_JOYSTICK != g->state) return;

    float   px    = (float)JOY_PX;
    float   r     = POINTER_JOYSTICK_RADIUS;
    float   tr    = r * JOY_THUMB_RATIO;
    Vector2 base  = { roundf(g->origin.x), roundf(g->origin.y) };
    Vector2 thumb = { roundf(g->thumb.x), roundf(g->thumb.y) };

    pixel_ring(base, r, r - px, JOY_OUTLINE);
    pixel_ring(base, r - px, r - 2.0f * px, JOY_RIM);
    pixel_ring(base, r - 2.0f * px, 0.0f, JOY_FILL);
    pixel_ring(base, r * JOY_WELL_RATIO, 0.0f, JOY_WELL);
    if (0 != g->dir_x || 0 != g->dir_y) {
        pixel_sector(base, r - 2.0f * px, r * JOY_WELL_RATIO, g->dir_x, g->dir_y, JOY_WEDGE);
        pixel_sector(base, r - px, r - 2.0f * px, g->dir_x, g->dir_y, JOY_RIM_LIT);
    }

    /* The thumb: a drop shadow, the outline, a dark lower crescent, a lit
     * body and a three-pixel glint up and left. */
    pixel_ring((Vector2){ thumb.x, thumb.y + px }, tr, 0.0f, JOY_SHADOW);
    pixel_ring(thumb, tr, tr - px, JOY_OUTLINE);
    pixel_ring(thumb, tr - px, 0.0f, JOY_THUMB_DARK);
    pixel_ring((Vector2){ thumb.x, thumb.y - px }, tr - 2.0f * px, 0.0f, JOY_THUMB_BODY);
    int glint = ((int)(tr / px) / 2 + 1) * JOY_PX;
    DrawRectangle((int)thumb.x - glint, (int)thumb.y - glint, 2 * JOY_PX, JOY_PX, JOY_SHINE);
    DrawRectangle((int)thumb.x - glint, (int)thumb.y - glint + JOY_PX, JOY_PX, JOY_PX, JOY_SHINE);
}
