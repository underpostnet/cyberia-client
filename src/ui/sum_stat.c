#include "sum_stat.h"
#include "ui_icon.h"

#include <stdio.h>

static const Color C_LABEL = { 150, 160, 190, 220 };
static const Color C_STAT  = { 120, 220, 140, 255 };

Stats sum_stat_compute(const ObjectLayerState* layers, int count, ObjectLayersManager* mgr) {
    Stats t = { 0 };
    if (!mgr) return t;
    for (int i = 0; i < count; i++) {
        ObjectLayer* ol = lookup_cached_layer(layers[i].item_id);
        if (!ol) continue;
        t.effect       += ol->data.stats.effect;
        t.resistance   += ol->data.stats.resistance;
        t.agility      += ol->data.stats.agility;
        t.range        += ol->data.stats.range;
        t.intelligence += ol->data.stats.intelligence;
        t.utility      += ol->data.stats.utility;
    }
    return t;
}

float sum_stat_draw(float x, float y, float width, float pad, int sum) {
    Rectangle header = { x, y, width, SUM_STAT_HEADER_H };
    DrawRectangleRec(header, (Color){ 50, 70, 120, 50 });
    DrawRectangleLinesEx(header, 1.0f, (Color){ 80, 110, 170, 80 });

    int icon_sz = 28;
    float icon_cx = x + pad + icon_sz * 0.5f;
    float icon_cy = y + SUM_STAT_HEADER_H * 0.5f;
    ui_icon_draw("stats", icon_cx, icon_cy, icon_sz, false, 0.0f);

    int sum_font = 22;
    char sum_label[16];
    snprintf(sum_label, sizeof(sum_label), "%+d", sum);
    int sum_tw = MeasureText(sum_label, sum_font);
    float sum_tx = x + pad + icon_sz + 8.0f;
    float sum_ty = y + (SUM_STAT_HEADER_H - sum_font) * 0.5f;
    DrawText(sum_label, (int)sum_tx, (int)sum_ty, sum_font, C_STAT);

    int total_font = 12;
    float total_tx = sum_tx + sum_tw + 8.0f;
    float total_ty = y + (SUM_STAT_HEADER_H - total_font) * 0.5f;
    DrawText("Total Stats", (int)total_tx, (int)total_ty, total_font, C_LABEL);

    return SUM_STAT_HEADER_H;
}
