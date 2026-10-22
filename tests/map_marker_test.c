/* The marks of both maps. A mark keeps a screen size between its bounds at any
 * cell size, with no step. Each bot or resource class has its own shape inside
 * its footprint; the entry spawn is a hexagon with the home icon in its own
 * colours. A destination label takes the first free side of its anchor —
 * below, above, right, left — inside the bounds, and none when every side is
 * taken. */

#include "ui/map_marker.h"

#include <assert.h>
#include <math.h>
#include <raylib.h>
#define RAYMATH_IMPLEMENTATION
#include <raymath.h>
#include <stdio.h>
#include <string.h>

int text_measure_compat(const char* text, int size) { return (int)strlen(text) * size / 2; }
int text_line_height(int size) { return size; }
const Color MODAL_QUEST = { 255, 200, 60, 255 };
bool CheckCollisionRecs(Rectangle a, Rectangle b) {
    return a.x < b.x + b.width && b.x < a.x + a.width && a.y < b.y + b.height && b.y < a.y + a.height;
}

/* What a mark drew: its primitives by shape, its extent and the tint of its icons. */
static Rectangle s_extent;
static int s_circles, s_triangles, s_rects, s_hexagons, s_icons, s_tinted, s_texts;
static const char* s_icon;

static void grow(float left, float top, float right, float bottom) {
    if (0.0f == s_extent.width) s_extent = (Rectangle){ left, top, 0.001f, 0.001f };
    float l = fminf(s_extent.x, left), t = fminf(s_extent.y, top);
    float r = fmaxf(s_extent.x + s_extent.width, right), b = fmaxf(s_extent.y + s_extent.height, bottom);
    s_extent = (Rectangle){ l, t, r - l, b - t };
}
static void reset(void) {
    s_extent = (Rectangle){ 0 };
    s_circles = s_triangles = s_rects = s_hexagons = s_icons = s_tinted = s_texts = 0;
}
void DrawCircleV(Vector2 c, float r, Color color) { s_circles++; grow(c.x - r, c.y - r, c.x + r, c.y + r); }
void DrawTriangle(Vector2 a, Vector2 b, Vector2 c, Color color) {
    s_triangles++;
    grow(fminf(a.x, fminf(b.x, c.x)), fminf(a.y, fminf(b.y, c.y)), fmaxf(a.x, fmaxf(b.x, c.x)), fmaxf(a.y, fmaxf(b.y, c.y)));
}
void DrawRectangleRec(Rectangle r, Color color) { s_rects++; grow(r.x, r.y, r.x + r.width, r.y + r.height); }
void DrawPoly(Vector2 c, int sides, float r, float rotation, Color color) {
    s_hexagons += 6 == sides;
    grow(c.x - r, c.y - r, c.x + r, c.y + r);
}
void DrawPolyLinesEx(Vector2 c, int sides, float r, float rotation, float thick, Color color) {
    assert(6 == sides);
    grow(c.x - r, c.y - r, c.x + r, c.y + r);
}
void ui_icon_draw_ex(const char* icon_id, float cx, float cy, float size, float rotation_deg, Color tint) {
    s_icons++;
    s_icon = icon_id;
    s_tinted += 255 != tint.r || 255 != tint.g || 255 != tint.b;
}
void text_draw_compat(const char* text, int x, int y, int size, Color color) { s_texts++; }
Color Fade(Color color, float alpha) { color.a = (unsigned char)(color.a * alpha); return color; }
Color instance_map_presence_color(ImapPresenceStatus status) { return (Color){ (unsigned char)(40 * status), 90, 90, 255 }; }

/* A glyph of `side` px stays in a footprint of `side` + 6 px and never shrinks under 80% of it. */
static void check_footprint(float side, float room) {
    assert(side + room >= s_extent.width && side + room >= s_extent.height);
    assert(side * 0.8f <= s_extent.width && side * 0.8f <= s_extent.height);
}

static bool same(Rectangle a, Rectangle b) { return a.x == b.x && a.y == b.y && a.width == b.width && a.height == b.height; }

