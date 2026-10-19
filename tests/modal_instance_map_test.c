#define RAYMATH_IMPLEMENTATION
#include "ui/modal_instance_map.c"
#include "data/instance_map_data.c"

#include <assert.h>
#include <math.h>
#include <raylib.h>
#include <stdio.h>
#include <string.h>

static Rectangle s_rect;
static Rectangle s_plate;
static int s_label_x, s_label_y, s_label_size, s_labels, s_lines;
static UmapGraph s_test_universe;
static Rectangle s_picture;
static Texture2D s_texture = { .id = 1, .width = 512, .height = 256 };

const UmapGraph* universe_map_data_graph(void) { return &s_test_universe; }
int text_measure_compat(const char* text, int size) { return (int)strlen(text) * size / 2; }
int text_line_height(int size) { return size; }
void text_draw_outlined(const char* text, int x, int y, int size, Color fg, Color outline, int rings) {
    s_label_x = x;
    s_label_y = y;
    s_label_size = size;
    s_labels++;
}
void DrawRectangleRec(Rectangle rect, Color color) { s_rect = rect; }
void DrawRectangleLinesEx(Rectangle rect, float width, Color color) { s_plate = rect; }
void DrawRectangle(int x, int y, int width, int height, Color color) {}
void DrawLineEx(Vector2 a, Vector2 b, float thick, Color color) { s_lines++; }
Texture2D fetch_texture(const char* url, int scale, FetchPriority priority, bool visible) { return s_texture; }
void DrawTexturePro(Texture2D texture, Rectangle src, Rectangle dst, Vector2 origin, float angle, Color tint) {
    s_picture = dst;
}
Color Fade(Color color, float alpha) { color.a = (unsigned char)(color.a * alpha); return color; }

static void check_label(Rectangle card, const char* name) {
    int before = s_labels;
    draw_node_name(name, card, false, 1.0f);
    assert(before + 1 == s_labels);
    Vector2 center = ui_rect_center(card);
    assert(1.0f >= fabsf(s_plate.x + s_plate.width * 0.5f - center.x));
    assert(1.0f >= fabsf(s_plate.y + s_plate.height * 0.5f - center.y));
    assert(card.width * 0.8f + 1.0f >= s_plate.width);
    assert(card.x <= s_plate.x && card.x + card.width >= s_plate.x + s_plate.width);
    assert(card.y <= s_plate.y && card.y + card.height >= s_plate.y + s_plate.height);
    assert(s_plate.y <= s_label_y && s_plate.y + s_plate.height >= s_label_y + s_label_size);
    assert(10 <= s_label_size && 20 >= s_label_size);
}

/* The `test` world: two maps side by side and one as wide as both under them. */
static void load_test_world(void) {
    memset(&s_graph, 0, sizeof s_graph);
    s_graph.node_count = 3;
    s_graph.cell = 1.0f / 128.0f;
    s_graph.span = (Vector2){ 1.0f, 114.0f / 128.0f };
    const float top = -57.0f / 128.0f, middle = 7.0f / 128.0f, bottom = 57.0f / 128.0f;
    s_graph.nodes[0] = (ImapNode){ .extent_w = 0.5f, .extent_h = 0.5f, .grid_x = 64, .grid_y = 64,
                                   .bounds = { -0.5f, top, 0.0f, middle } };
    s_graph.nodes[1] = (ImapNode){ .extent_w = 0.5f, .extent_h = 0.5f, .grid_x = 64, .grid_y = 64,
                                   .bounds = { 0.0f, top, 0.5f, middle } };
    s_graph.nodes[2] = (ImapNode){ .extent_w = 1.0f, .extent_h = 50.0f / 128.0f, .grid_x = 128, .grid_y = 50,
                                   .bounds = { -0.5f, middle, 0.5f, bottom } };
    s_graph.edge_count = 2;
    s_graph.edges[0] = (ImapEdge){ .source_node = 0, .target_node = 2, .joined = true,
                                   .source_cell_x = 10, .source_cell_y = 60, .target_cell_x = 100, .target_cell_y = 40 };
    s_graph.edges[1] = (ImapEdge){ .source_node = 0, .target_node = 1, .joined = true,
                                   .source_cell_x = 60, .source_cell_y = 10, .target_cell_x = 4, .target_cell_y = 10 };
}

