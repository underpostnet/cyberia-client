#include "graph_layout.h"

#include <float.h>
#include <math.h>
#include <raylib.h>

Vector2 graph_layout_pack(const Vector2* cells, const Vector2* sizes, int count, float gap, Vector2* centers) {
    Vector2 span = {0};
    for (int axis = 0; 2 > axis; axis++) {
        float previous = -FLT_MAX, offset = 0.0f, smallest = FLT_MAX;
        for (int i = 0; count > i; i++) smallest = fminf(smallest, 0 == axis ? sizes[i].x : sizes[i].y);
        for (;;) {
            float cell = FLT_MAX, extent = 0.0f;
            for (int i = 0; count > i; i++) {
                float value = 0 == axis ? cells[i].x : cells[i].y;
                if (value > previous && value < cell) cell = value;
            }
            if (FLT_MAX == cell) break;
            if (-FLT_MAX != previous && 1.0f < cell - previous) offset += smallest + gap;
            for (int i = 0; count > i; i++) {
                if (cell == (0 == axis ? cells[i].x : cells[i].y))
                    extent = fmaxf(extent, 0 == axis ? sizes[i].x : sizes[i].y);
            }
            for (int i = 0; count > i; i++) {
                if (cell != (0 == axis ? cells[i].x : cells[i].y)) continue;
                if (0 == axis) centers[i].x = offset + extent * 0.5f;
                else centers[i].y = offset + extent * 0.5f;
            }
            offset += extent + gap;
            previous = cell;
        }
        float length = 0 < count ? offset - gap : 0.0f;
        if (0 == axis) span.x = length;
        else span.y = length;
        for (int i = 0; count > i; i++) {
            if (0 == axis) centers[i].x -= length * 0.5f;
            else centers[i].y -= length * 0.5f;
        }
    }
    return span;
}
