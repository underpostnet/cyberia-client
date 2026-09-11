#include "fx/fx_shapes.h"

#include <raylib.h>

const Color FX_SPARK_GOLD = { 255, 226, 20, 255 };
const Color FX_SPARK_GRAY = { 168, 168, 168, 255 };
const Color FX_SPARK_RED  = { 220,  40,  40, 255 };

void fx_shape_bar(float x, float y, float w, float h, float border, Color body) {
    DrawRectangle((int)(x - border), (int)(y - border),
                  (int)(w + border * 2.0f), (int)(h + border * 2.0f), BLACK);
    DrawRectangle((int)x, (int)y, (int)w, (int)h, body);
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
