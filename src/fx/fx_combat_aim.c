#include "fx/fx_combat_aim.h"

#include "fx/fx_shapes.h"

#include <math.h>
#include <raylib.h>
#include <raymath.h>

const Color FX_AIM_ACCENT = { 255, 214,  90, 255 };
const Color FX_AIM_LOCK   = { 120, 220, 255, 255 };
const Color FX_AIM_CANCEL = { 220,  60,  60, 255 };

static const Color FX_AIM_EDGE = { 6, 8, 12, 200 };  /* the dark edge that keeps a mark readable on any ground */

/* `c` with its alpha scaled by `k`, so a faded tone fades every part. */
static Color scaled(Color c, float k) {
    c.a = (unsigned char)((float)c.a * k);
    return c;
}

#define GUIDE_PULSES      3
#define GUIDE_PULSE_SPEED 2.2f   /* lengths of the guide per second */
#define RETICLE_LOCK_S    0.22f  /* a locked reticle closes in during this time */

void fx_combat_guide(Vector2 from, Vector2 dir, float start, float length, float width, Color tone, float t) {
    if (length <= start) return;
    float   span   = length - start;
    Vector2 side   = { -dir.y * width * 0.5f, dir.x * width * 0.5f };
    Vector2 base   = Vector2Add(from, Vector2Scale(dir, start));
    Vector2 tip    = Vector2Add(from, Vector2Scale(dir, length));
    Vector2 corner[4] = {
        Vector2Add(base, side), Vector2Add(tip, side), Vector2Subtract(tip, side), Vector2Subtract(base, side),
    };

    float degrees = atan2f(dir.y, dir.x) * RAD2DEG;
    DrawRectanglePro((Rectangle){ base.x, base.y, span, width }, (Vector2){ 0.0f, width * 0.5f }, degrees, scaled(tone, 0.22f));
    for (int i = 0; 4 > i; i++) {
        DrawLineEx(corner[i], corner[(i + 1) % 4], 4.0f, scaled(FX_AIM_EDGE, (float)tone.a / 255.0f));
        DrawLineEx(corner[i], corner[(i + 1) % 4], 2.0f, scaled(tone, 0.85f));
    }
    DrawLineEx(base, tip, 2.0f, scaled(tone, 0.55f));

    for (int k = 0; GUIDE_PULSES > k; k++) {
        float along = fmodf(t * GUIDE_PULSE_SPEED + (float)k / (float)GUIDE_PULSES, 1.0f);
        Vector2 at  = Vector2Add(base, Vector2Scale(dir, span * along));
        fx_shape_spark(at.x, at.y, width * 0.32f, tone, (1.0f - along * 0.7f) * (float)tone.a / 255.0f);
    }

    Vector2 back = Vector2Subtract(tip, Vector2Scale(dir, width));
    Color   edge = scaled(FX_AIM_EDGE, (float)tone.a / 255.0f);
    DrawLineEx(Vector2Add(back, side), tip, 5.0f, edge);
    DrawLineEx(Vector2Subtract(back, side), tip, 5.0f, edge);
    DrawLineEx(Vector2Add(back, side), tip, 3.0f, tone);
    DrawLineEx(Vector2Subtract(back, side), tip, 3.0f, tone);
}

/* One corner bracket: an L with arms `arm` long and `thick` wide, pointing
 * from corner (x, y) along (sx, sy), on a dark edge. */
static void bracket(float x, float y, float sx, float sy, float arm, float thick, Color tone) {
    Rectangle across = { sx > 0 ? x : x - arm, sy > 0 ? y : y - thick, arm, thick };
    Rectangle down   = { sx > 0 ? x : x - thick, sy > 0 ? y : y - arm, thick, arm };
    DrawRectangleRec((Rectangle){ across.x - 1.0f, across.y - 1.0f, across.width + 2.0f, across.height + 2.0f }, FX_AIM_EDGE);
    DrawRectangleRec((Rectangle){ down.x - 1.0f, down.y - 1.0f, down.width + 2.0f, down.height + 2.0f }, FX_AIM_EDGE);
    DrawRectangleRec(across, tone);
    DrawRectangleRec(down, tone);
}