int main(void) {
    load_test_world();
    /* At every zoom, pan and panel size the surface has no seam and every map keeps its own area. */
    for (int i = 0; 6000 > i; i++) {
        s_m.panel = (Rectangle){ 0, 0, 640.0f + (float)(i % 7) * 97.3f, 360.0f + (float)(i % 5) * 61.7f };
        s_m.zoom = 0.45f + (float)i * 0.0021f;
        s_m.pan = (Vector2){ 0.37f + (float)i * 0.013f, -2.71f + (float)i * 0.007f };
        Rectangle a = node_rect(0), b = node_rect(1), c = node_rect(2);
        assert(a.x + a.width == b.x && a.y == b.y && a.height == b.height);
        assert(a.y + a.height == c.y && b.y + b.height == c.y);
        assert(a.x == c.x && b.x + b.width == c.x + c.width);
        assert(0 == node_at_point(b.x - 0.01f, b.y + b.height * 0.5f));
        assert(1 == node_at_point(b.x, b.y + b.height * 0.5f));
        assert(2 == node_at_point(a.x + a.width * 0.5f, c.y));
        assert(2 == node_at_point(b.x + b.width * 0.5f, c.y));
        assert(-1 == node_at_point(c.x + c.width, c.y));
        assert(-1 == node_at_point(a.x + a.width * 0.5f, a.y - 1));
    }

    /* A link across a shared edge shows only while one of its maps is selected; a link apart always shows. */
    s_m.zoom = 5.0f;
    s_m.pan = (Vector2){ 0 };
    s_m.panel = (Rectangle){ 0, 0, 1280, 800 };
    s_m.selected_node = -1;
    s_lines = 0;
    draw_edge(&s_graph.edges[0], 0, 1.0f, 0.0);
    assert(0 == s_lines);
    s_m.selected_node = 2;
    draw_edge(&s_graph.edges[0], 0, 1.0f, 0.0);
    assert(0 < s_lines);
    s_lines = 0;
    draw_edge(&s_graph.edges[1], 1, 1.0f, 0.0);
    assert(0 == s_lines);
    s_graph.edges[1].joined = false;
    draw_edge(&s_graph.edges[1], 1, 1.0f, 0.0);
    assert(0 < s_lines);

    /* The rim of each map lies outside it; its picture covers the shared part. */
    Rectangle card = node_rect(2);
    draw_surface_rim(1.0f);
    assert(s_rect.x == card.x - 1.0f && s_rect.width == card.width + 2.0f);

    /* A picture of its map proportions fills the map; another keeps its own inside it. */
    draw_picture(node_rect(0), (Vector2){ 0.5f, 0.5f }, "/map.png", "Gate", FETCH_P1, 1.0f);
    assert(1.0f >= fabsf(s_picture.width - 2.0f * s_picture.height));
    s_texture.height = 512;
    draw_picture(node_rect(0), (Vector2){ 0.5f, 0.5f }, "/map.png", "Gate", FETCH_P1, 1.0f);
    Rectangle a = node_rect(0);
    assert(a.x == s_picture.x && a.y == s_picture.y && a.width == s_picture.width && a.height == s_picture.height);
    draw_picture((Rectangle){ 0, 0, 200, 100 }, (Vector2){ 2, 1 }, "/map.png", "Gate", FETCH_P1, 1.0f);
    assert(50.0f == s_picture.x && 100.0f == s_picture.width && 100.0f == s_picture.height);

    /* Name plates sit at the centre of their cards, sized with them. */
    check_label((Rectangle){ 20, 40, 320, 180 }, "Forest Gate");
    check_label((Rectangle){ 20, 40, 95, 48 }, "The Valley of the Forgotten Kingdom");
    check_label((Rectangle){ 20, 40, 160, 80 }, "Gate");
    int small = s_label_size;
    check_label((Rectangle){ 20, 40, 640, 360 }, "Gate");
    assert(small < s_label_size);
    /* A long name shrinks its text before it loses letters. */
    check_label((Rectangle){ 20, 40, 300, 200 }, "Amethyst Foliage Expanse");
    assert(20 > s_label_size && 10 < s_label_size);
    int before = s_labels;
    draw_node_name("Gate", (Rectangle){ 0, 0, 24, 16 }, false, 1.0f);
    assert(before == s_labels);

    char label[64];
    fit_label(label, sizeof label, "A very long map name", 13, 45);
    assert(45 >= MeasureText(label, 13) && NULL != strstr(label, "..."));
    fit_label(label, sizeof label, "A very long map name", 13, 4);
    assert('\0' == label[0]);
    fit_label(label, sizeof label, "Árbol del río", 13, 35);
    assert(35 >= MeasureText(label, 13) && NULL != strstr(label, "..."));
    puts("modal_instance_map_test OK");
    return 0;
}
