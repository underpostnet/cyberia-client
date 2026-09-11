#include "stat_panel.h"
#include "text.h"
#include "ui_icon.h"

#include <raylib.h>
#include <stdio.h>

static const Color C_LABEL    = { 150, 160, 190, 220 };
static const Color C_POSITIVE = { 120, 220, 140, 255 };
static const Color C_NEGATIVE = { 200,  80,  80, 220 };

static Color value_color(float value) {
    return 0.0f < value ? C_POSITIVE : 0.0f > value ? C_NEGATIVE : C_LABEL;
}

float stat_panel_sum_draw(float x, float y, float width, float pad,
                          const float values[CYBERIA_STAT_COUNT], const char* label) {
    Rectangle header = { x, y, width, STAT_PANEL_SUM_H };
    DrawRectangleRec(header, (Color){ 50, 70, 120, 50 });
    DrawRectangleLinesEx(header, 1.0f, (Color){ 80, 110, 170, 80 });

    float sum = 0.0f;
    for (int i = 0; CYBERIA_STAT_COUNT > i; i++) sum += values[i];

    int icon_sz = 28;
    ui_icon_draw("stats", x + pad + icon_sz * 0.5f, y + STAT_PANEL_SUM_H * 0.5f, icon_sz, false, 0.0f);

    /* Centred on the rendered line height: text.h scales every size by the
     * client-hints font factor. */
    int sum_font = 22;
    char sum_label[16];
    snprintf(sum_label, sizeof(sum_label), "%+.0f", sum);
    float sum_tx = x + pad + icon_sz + 8.0f;
    float sum_ty = y + (STAT_PANEL_SUM_H - (float)text_line_height(sum_font)) * 0.5f;
    DrawText(sum_label, (int)sum_tx, (int)sum_ty, sum_font, value_color(sum));

    int label_font = 12;
    float label_tx = sum_tx + MeasureText(sum_label, sum_font) + 8.0f;
    float label_ty = y + (STAT_PANEL_SUM_H - (float)text_line_height(label_font)) * 0.5f;
    DrawText(label, (int)label_tx, (int)label_ty, label_font, C_LABEL);

    return STAT_PANEL_SUM_H;
}

float stat_panel_grid_draw(float x, float y, float width, float pad, int font,
                           const float values[CYBERIA_STAT_COUNT]) {
    float col_w = width * 0.5f;
    float row_h = (float)font + 8.0f;
    int icon_sz = font * 2;
    for (int i = 0; CYBERIA_STAT_COUNT > i; i++) {
        float cx = x + (i % 2) * col_w;
        float cy = y + (i / 2) * row_h;
        ui_icon_draw(CYBERIA_STAT_ICONS[i], cx + icon_sz * 0.5f, cy + icon_sz * 0.5f, icon_sz, false, 0.0f);
        DrawText(CYBERIA_STAT_LABELS[i], (int)(cx + icon_sz + 4.0f), (int)cy, font, C_LABEL);
        char value[16];
        snprintf(value, sizeof(value), "%+.0f", values[i]);
        DrawText(value, (int)(cx + col_w - MeasureText(value, font) - pad), (int)cy, font, value_color(values[i]));
    }
    return 3.0f * row_h;
}

float stat_panel_draw(float x, float y, float width, float pad, int font,
                      const float values[CYBERIA_STAT_COUNT], const char* label) {
    float sum_h = stat_panel_sum_draw(x, y, width, pad, values, label) + pad;
    return sum_h + stat_panel_grid_draw(x, y + sum_h, width, pad, font, values);
}
