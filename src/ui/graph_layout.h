#ifndef CYBERIA_UI_GRAPH_LAYOUT_H
#define CYBERIA_UI_GRAPH_LAYOUT_H

#include <raylib.h>

/* Places `count` nodes on their layout cells with the empty rows and columns
 * removed: a column is as wide as its widest node, a row as tall as its
 * tallest, `gap` apart, in the order of the cells. Writes each centre, about
 * the origin, and returns the span. */
Vector2 graph_layout_pack(const Vector2* cells, const Vector2* sizes, int count, float gap, Vector2* centers);

#endif /* CYBERIA_UI_GRAPH_LAYOUT_H */