int main(void) {
    /* A mark grows with the cell only between its bounds, step by step, never past its maximum. */
    assert(MAP_MARKER_PRESENCE_MIN == map_marker_size(0.0f, MAP_MARKER_PRESENCE_MIN, MAP_MARKER_PRESENCE_MAX));
    assert(MAP_MARKER_PRESENCE_MIN == map_marker_size(MAP_MARKER_CELL_LOW, MAP_MARKER_PRESENCE_MIN, MAP_MARKER_PRESENCE_MAX));
    assert(MAP_MARKER_PRESENCE_MAX == map_marker_size(MAP_MARKER_CELL_HIGH, MAP_MARKER_PRESENCE_MIN, MAP_MARKER_PRESENCE_MAX));
    assert(MAP_MARKER_PRESENCE_MAX == map_marker_size(1e6f, MAP_MARKER_PRESENCE_MIN, MAP_MARKER_PRESENCE_MAX));
    float last = MAP_MARKER_PRESENCE_MIN;
    for (float cell = 0.0f; 4000.0f > cell; cell += 0.05f) {
        float side = map_marker_size(cell, MAP_MARKER_PRESENCE_MIN, MAP_MARKER_PRESENCE_MAX);
        assert(MAP_MARKER_PRESENCE_MIN <= side && MAP_MARKER_PRESENCE_MAX >= side);
        assert(last <= side && 0.05f >= side - last);
        last = side;
    }

    /* A passive bot is a circle, a hostile bot a triangle, a resource a square: shape tells them apart. */
    const float sides[2] = { MAP_MARKER_PRESENCE_MIN, MAP_MARKER_PRESENCE_MAX };
    for (int i = 0; 2 > i; i++) {
        reset();
        map_marker_draw_presence((Vector2){ 100.4f, 100.6f }, sides[i], IMAP_PRESENCE_PASSIVE, 1, 1.0f);
        assert(0 < s_circles && 0 == s_triangles && 0 == s_rects);
        check_footprint(sides[i], 6.0f);
        reset();
        map_marker_draw_presence((Vector2){ 100, 100 }, sides[i], IMAP_PRESENCE_HOSTILE, 1, 1.0f);
        assert(0 == s_circles && 0 < s_triangles && 0 == s_rects);
        check_footprint(sides[i], 6.0f);
        reset();
        map_marker_draw_presence((Vector2){ 100, 100 }, sides[i], IMAP_PRESENCE_RESOURCE, 1, 1.0f);
        assert(0 == s_circles && 0 == s_triangles && 0 < s_rects);
        check_footprint(sides[i], 6.0f);
    }
    /* A group shows its count; a single mark shows none. */
    reset();
    map_marker_draw_presence((Vector2){ 100, 100 }, MAP_MARKER_PRESENCE_MIN, IMAP_PRESENCE_PASSIVE, 1, 1.0f);
    assert(0 == s_texts);
    map_marker_draw_presence((Vector2){ 100, 100 }, MAP_MARKER_PRESENCE_MIN, IMAP_PRESENCE_PASSIVE, 3, 1.0f);
    assert(1 == s_texts);

    /* The entry spawn is the one hexagon, with the home icon in its own colours. */
    for (int i = 0; 2 > i; i++) {
        float side = 0 == i ? MAP_MARKER_SPAWN_MIN : MAP_MARKER_SPAWN_MAX;
        reset();
        map_marker_draw_spawn((Vector2){ 100, 100 }, side, 1.0f);
        assert(3 == s_hexagons && 0 == s_circles && 0 == s_triangles && 0 == s_rects);
        assert(1 == s_icons && 0 == s_tinted && 0 == strcmp("home", s_icon));
        check_footprint(side, 10.0f);
    }
    assert(0 != memcmp(&MAP_MARKER_SPAWN, &MAP_MARKER_PLAYER, sizeof(Color)));

    /* The talk badge is a round plate with the chat icon in its own colours. */
    reset();
    map_marker_draw_talk((Rectangle){ 90, 90, MAP_MARKER_QUEST_MIN, MAP_MARKER_QUEST_MIN }, 1.0f);
    assert(3 == s_circles && 0 == s_triangles && 0 == s_hexagons);
    assert(1 == s_icons && 0 == s_tinted && 0 == strcmp("chat", s_icon));

    Vector2 size = map_marker_label_size("East", 12, false, -1);
    assert(24.0f + 8.0f == size.x && 16.0f == size.y);
    Vector2 arrow = map_marker_label_size("East", 12, true, -1);
    assert(size.x + 7.0f + 4.0f == arrow.x && size.y == arrow.y);
    /* A stat sum takes a gap, the stack icon and its number. */
    Vector2 value = map_marker_label_size("East", 12, false, 44);
    assert(size.x + 4.0f + 14.0f + 2.0f + 12.0f == value.x && size.y == value.y);

    Rectangle bounds = { 0, 0, 400, 400 }, box;
    Vector2 at = { 100, 100 }, label = { 40, 16 };
    assert(map_marker_place_label(at, label, 10.0f, bounds, NULL, 0, &box) && same((Rectangle){ 80, 110, 40, 16 }, box));
    Rectangle taken[4] = { { 80, 110, 40, 16 } };
    assert(map_marker_place_label(at, label, 10.0f, bounds, taken, 1, &box) && same((Rectangle){ 80, 74, 40, 16 }, box));
    taken[1] = (Rectangle){ 80, 74, 40, 16 };
    assert(map_marker_place_label(at, label, 10.0f, bounds, taken, 2, &box) && same((Rectangle){ 110, 92, 40, 16 }, box));
    taken[2] = (Rectangle){ 110, 92, 40, 16 };
    assert(map_marker_place_label(at, label, 10.0f, bounds, taken, 3, &box) && same((Rectangle){ 50, 92, 40, 16 }, box));
    taken[3] = (Rectangle){ 50, 92, 40, 16 };
    assert(!map_marker_place_label(at, label, 10.0f, bounds, taken, 4, &box));

    /* A label never leaves the bounds. */
    assert(map_marker_place_label((Vector2){ 395, 395 }, label, 10.0f, bounds, NULL, 0, &box));
    assert(358.0f == box.x && 382.0f == box.y);

    puts("map_marker_test OK");
    return 0;
}
