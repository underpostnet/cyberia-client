#include "fx/fx_combat_aim.h"

#include <assert.h>
#include <math.h>
#include <raylib.h>
#define RAYMATH_IMPLEMENTATION
#include <raymath.h>
#include <stdio.h>

/* Bounds of every shape drawn since reset(). */
static float s_min_x, s_min_y, s_max_x, s_max_y;
static int   s_shapes;

static void reset(void) {
    s_min_x = s_min_y = INFINITY;
    s_max_x = s_max_y = -INFINITY;
    s_shapes = 0;
}

static void cover(float x0, float y0, float x1, float y1) {
    s_shapes++;
    s_min_x = fminf(s_min_x, fminf(x0, x1));
    s_min_y = fminf(s_min_y, fminf(y0, y1));
    s_max_x = fmaxf(s_max_x, fmaxf(x0, x1));
    s_max_y = fmaxf(s_max_y, fmaxf(y0, y1));
}

void DrawRectangle(int x, int y, int w, int h, Color c) { cover((float)x, (float)y, (float)(x + w), (float)(y + h)); }
void DrawRectangleRec(Rectangle r, Color c) { cover(r.x, r.y, r.x + r.width, r.y + r.height); }
void DrawLineEx(Vector2 a, Vector2 b, float thick, Color c) { cover(a.x, a.y, b.x, b.y); }
void DrawRectanglePro(Rectangle r, Vector2 origin, float rotation, Color c) {
    Vector2 dir = { cosf(rotation * DEG2RAD), sinf(rotation * DEG2RAD) };
    Vector2 end = Vector2Add((Vector2){ r.x, r.y }, Vector2Scale(dir, r.width));
    cover(r.x, r.y, end.x, end.y);
}

int main(void) {
    /* A reticle frames its target: its brackets sit at the half size, wider while a lock closes in. */
    reset();
    fx_combat_reticle((Vector2){ 100.0f, 100.0f }, 20.0f, FX_RETICLE_CANDIDATE, 0.0f, 0.0f);
    assert(0 < s_shapes && 80.0f - 3.0f <= s_min_x && s_max_x <= 120.0f + 3.0f);
    reset();
    fx_combat_reticle((Vector2){ 100.0f, 100.0f }, 20.0f, FX_RETICLE_LOCKED, 0.0f, 0.0f);
    float opening = s_max_x - s_min_x;
    reset();
    fx_combat_reticle((Vector2){ 100.0f, 100.0f }, 20.0f, FX_RETICLE_LOCKED, 1.0f, 0.0f);
    assert(opening > s_max_x - s_min_x && 120.0f + 4.0f >= s_max_x);

    /* A guide runs from its start to its length along its direction, no further. */
    reset();
    fx_combat_guide((Vector2){ 200.0f, 200.0f }, (Vector2){ 1.0f, 0.0f }, 20.0f, 150.0f, 16.0f, FX_AIM_LOCK, 0.4f);
    assert(0 < s_shapes && 200.0f + 20.0f - 1.0f <= s_min_x && s_max_x <= 200.0f + 150.0f + 1.0f);
    assert(200.0f - 8.0f - 1.0f <= s_min_y && s_max_y <= 200.0f + 8.0f + 1.0f);
    reset();
    fx_combat_guide((Vector2){ 200.0f, 200.0f }, (Vector2){ 1.0f, 0.0f }, 20.0f, 10.0f, 16.0f, FX_AIM_LOCK, 0.4f);
    assert(0 == s_shapes);

    /* The anchor stays a flat ring under the feet. */
    reset();
    fx_player_anchor((Vector2){ 64.0f, 96.0f }, 20.0f, 2.0f, 0.0f);
    assert(0 < s_shapes && 64.0f - 25.0f <= s_min_x && s_max_x <= 64.0f + 25.0f);
    assert(96.0f - 13.0f <= s_min_y && s_max_y <= 96.0f + 14.0f);

    puts("fx_combat_aim_test OK");
    return 0;
}
