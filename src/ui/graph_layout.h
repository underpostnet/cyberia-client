#ifndef CYBERIA_UI_GRAPH_LAYOUT_H
#define CYBERIA_UI_GRAPH_LAYOUT_H

#include <raylib.h>

/* Places `count` nodes on their layout cells: a column is as wide as its
 * widest node, a row as tall as its tallest, `gap` apart, in the order of the
 * cells. A run of empty columns or rows leaves the room of the smallest node
 * and one more gap. Writes each centre, about the origin, and returns the span. */
Vector2 graph_layout_pack(const Vector2* cells, const Vector2* sizes, int count, float gap, Vector2* centers);

#endif /* CYBERIA_UI_GRAPH_LAYOUT_H */
