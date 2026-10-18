#include "ui/graph_layout.h"

#include <assert.h>
#include <math.h>
#include <raylib.h>
#include <stdio.h>

static bool near(float a, float b) { return 0.001f > fabsf(a - b); }

int main(void) {
    Vector2 cells[] = { { -12, 4 }, { 8, 4 }, { 8, 30 }, { -12, 30 } };
    Vector2 sizes[] = { { 0.8f, 0.4f }, { 0.4f, 0.8f }, { 0.6f, 0.3f }, { 0.7f, 0.2f } };
    Vector2 centers[4];
    Vector2 span = graph_layout_pack(cells, sizes, 4, 0.025f, centers);
    assert(near(span.x, 1.425f) && near(span.y, 1.125f));
    assert(centers[0].x < centers[1].x && near(centers[0].y, centers[1].y));
    assert(centers[0].y < centers[3].y && near(centers[0].x, centers[3].x));
    for (int i = 0; 4 > i; i++) {
        for (int j = i + 1; 4 > j; j++) {
            float dx = fabsf(centers[i].x - centers[j].x);
            float dy = fabsf(centers[i].y - centers[j].y);
            assert(dx >= (sizes[i].x + sizes[j].x) * 0.5f || dy >= (sizes[i].y + sizes[j].y) * 0.5f);
        }
    }
    Vector2 reversed_cells[4], reversed_sizes[4], reversed[4];
    for (int i = 0; 4 > i; i++) {
        reversed_cells[i] = cells[3 - i];
        reversed_sizes[i] = sizes[3 - i];
    }
    graph_layout_pack(reversed_cells, reversed_sizes, 4, 0.025f, reversed);
    for (int i = 0; 4 > i; i++) {
        assert(near(centers[i].x, reversed[3 - i].x) && near(centers[i].y, reversed[3 - i].y));
        assert(near(cells[i].x, reversed_cells[3 - i].x));
    }
    assert(near(graph_layout_pack(cells, sizes, 0, 0.025f, centers).x, 0));
    puts("graph_layout_test OK");
    return 0;
}
