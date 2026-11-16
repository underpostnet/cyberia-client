#include "fx/fx_shapes.h"

#include <math.h>
#include <raylib.h>

const Color FX_SPARK_GOLD = { 255, 226, 20, 255 };
const Color FX_SPARK_GRAY = { 168, 168, 168, 255 };
const Color FX_SPARK_RED  = { 220,  40,  40, 255 };
const Color FX_SPARK_GREEN = {  70, 220, 100, 255 };
const Color FX_SPARK_BLUE  = {  80, 160, 255, 255 };

void fx_shape_bar(float x, float y, float w, float h, float border, Color body) {
    DrawRectangle((int)(x - border), (int)(y - border),
                  (int)(w + border * 2.0f), (int)(h + border * 2.0f), BLACK);
    DrawRectangle((int)x, (int)y, (int)w, (int)h, body);
}

void fx_shape_cooldown_bar(Rectangle r, float charge) {
    static const Color bg    = {  24,  26,  38, 230 };
    static const Color fill  = { 100, 200, 255, 230 };
    static const Color ready = { 120, 230, 150, 240 };
    fx_shape_bar(r.x, r.y, r.width, r.height, 1.0f, bg);
    DrawRectangle((int)r.x, (int)r.y, (int)(r.width * charge), (int)r.height, 1.0f <= charge ? ready : fill);
}

void fx_shape_spark(float cx, float cy, float size_px, Color body, float alpha) {
    if (alpha <= 0.0f) return;
    if (alpha > 1.0f) alpha = 1.0f;
    if (size_px < 2.0f) size_px = 2.0f;

    float border = size_px * 0.28f;
    if (border < 1.0f) border = 1.0f;

    int x = (int)(cx - size_px * 0.5f + 0.5f);
    int y = (int)(cy - size_px * 0.5f + 0.5f);
    int s = (int)(size_px + 0.5f);
    int b = (int)(border + 0.5f);

    unsigned char a = (unsigned char)(255.0f * alpha + 0.5f);
    body.a = a;

    DrawRectangle(x - b, y - b, s + b * 2, s + b * 2, (Color){ 0, 0, 0, a });
    DrawRectangle(x, y, s, s, body);
}

static unsigned char scale_channel(unsigned char value, float factor) {
    float scaled = (float)value * factor;
    return (unsigned char)(scaled > 255.0f ? 255.0f : scaled);
}

void fx_shape_spark_shaded(float cx, float cy, float size_px, Color body) {
    if (size_px < 3.0f) size_px = 3.0f;
    int s = (int)(size_px + 0.5f);
    int b = (int)(size_px * 0.22f + 0.5f);
    if (b < 1) b = 1;
    int band = s / 3;
    if (band < 1) band = 1;
    int x = (int)(cx - size_px * 0.5f + 0.5f);
    int y = (int)(cy - size_px * 0.5f + 0.5f);

    Color light = { scale_channel(body.r, 1.25f), scale_channel(body.g, 1.25f), scale_channel(body.b, 1.25f), 255 };
    Color shade = { scale_channel(body.r, 0.55f), scale_channel(body.g, 0.55f), scale_channel(body.b, 0.55f), 255 };
    body.a = 255;

    DrawRectangle(x - b, y - b, s + b * 2, s + b * 2, BLACK);
    DrawRectangle(x, y, s, s, body);
    DrawRectangle(x, y, s, band, light);
    DrawRectangle(x, y, band, s, light);
    DrawRectangle(x, y + s - band, s, band, shade);
    DrawRectangle(x + s - band, y, band, s, shade);
}

void fx_shape_plus(float cx, float cy, float size_px, Color body) {
    int s = (int)(size_px + 0.5f);
    int arm = s / 3 < 1 ? 1 : s / 3;
    int b = arm / 2 < 1 ? 1 : arm / 2;
    int mid = (s - arm) / 2;
    int x = (int)(cx - size_px * 0.5f + 0.5f);
    int y = (int)(cy - size_px * 0.5f + 0.5f);
    body.a = 255;

    /* Both borders first, so neither arm's border cuts across the other's body. */
    DrawRectangle(x - b, y + mid - b, s + b * 2, arm + b * 2, BLACK);
    DrawRectangle(x + mid - b, y - b, arm + b * 2, s + b * 2, BLACK);
    DrawRectangle(x, y + mid, s, arm, body);
    DrawRectangle(x + mid, y, arm, s, body);
}

/* Art pixels on one side of a disc row: those with their center inside `r`. */
static int row_cells(float r, float y, float px) {
    if (r <= fabsf(y)) return 0;
    return (int)(sqrtf(r * r - y * y) / px + 0.5f);
}

void fx_shape_pixel_ring(Vector2 c, float r_out, float r_in, float px, Color color) {
    int rows = (int)ceilf(r_out / px);
    for (int row = -rows; row < rows; row++) {
        float y   = ((float)row + 0.5f) * px;
        int   out = row_cells(r_out, y, px);
        int   in  = row_cells(r_in, y, px);
        if (out <= in) continue;
        int top = (int)c.y + row * (int)px;
        int w   = (out - in) * (int)px;
        DrawRectangle((int)c.x - out * (int)px, top, w, (int)px, color);
        DrawRectangle((int)c.x + in * (int)px, top, w, (int)px, color);
    }
}

void fx_shape_pixel_arc(Vector2 c, float r_out, float r_in, float px, float from, float span, Color color) {
    const float turn = 2.0f * PI;
    if (span <= 0.0f) return;
    int cells = (int)ceilf(r_out / px);
    for (int row = -cells; row < cells; row++) {
        for (int col = -cells; col < cells; col++) {
            float x = ((float)col + 0.5f) * px;
            float y = ((float)row + 0.5f) * px;
            float d = sqrtf(x * x + y * y);
            if (r_out <= d || d < r_in) continue;
            float along = fmodf(atan2f(y, x) - from, turn);
            if (along < 0.0f) along += turn;
            if (span <= along) continue;
            DrawRectangle((int)c.x + col * (int)px, (int)c.y + row * (int)px, (int)px, (int)px, color);
        }
    }
}