void fx_combat_reticle(Vector2 at, float half, fx_reticle_style_e style, float age, float t) {
    Color tone  = FX_RETICLE_MANUAL == style ? FX_AIM_ACCENT : FX_AIM_LOCK;
    float thick = FX_RETICLE_LOCKED == style ? 3.0f : 2.0f;
    float arm   = half * (FX_RETICLE_MANUAL == style ? 0.35f : 0.55f);
    float size  = half;
    switch (style) {
    case FX_RETICLE_CANDIDATE:
        tone = scaled(tone, 0.6f + 0.2f * sinf(t * 3.0f));
        size = half * (1.0f + 0.06f * sinf(t * 3.0f));
        break;
    case FX_RETICLE_LOCKED:
        size = half * (RETICLE_LOCK_S > age ? 1.6f - 0.6f * age / RETICLE_LOCK_S : 1.0f + 0.04f * sinf(t * 6.0f));
        break;
    case FX_RETICLE_MANUAL:
        size = half * (1.0f + 0.08f * sinf(t * 8.0f));
        break;
    }
    bracket(at.x - size, at.y - size, 1.0f, 1.0f, arm, thick, tone);
    bracket(at.x + size, at.y - size, -1.0f, 1.0f, arm, thick, tone);
    bracket(at.x - size, at.y + size, 1.0f, -1.0f, arm, thick, tone);
    bracket(at.x + size, at.y + size, -1.0f, -1.0f, arm, thick, tone);

    switch (style) {
    case FX_RETICLE_CANDIDATE:
        fx_shape_spark(at.x, at.y, 3.0f, tone, 1.0f);
        break;
    case FX_RETICLE_LOCKED:
        fx_shape_spark(at.x, at.y, 5.0f, tone, 1.0f);
        break;
    case FX_RETICLE_MANUAL:
        DrawRectangleRec((Rectangle){ at.x - 5.0f, at.y - 2.0f, 10.0f, 4.0f }, FX_AIM_EDGE);
        DrawRectangleRec((Rectangle){ at.x - 2.0f, at.y - 5.0f, 4.0f, 10.0f }, FX_AIM_EDGE);
        DrawRectangleRec((Rectangle){ at.x - 4.0f, at.y - 1.0f, 8.0f, 2.0f }, tone);
        DrawRectangleRec((Rectangle){ at.x - 1.0f, at.y - 4.0f, 2.0f, 8.0f }, tone);
        break;
    }
}

void fx_player_anchor(Vector2 feet, float rx, float px, float t) {
    float ry    = rx * 0.45f;
    Color tone  = scaled(FX_AIM_LOCK, 0.55f + 0.15f * sinf(t * 2.5f));
    int   steps = (int)ceilf(2.0f * PI * rx / px);
    for (int i = 0; steps > i; i++) {
        float a = 2.0f * PI * (float)i / (float)steps;
        float x = roundf((feet.x + cosf(a) * rx) / px) * px;
        float y = roundf((feet.y + sinf(a) * ry) / px) * px;
        DrawRectangleRec((Rectangle){ x - px * 0.5f, y - px * 0.5f + px, px, px }, FX_AIM_EDGE);
        DrawRectangleRec((Rectangle){ x - px * 0.5f, y - px * 0.5f, px, px }, tone);
    }
    const Vector2 notch[4] = { { rx + px, 0.0f }, { -rx - px, 0.0f }, { 0.0f, ry + px }, { 0.0f, -ry - px } };
    for (int i = 0; 4 > i; i++) {
        float x = roundf((feet.x + notch[i].x) / px) * px;
        float y = roundf((feet.y + notch[i].y) / px) * px;
        DrawRectangleRec((Rectangle){ x - px, y - px, 2.0f * px, 2.0f * px }, FX_AIM_EDGE);
        DrawRectangleRec((Rectangle){ x - px * 0.5f, y - px * 0.5f, px, px }, FX_AIM_LOCK);
    }
}
