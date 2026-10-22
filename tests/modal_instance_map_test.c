#define RAYMATH_IMPLEMENTATION
#include "ui/modal_instance_map.c"
#include "data/instance_map_data.c"
#include "data/quest_progress_store.c"
#include "data/quest_target.c"
#include "data/map_route.c"

#include <assert.h>
#include <float.h>
#include <math.h>
#include <raylib.h>
#include <stdio.h>
#include <string.h>

static Rectangle s_rect;
static Rectangle s_plate;
static int s_label_x, s_label_y, s_label_size, s_texts, s_lines;
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
    s_texts++;
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
LocalPlayer g_local_player;
GameState g_game_state;
const Color MODAL_QUEST = { 0 }, MODAL_ACTION = { 0 }, MAP_MARKER_PLAYER = { 255, 52, 64, 255 },
            MAP_MARKER_SPAWN = { 70, 230, 120, 255 }, MAP_MARKER_ROUTE = { 255, 190, 90, 255 };
int GetScreenWidth(void) { return 1280; }
int GetScreenHeight(void) { return 800; }
int game_state_slot(EntityID id) { return 0; }
static RenderState s_own_render;
static float s_player_r;
const RenderState* game_state_own_render(void) { return &s_own_render; }
uint8_t local_player_status_icon(void) { return 0; }
const char* presentation_runtime_status_icon(uint8_t status_id) { return NULL; }
Vector2 local_player_view_facing(void) { return (Vector2){ 0, -1 }; }
void ui_icon_draw_ex(const char* icon_id, float cx, float cy, float size, float rotation_deg, Color tint) { assert(icon_id); }
void map_marker_draw_pulse(Vector2 at, Color color, float alpha) {}
void map_marker_draw_player(Vector2 at, float radius, Vector2 facing, float alpha, double time) { s_player_r = radius; }
void map_marker_draw_gate(Vector2 at, float radius, Color color, float alpha) {}
/* The last quest badge and the talk badges of a frame. */
static Rectangle s_quest_box;
static int s_talks;
static float s_talk_side;
void map_marker_draw_quest(Rectangle box, const char* icon, bool muted, int count, float alpha) { s_quest_box = box; }
void map_marker_draw_talk(Rectangle box, float alpha) {
    s_talks++;
    s_talk_side = box.width;
}
Rectangle map_marker_pin_rect(Vector2 cell, float side) { return (Rectangle){ cell.x - side * 0.5f, cell.y - side, side, side }; }
void map_marker_draw_pin(Vector2 cell, float side, const char* icon, int count, Color accent, float alpha) {}
const char* map_detail_service_icon(ImapService service) { return "service"; }
const char* map_detail_quest_icon(ImapQuestState state) { return "quest"; }

/* The bot and resource marks of the last overlay: their sizes, places, classes and counts, in draw order. */
#define DRAWN_MAX 16
static int s_drawn;
static float s_side_min, s_side_max;
static Vector2 s_drawn_at[DRAWN_MAX];
static int s_drawn_count[DRAWN_MAX];
static ImapPresenceStatus s_drawn_class[DRAWN_MAX];
void map_marker_draw_presence(Vector2 at, float side, ImapPresenceStatus status, int count, float alpha) {
    assert(DRAWN_MAX > s_drawn);
    s_side_min = fminf(s_side_min, side);
    s_side_max = fmaxf(s_side_max, side);
    s_drawn_at[s_drawn] = at;
    s_drawn_count[s_drawn] = count;
    s_drawn_class[s_drawn++] = status;
}
/* A label side meets nothing but `s_block`. */
static Rectangle s_block;
bool map_marker_place_label(Vector2 anchor, Vector2 size, float gap, Rectangle bounds, const Rectangle* taken,
                            int count, Rectangle* out) {
    for (int i = 0; count > i; i++)
        if (0 == memcmp(&taken[i], &s_block, sizeof s_block)) return false;
    return true;
}
static int s_spawns;
static float s_spawn_side;
static Vector2 s_spawn_drawn;
void map_marker_draw_spawn(Vector2 at, float side, float alpha) {
    s_spawns++;
    s_spawn_side = side;
    s_spawn_drawn = at;
}
float toolbar_height(void) { return 48.0f; }
UITabs ui_tabs_row(int count, float x, float y, float width, float tab_h) {
    return (UITabs){ x, y, width / (float)count, tab_h, count, count };
}
int ui_tabs_hit(const UITabs* group, int mx, int my) { return -1; }
static int s_row = -1;
int map_detail_row_at(Vector2 at) { return s_row; }
void map_detail_entry_text(const ImapEntrySpawn* spawn, char* out, size_t size) { copy_str(out, size, "A (5, 6)"); }

/* The quest target marks of the last frame, and the labels it drew in order. */
static int s_targets_drawn, s_targets_followed;
static float s_target_side_max;
void map_marker_draw_target(Rectangle box, bool followed, float alpha, double time) {
    s_targets_drawn++;
    s_targets_followed += followed;
    s_target_side_max = fmaxf(s_target_side_max, fmaxf(box.width, box.height));
}
static char s_drawn_labels[8][96];
static int s_drawn_values[8];
static int s_label_draws;
Vector2 map_marker_label_size(const char* text, int font, bool arrow, int value) {
    return (Vector2){ 6.0f * strlen(text), 16 };
}
void map_marker_draw_label(Rectangle box, const char* text, int font, Color color, bool arrow, bool strong, int value,
                           float alpha) {
    if (8 > s_label_draws) {
        copy_str(s_drawn_labels[s_label_draws], sizeof s_drawn_labels[0], text);
        s_drawn_values[s_label_draws] = value;
    }
    s_label_draws++;
}
int map_detail_next_page(void) { return 6; }
bool CheckCollisionPointRec(Vector2 p, Rectangle r) {
    return p.x >= r.x && p.x < r.x + r.width && p.y >= r.y && p.y < r.y + r.height;
}
/* The metadata requests: how many, and the last one. */
static int s_fetches;
static char s_fetch_id[64], s_fetch_url[256];
static FetchCompletedCb s_fetch_done;
void fetch_request_start(const char* id, const char* url, FetchCompletedCb callback) {
    s_fetches++;
    copy_str(s_fetch_id, sizeof s_fetch_id, id);
    copy_str(s_fetch_url, sizeof s_fetch_url, url);
    s_fetch_done = callback;
}
static int s_forgotten;
static char s_forgotten_url[256];
void fetch_forget(const char* url) {
    s_forgotten++;
    copy_str(s_forgotten_url, sizeof s_forgotten_url, url);
}
void fetch_data_release(void* data) {}
Vector2 graph_layout_pack(const Vector2* cells, const Vector2* sizes, int count, float gap, Vector2* centers) {
    return (Vector2){ 0 };
}
void input_gestures_set_blocked(bool blocked) {}
static int s_minimap_hides;
void hud_minimap_overlay_hide(void) { s_minimap_hides++; }
void audio_event(const char* logic_event_id) {}
const QuestMetadataEntry* quest_cache_get(const char* code) { return NULL; }
void modal_opened(void) {}
void modal_closed(void) {}
void modal_map_set_expanded(bool expanded) {}
void session_gate_travel(const char* code, const char* path, const char* name, bool in_place) {}
static int s_universe_opens;
void universe_map_data_open(void) { s_universe_opens++; }
void universe_map_data_close(void) {}
void universe_map_data_retry(void) {}
int universe_map_data_generation(void) { return 0; }
UmapTravel universe_map_data_travel(int node) { return UMAP_TRAVEL_NO_ROUTE; }
bool universe_map_data_in_place(int node) { return false; }

static void check_label(Rectangle card, const char* name) {
    int before = s_texts;
    draw_node_name(name, card, false, 1.0f);
    assert(before + 1 == s_texts);
    Vector2 center = ui_rect_center(card);
    assert(1.0f >= fabsf(s_plate.x + s_plate.width * 0.5f - center.x));
    assert(1.0f >= fabsf(s_plate.y + s_plate.height * 0.5f - center.y));
    assert(card.width * 0.8f + 1.0f >= s_plate.width);
    assert(card.x <= s_plate.x && card.x + card.width >= s_plate.x + s_plate.width);
    assert(card.y <= s_plate.y && card.y + card.height >= s_plate.y + s_plate.height);
    assert(s_plate.y <= s_label_y && s_plate.y + s_plate.height >= s_label_y + s_label_size);
    assert(10 <= s_label_size && 20 >= s_label_size);
}

/* Draws the bots and resources of map 0 after a clean record. */
static void draw_overlay(void) {
    s_drawn = 0;
    s_side_min = FLT_MAX;
    s_side_max = 0.0f;
    s_hit_count = 0;
    draw_node_overlay(0, 1.0f, 0.0);
}

/* The `test` world: two maps side by side and one as wide as both under them. */
static void load_test_world(void) {
    memset(&s_graph, 0, sizeof s_graph);
    s_graph.node_count = 3;
    s_graph.cell = 1.0f / 128.0f;
    s_graph.span = (Vector2){ 1.0f, 114.0f / 128.0f };
    const float top = -57.0f / 128.0f, middle = 7.0f / 128.0f, bottom = 57.0f / 128.0f;
    s_graph.nodes[0] = (ImapNode){ .map_code = "a", .extent_w = 0.5f, .extent_h = 0.5f, .grid_x = 64, .grid_y = 64,
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
    draw_edge_line(&s_graph.edges[0], 0, LINK_PLAIN, 1.0f, 0.0);
    assert(0 == s_lines);
    s_m.selected_node = 2;
    draw_edge_line(&s_graph.edges[0], 0, LINK_PLAIN, 1.0f, 0.0);
    assert(0 < s_lines);
    s_lines = 0;
    draw_edge_line(&s_graph.edges[1], 1, LINK_PLAIN, 1.0f, 0.0);
    assert(0 == s_lines);
    /* A portal in focus shows its link even across a shared edge. */
    draw_edge_line(&s_graph.edges[1], 1, LINK_FOCUS, 1.0f, 0.0);
    assert(0 < s_lines);
    s_graph.edges[1].joined = false;
    s_lines = 0;
    draw_edge_line(&s_graph.edges[1], 1, LINK_PLAIN, 1.0f, 0.0);
    assert(0 < s_lines);

    /* The selected or hovered portal leads; while a portal is selected, the other links step back. */
    s_m.selected = (MapDetail){ MAP_DETAIL_MAP, 2, -1 };
    assert(LINK_PLAIN == link_emphasis(0, NULL) && LINK_PLAIN == link_emphasis(1, NULL));
    MapHit gate = { .kind = MAP_DETAIL_PORTAL, .index = 1 };
    assert(LINK_FOCUS == link_emphasis(1, &gate));
    s_m.selected = (MapDetail){ MAP_DETAIL_PORTAL, 0, -1 };
    assert(LINK_FOCUS == link_emphasis(0, NULL) && LINK_DIM == link_emphasis(1, NULL));

    /* A tap on a mark selects it and inspects its map; a tap on empty space clears both. */
    s_hit_count = 0;
    add_hit((Rectangle){ 100, 300, 20, 20 }, MAP_DETAIL_PORTAL, 1, 1);
    s_card = (Rectangle){ 0 };
    resolve_click((Vector2){ 110, 310 });
    assert(MAP_DETAIL_PORTAL == s_m.selected.kind && 1 == s_m.selected.index && 0 == s_m.selected_node);
    assert(0 == strcmp("a", s_m.selected_code));
    resolve_click((Vector2){ 5, 795 });
    assert(MAP_DETAIL_NONE == s_m.selected.kind && -1 == s_m.selected_node && '\0' == s_m.selected_code[0]);
    s_hit_count = 0;
    s_m.selected_node = 2;

    /* Selection and hover outline a map: no fill covers its picture. */
    s_rect = (Rectangle){ -1, -1, 0, 0 };
    draw_node_marks(2, 2, 1.0f, 0.0);
    draw_node_marks(1, 1, 1.0f, 0.0);
    assert(-1.0f == s_rect.x);

    /* The own player keeps a readable mark from the widest zoom to the closest. */
    copy_str(g_local_player.map_code, sizeof g_local_player.map_code, "a");
    s_own_render.interp_pos = (Vector2){ 32, 32 };
    g_game_state.mask[0] = COMP_PROGRESS;
    s_m.zoom = IMAP_ZOOM_MIN;
    draw_player_marker(1.0f, 0.0);
    assert(MAP_MARKER_PLAYER_MIN_R == s_player_r);
    s_m.zoom = imap_zoom_max();
    draw_player_marker(1.0f, 0.0);
    assert(MAP_MARKER_PLAYER_MIN_R < s_player_r && MAP_MARKER_PLAYER_MAX_R >= s_player_r);
    s_m.zoom = 5.0f;

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
    int before = s_texts;
    draw_node_name("Gate", (Rectangle){ 0, 0, 24, 16 }, false, 1.0f);
    assert(before == s_texts);

    char label[64];
    fit_label(label, sizeof label, "A very long map name", 13, 45);
    assert(45 >= MeasureText(label, 13) && NULL != strstr(label, "..."));
    fit_label(label, sizeof label, "A very long map name", 13, 4);
    assert('\0' == label[0]);
    fit_label(label, sizeof label, "Árbol del río", 13, 35);
    assert(35 >= MeasureText(label, 13) && NULL != strstr(label, "..."));
    /* Bots and resources keep a screen size between their bounds at every zoom: no step where the
     * player's tag shows, the maximum at the closest zoom, and never a larger hit. */
    const int cells[5][2] = { { 10, 10 }, { 11, 10 }, { 30, 30 }, { 31, 30 }, { 40, 40 } };
    const ImapPresenceStatus classes[5] = { IMAP_PRESENCE_PASSIVE, IMAP_PRESENCE_PASSIVE, IMAP_PRESENCE_HOSTILE,
                                            IMAP_PRESENCE_RESOURCE, IMAP_PRESENCE_PASSIVE };
    s_graph.presence_poi_count = 5;
    for (int i = 0; 5 > i; i++)
        s_graph.presence_pois[i] = (ImapPresencePoi){ .node = 0, .cell_x = cells[i][0], .cell_y = cells[i][1],
                                                      .presence_status = classes[i], .first_placed = -1 };
    s_m.panel = (Rectangle){ 0, 0, 1280, 800 };
    s_m.pan = (Vector2){ 0 };
    s_m.selected = (MapDetail){ MAP_DETAIL_NONE, -1, -1 };
    float last = 0.0f;
    for (float zoom = IMAP_ZOOM_MIN; imap_zoom_max() >= zoom; zoom *= 1.01f) {
        s_m.zoom = zoom;
        draw_overlay();
        assert(MAP_MARKER_PRESENCE_MIN <= s_side_min && MAP_MARKER_PRESENCE_MAX >= s_side_max && s_side_min == s_side_max);
        assert(0.0f == last || 0.25f >= fabsf(s_side_max - last));
        for (int h = 0; s_hit_count > h; h++) {
            float badge = 1 < s_hits[h].count ? IMAP_BADGE_W : 0.0f;
            assert(MAP_MARKER_PRESENCE_MAX + 6.0f >= s_hits[h].rect.height);
            assert(s_hits[h].rect.width == s_hits[h].rect.height + badge);
        }
        last = s_side_max;
    }
    float tag_zoom = IMAP_TAG_CELL_PX / (IMAP_NODE_SIDE * s_graph.cell);
    s_m.zoom = tag_zoom * 0.999f;
    draw_overlay();
    float below = s_side_max;
    s_m.zoom = tag_zoom * 1.001f;
    draw_overlay();
    assert(0.05f > fabsf(s_side_max - below));
    s_m.zoom = imap_zoom_max();
    draw_overlay();
    assert(MAP_MARKER_PRESENCE_MAX == s_side_max && 5 == s_drawn);
    /* Close up, no mark groups; resources draw first and hostile bots last, on top. */
    for (int i = 0; 5 > i; i++) assert(1 == s_drawn_count[i]);
    assert(IMAP_PRESENCE_RESOURCE == s_drawn_class[0] && IMAP_PRESENCE_HOSTILE == s_drawn_class[4]);
    /* A mark stays on its cell: zoom moves it with the map. */
    for (int z = 0; 2 > z; z++) {
        s_m.zoom = 0 == z ? 20.0f : imap_zoom_max() * 0.5f;
        draw_overlay();
        assert(5 == s_drawn);
        ImapBounds b = s_graph.nodes[0].bounds;
        Vector2 cell = screen_point((Vector2){ b.left + (b.right - b.left) * 40.5f / 64.0f,
                                               b.top + (b.bottom - b.top) * 40.5f / 64.0f });
        assert(1.0f >= fabsf(s_drawn_at[3].x - cell.x) && 1.0f >= fabsf(s_drawn_at[3].y - cell.y));
    }

    /* Far out, overlapping marks of one class group with a count; classes never mix; the selection stays apart. */
    s_m.zoom = IMAP_ZOOM_MIN;
    draw_overlay();
    assert(4 == s_drawn);
    int groups = 0;
    for (int i = 0; s_drawn > i; i++) groups += 2 == s_drawn_count[i] && IMAP_PRESENCE_PASSIVE == s_drawn_class[i];
    assert(1 == groups);
    s_m.selected = (MapDetail){ MAP_DETAIL_PLACE, 1, -1 };
    draw_overlay();
    assert(5 == s_drawn && MAP_MARKER_PRESENCE_MIN == s_side_max);
    s_m.selected = (MapDetail){ MAP_DETAIL_NONE, -1, -1 };
    /* A tap on a group zooms in on it and selects nothing. */
    s_hit_count = 0;
    s_card = (Rectangle){ 0 };
    add_hit((Rectangle){ 600, 400, 20, 20 }, MAP_DETAIL_PLACE, 0, 2);
    s_m.zoom_target = 1.0f;
    resolve_click((Vector2){ 610, 410 });
    assert(IMAP_GROUP_ZOOM == s_m.zoom_target && MAP_DETAIL_NONE == s_m.selected.kind);

    /* A leading label may cover a bot or a resource, never another mark; a plain label covers nothing. */
    s_graph.presence_pois[5] = (ImapPresencePoi){ .node = 0, .cell_x = 50, .cell_y = 50, .quest_count = 1,
                                                  .first_placed = -1 };
    MapHit bot = { .kind = MAP_DETAIL_PLACE, .index = 0 }, place = { .kind = MAP_DETAIL_PLACE, .index = 5 };
    MapHit portal = { .kind = MAP_DETAIL_PORTAL, .index = 0 };
    assert(presence_hit(&bot) && !presence_hit(&place) && !presence_hit(&portal));
    s_block = (Rectangle){ 10, 10, 9, 9 };
    Rectangle out, list[2] = { s_block, { 50, 50, 20, 20 } };
    assert(!place_label((Vector2){ 0 }, (Vector2){ 40, 16 }, 6.0f, s_m.panel, list, 2, 1, false, &out));
    assert(place_label((Vector2){ 0 }, (Vector2){ 40, 16 }, 6.0f, s_m.panel, list, 2, 1, true, &out));
    assert(!place_label((Vector2){ 0 }, (Vector2){ 40, 16 }, 6.0f, s_m.panel, list, 2, 0, true, &out));

    /* Quest targets: a wolf to defeat on map a, twice, and once on map c, one portal away. */
    s_graph.presence_pois[6] = (ImapPresencePoi){ .node = 2, .cell_x = 100, .cell_y = 40,
                                                  .presence_status = IMAP_PRESENCE_HOSTILE, .first_placed = 2 };
    s_graph.presence_poi_count = 7;
    s_graph.presence_pois[0].first_placed = 0;
    s_graph.presence_pois[1].first_placed = 1;
    s_graph.placed[0] = (ImapPlaced){ .poi = 0, .bot = true, .item_id = "wolf", .mortal = true, .stats = 44 };
    s_graph.placed[1] = (ImapPlaced){ .poi = 1, .bot = true, .item_id = "wolf", .mortal = true, .stats = 44 };
    s_graph.placed[2] = (ImapPlaced){ .poi = 6, .bot = true, .item_id = "wolf", .mortal = true, .stats = 54 };
    s_graph.placed_count = 3;
    s_state = IMAP_DATA_READY;
    s_generation++;
    QuestSnapshot hunt = { .code = "q", .status = "active", .active_step = "Hunt", .step_id = "s", .objective_count = 1,
                           .objectives = { { .type = QUEST_OBJECTIVE_KILL, .item_id = "wolf", .current = 1,
                                             .required = 3 } } };
    quest_progress_store_upsert(&hunt);
    quest_target_update();
    const QuestTarget* wolf = quest_target_find("q", "s", QUEST_OBJECTIVE_KILL, "wolf");
    assert(3 == wolf->destination_count && 1 == wolf->destinations[2].hops);
    /* A target never groups: far out, the two wolves of map a stay two marks. */
    s_m.selected = (MapDetail){ MAP_DETAIL_NONE, -1, -1 };
    s_m.zoom = IMAP_ZOOM_MIN;
    mark_targets();
    draw_overlay();
    assert(5 == s_drawn);
    for (int i = 0; s_drawn > i; i++) assert(1 == s_drawn_count[i]);
    /* Brackets keep a bounded size from the widest zoom to the closest; only the followed destination in focus breathes. */
    quest_target_track(wolf, 2);
    for (int z = 0; 2 > z; z++) {
        s_m.zoom = 0 == z ? IMAP_ZOOM_MIN : imap_zoom_max();
        mark_targets();
        draw_overlay();
        s_targets_drawn = s_targets_followed = 0;
        s_target_side_max = 0.0f;
        draw_quest_targets(1.0f, 0.0);
        assert(3 == s_targets_drawn && 1 == s_targets_followed);
        assert(MAP_MARKER_PRESENCE_MAX + 6.0f >= s_target_side_max);
    }
    /* With nothing selected, the route to the followed destination on another map leads in the route colour. */
    select_node(-1);
    refresh_route();
    assert(LINK_ROUTE == link_emphasis(0, NULL) && LINK_PLAIN == link_emphasis(1, NULL));
    /* Locating shows the objective card, the destination's map, and its cell at the centre, close up: also
     * while the panel still grows from the compact readout. */
    s_m.panel = (Rectangle){ 20, 10, 200, 30 };
    focus_destination(wolf, 2);
    s_m.panel = (Rectangle){ 0, 0, 1280, 800 };
    float zoom = IMAP_LOCATE_CELL_PX / (IMAP_NODE_SIDE * s_graph.cell);
    assert(MAP_DETAIL_OBJECTIVE == s_m.selected.kind && 2 == s_m.selected_node && 1e-4f > fabsf(zoom - s_m.zoom_target));
    s_m.zoom = s_m.zoom_target;
    s_m.pan = s_m.pan_target;
    Vector2 cell = instance_map_cell_to_card(node_rect(2), &s_graph.nodes[2], 100.5f, 40.5f);
    Rectangle area = frame_area();
    assert(1.0f >= fabsf(cell.x - (area.x + area.width * 0.5f)) && 1.0f >= fabsf(cell.y - (area.y + area.height * 0.5f)));
    /* A row of the card puts another destination in focus; the last row stops following. */
    s_hit_count = 0;
    s_card = (Rectangle){ 0, 300, 300, 300 };
    s_row = 0;
    resolve_click((Vector2){ 10, 400 });
    assert(wolf == quest_target_tracked() && 0 == quest_target_tracked_destination() && 0 == s_m.selected_node);
    refresh_route();
    assert(LINK_PLAIN == link_emphasis(0, NULL));
    /* The followed objective labels its destination in focus first, then the entry spawn; its other wolf
     * nearby repeats that text and takes no label. */
    s_graph.entry_spawn = (ImapEntrySpawn){ .mode = IMAP_SPAWN_FIXED, .map_code = "a", .cell_x = 12, .cell_y = 12 };
    s_m.zoom = s_m.zoom_target;
    s_m.pan = s_m.pan_target;
    mark_targets();
    draw_overlay();
    draw_entry_spawn(1.0f);
    s_label_draws = 0;
    draw_labels(NULL, s_m.panel, (Rectangle){ 0 }, (Rectangle){ 0 }, 1.0f);
    assert(2 == s_label_draws && 0 == strcmp("DEFEAT wolf", s_drawn_labels[0]));
    assert(0 == strcmp("ENTRY SPAWN: A (5, 6)", s_drawn_labels[1]));
    s_row = MAP_DETAIL_UNFOLLOW;
    resolve_click((Vector2){ 10, 400 });
    assert(NULL == quest_target_tracked() && MAP_DETAIL_NONE == s_m.selected.kind);
    /* The objective row of a target follows it, with that target in focus. */
    s_m.selected = (MapDetail){ MAP_DETAIL_PLACE, wolf->destinations[1].poi, -1 };
    s_row = MAP_DETAIL_FOLLOW + (int)(wolf - quest_target_get(0));
    resolve_click((Vector2){ 10, 400 });
    assert(wolf == quest_target_tracked() && 1 == quest_target_tracked_destination());
    assert(MAP_DETAIL_OBJECTIVE == s_m.selected.kind);
    quest_target_untrack();
    s_row = -1;
    s_card = (Rectangle){ 0 };
    s_graph.entry_spawn = (ImapEntrySpawn){ 0 };

    /* A fixed entry spawn shows on its cell at a bounded size at every zoom, and a tap opens its card. */
    s_graph.entry_spawn = (ImapEntrySpawn){ .mode = IMAP_SPAWN_FIXED, .map_code = "a", .map_name = "A", .cell_x = 5,
                                            .cell_y = 6 };
    for (float zoom = IMAP_ZOOM_MIN; imap_zoom_max() >= zoom; zoom *= 1.05f) {
        s_m.zoom = zoom;
        s_hit_count = s_spawns = 0;
        draw_entry_spawn(1.0f);
        assert(1 == s_spawns && MAP_MARKER_SPAWN_MIN <= s_spawn_side && MAP_MARKER_SPAWN_MAX >= s_spawn_side);
        Vector2 cell = instance_map_cell_to_card(node_rect(0), &s_graph.nodes[0], 5.5f, 6.5f);
        assert(cell.x == s_spawn_drawn.x && cell.y == s_spawn_drawn.y);
        assert(1 == s_hit_count && MAP_DETAIL_SPAWN == s_hits[0].kind);
    }
    s_m.zoom = 5.0f;
    s_hit_count = 0;
    draw_entry_spawn(1.0f);
    resolve_click(s_spawn_drawn);
    assert(MAP_DETAIL_SPAWN == s_m.selected.kind && 0 == s_m.selected_node);
    /* A random spawn, an unresolved one or one on a map this world lacks shows no cell. */
    const ImapEntrySpawn hidden[3] = { { .mode = IMAP_SPAWN_RANDOM },
                                       { .mode = IMAP_SPAWN_UNRESOLVED, .map_code = "a", .cell_x = 5, .cell_y = 6 },
                                       { .mode = IMAP_SPAWN_FIXED, .map_code = "elsewhere", .cell_x = 5, .cell_y = 6 } };
    for (int i = 0; 3 > i; i++) {
        s_graph.entry_spawn = hidden[i];
        s_hit_count = s_spawns = 0;
        draw_entry_spawn(1.0f);
        assert(0 == s_spawns && 0 == s_hit_count && !s_spawn_shown);
    }

    /* A selected map shows the route to it from the player's map, built again only when the map or the tree
     * changes; the player's own map shows none, and neither does a reopened map. */
    s_graph.nodes[1].map_code[0] = 'b';
    s_graph.nodes[2].map_code[0] = 'c';
    s_generation++;
    map_route_update();
    select_node(2);
    refresh_route();
    assert(LINK_ROUTE == link_emphasis(0, NULL) && LINK_PLAIN == link_emphasis(1, NULL));
    int built = s_route_revision;
    refresh_route();
    assert(built == s_route_revision);
    select_node(0);
    refresh_route();
    assert(LINK_PLAIN == link_emphasis(0, NULL));
    select_node(2);
    copy_str(g_local_player.map_code, sizeof g_local_player.map_code, "c");
    map_route_update();
    refresh_route();
    assert(built != s_route_revision && LINK_PLAIN == link_emphasis(0, NULL));
    copy_str(g_local_player.map_code, sizeof g_local_player.map_code, "a");
    map_route_update();
    refresh_route();
    assert(LINK_ROUTE == link_emphasis(0, NULL));
    modal_instance_map_toggle();
    refresh_route();
    assert(-1 == s_m.selected_node && LINK_PLAIN == link_emphasis(0, NULL));
    modal_instance_map_close();
    s_state = IMAP_DATA_READY;
    s_generation++;
    quest_target_update();

    /* A quest the player can take keeps a quest badge of its own size at every zoom; it bobs, its hit stays. */
    s_graph.presence_pois[5].quest_state = IMAP_QUEST_AVAILABLE;
    Vector2 place_at = { 400, 400 };
    for (int z = 0; 2 > z; z++) {
        s_m.zoom = 0 == z ? IMAP_ZOOM_MIN : imap_zoom_max();
        Rectangle hit = draw_place(&s_graph.presence_pois[5], place_at, 16.0f, 1.0f, 0.0);
        Rectangle still = s_quest_box;
        assert(MAP_MARKER_QUEST_MIN <= still.width && MAP_MARKER_QUEST_MAX >= still.width);
        Rectangle moved = draw_place(&s_graph.presence_pois[5], place_at, 16.0f, 1.0f, 0.4);
        assert(0 == memcmp(&hit, &moved, sizeof hit) && still.y != s_quest_box.y);
    }
    s_graph.presence_pois[5].quest_state = IMAP_QUEST_LOCKED;
    draw_place(&s_graph.presence_pois[5], place_at, 16.0f, 1.0f, 0.0);
    Rectangle locked = s_quest_box;
    draw_place(&s_graph.presence_pois[5], place_at, 16.0f, 1.0f, 0.4);
    assert(locked.y == s_quest_box.y);

    /* An NPC a talk objective names takes a talk badge as large as a quest badge, at every zoom. */
    QuestSnapshot talk = { .code = "t", .status = "active", .active_step = "Talk", .step_id = "s",
                           .objective_count = 1,
                           .objectives = { { .type = QUEST_OBJECTIVE_TALK, .item_id = "wolf", .required = 1 } } };
    quest_progress_store_upsert(&talk);
    quest_target_update();
    s_m.zoom = IMAP_ZOOM_MIN;
    s_m.selected = (MapDetail){ MAP_DETAIL_NONE, -1, -1 };
    mark_targets();
    draw_overlay();
    s_talks = 0;
    draw_quest_targets(1.0f, 0.0);
    assert(3 == s_talks && MAP_MARKER_QUEST_MIN <= s_talk_side);

    /* The quest on offer labels before every target but the followed one. Close up a bot or a resource names
     * its item and its stat sum as placed; far out it says nothing unless selected. No label covers the card. */
    s_graph.quests[0] = (ImapProvider){ .poi = 5, .code = "offer", .label = "First" };
    s_graph.quest_count = 1;
    s_graph.presence_pois[3].first_placed = 3;
    s_graph.placed[3] = (ImapPlaced){ .poi = 3, .item_id = "ore", .stats = 25 };
    s_graph.placed_count = 4;
    s_graph.entry_spawn = (ImapEntrySpawn){ 0 };
    s_spawn_shown = false;
    s_block = (Rectangle){ -9, -9, 1, 1 };
    s_m.zoom = imap_zoom_max();
    s_m.pan = (Vector2){ 0 };
    mark_targets();
    draw_overlay();
    add_hit(draw_place(&s_graph.presence_pois[5], place_at, 16.0f, 1.0f, 0.0), MAP_DETAIL_PLACE, 5, 1);
    s_label_draws = 0;
    draw_labels(NULL, (Rectangle){ -4000, -4000, 9000, 9000 }, (Rectangle){ 0 }, (Rectangle){ 0 }, 1.0f);
    assert(0 == strcmp("NEW QUEST: First", s_drawn_labels[0]));
    bool ore = false;
    for (int i = 0; 8 > i && s_label_draws > i; i++)
        ore |= 0 == strcmp("ore", s_drawn_labels[i]) && 25 == s_drawn_values[i];
    assert(ore);
    s_m.zoom = IMAP_ZOOM_MIN;
    draw_overlay();
    s_label_draws = 0;
    draw_labels(NULL, (Rectangle){ -4000, -4000, 9000, 9000 }, (Rectangle){ 0 }, (Rectangle){ 0 }, 1.0f);
    for (int i = 0; 8 > i && s_label_draws > i; i++) assert(0 != strcmp("ore", s_drawn_labels[i]));
    s_m.selected = (MapDetail){ MAP_DETAIL_PLACE, 3, -1 };
    s_label_draws = 0;
    draw_labels(NULL, (Rectangle){ -4000, -4000, 9000, 9000 }, (Rectangle){ 0 }, (Rectangle){ 0 }, 1.0f);
    ore = false;
    for (int i = 0; 8 > i && s_label_draws > i; i++) ore |= 0 == strcmp("ore", s_drawn_labels[i]);
    assert(ore);
    s_block = (Rectangle){ 1, 1, 1, 1 };
    s_label_draws = 0;
    draw_labels(NULL, (Rectangle){ -4000, -4000, 9000, 9000 }, s_block, (Rectangle){ 0 }, 1.0f);
    assert(0 == s_label_draws);
    s_block = (Rectangle){ 0 };
    s_m.selected = (MapDetail){ MAP_DETAIL_NONE, -1, -1 };

    /* While a quest is followed and its card is away, a button takes the view back to the destination in focus. */
    const QuestTarget* hunted = quest_target_find("q", "s", QUEST_OBJECTIVE_KILL, "wolf");
    quest_target_track(hunted, 1);
    select_node(-1);
    s_m.panel = (Rectangle){ 0, 0, 1280, 800 };
    Rectangle back = follow_button_rect();
    assert(44.0f <= back.width && IMAP_CARD_BTN_H == back.height);
    s_follow_btn = back;
    s_card = (Rectangle){ 0 };
    resolve_click(ui_rect_center(back));
    assert(MAP_DETAIL_OBJECTIVE == s_m.selected.kind && 1 == quest_target_tracked_destination());
    assert(0.0f == follow_button_rect().width);
    /* A page row of the objective card shows the next page. */
    s_card = (Rectangle){ 0, 300, 300, 300 };
    s_row = MAP_DETAIL_PAGE;
    resolve_click((Vector2){ 10, 400 });
    assert(MAP_DETAIL_OBJECTIVE == s_m.selected.kind && 6 == s_m.selected.item);
    s_row = -1;
    s_card = s_follow_btn = (Rectangle){ 0 };
    quest_target_untrack();
    select_node(-1);

    /* The universe shows the route to a selected world the way it is travelled; a world nothing leads to has none. */
    s_test_universe = (UmapGraph){ .node_count = 4, .current = 0, .link_count = 3 };
    s_test_universe.links[0] = (UmapLink){ 0, 1, false };
    s_test_universe.links[1] = (UmapLink){ 2, 1, true };
    s_test_universe.links[2] = (UmapLink){ 3, 0, false };
    s_m.view = VIEW_UNIVERSE;
    select_node(2);
    refresh_universe_route();
    assert(2 == s_umap_hops[2] && 1 == s_umap_next && s_umap_route[0] && s_umap_route[1] && !s_umap_route[2]);
    assert(!s_umap_reverse[0] && s_umap_reverse[1]);
    select_node(3);
    refresh_universe_route();
    assert(-1 == s_umap_hops[3] && !s_umap_route[0] && !s_umap_route[1] && !s_umap_route[2]);
    select_node(0);
    refresh_universe_route();
    assert(0 == s_umap_hops[0] && !s_umap_route[0]);
    s_m.view = VIEW_WORLD;
    select_node(-1);

    /* One request opens the map. A view switch or a frame asks for nothing more; the universe loads once. A
     * response after the close changes nothing; a reopen asks for the same graph, another world for its own. */
    copy_str(g_game_state.instance_code, sizeof g_game_state.instance_code, "world-a");
    s_fetches = s_universe_opens = s_minimap_hides = 0;
    modal_instance_map_toggle();
    assert(1 == s_fetches && NULL != strstr(s_fetch_url, "/cyberia-instance/instance-map/world-a/static"));
    assert(1 == s_minimap_hides); /* the expanded map takes the place of the minimap */
    char url[256], id[64];
    copy_str(url, sizeof url, s_fetch_url);
    copy_str(id, sizeof id, s_fetch_id);
    FetchCompletedCb done = s_fetch_done;
    const char* world =
        "{\"status\":\"success\",\"data\":{\"instanceCode\":\"world-a\",\"entrySpawn\":{\"mode\":\"random\"},"
        "\"nodes\":[{\"mapCode\":\"a\",\"w\":1,\"h\":1,\"gridX\":16,\"gridY\":16,\"position\":{\"x\":0,\"y\":0}}],"
        "\"edges\":[],\"presencePois\":[{\"mapCode\":\"a\",\"cellX\":2,\"cellY\":2,\"presenceStatus\":\"hostile\"}]}}";
    FetchResponse response = { .asset_id = id, .success = true, .data = (void*)world, .size = strlen(world) };
    done(&response);
    assert(IMAP_DATA_READY == instance_map_data_state() && IMAP_SPAWN_RANDOM == s_graph.entry_spawn.mode);
    int generation = instance_map_data_generation();
    for (int i = 0; 2 > i; i++) {
        select_view(VIEW_UNIVERSE);
        select_view(VIEW_WORLD);
    }
    for (int i = 0; 10 > i; i++) {
        draw_overlay();
        draw_entry_spawn(1.0f);
    }
    assert(1 == s_fetches && 1 == s_universe_opens && generation == instance_map_data_generation());
    modal_instance_map_close();
    done(&response);
    assert(IMAP_DATA_IDLE == instance_map_data_state() && generation == instance_map_data_generation());
    modal_instance_map_toggle();
    assert(2 == s_fetches && 0 == strcmp(url, s_fetch_url));
    modal_instance_map_close();
    copy_str(g_game_state.instance_code, sizeof g_game_state.instance_code, "world-b");
    modal_instance_map_toggle();
    assert(3 == s_fetches && NULL != strstr(s_fetch_url, "/cyberia-instance/instance-map/world-b/static"));
    /* A world rebuild forgets the held graph: an open map fetches it again, a closed one on its next open. */
    s_forgotten = 0;
    instance_map_data_refresh();
    assert(1 == s_forgotten && 0 == strcmp(s_fetch_url, s_forgotten_url) && 4 == s_fetches);
    assert(IMAP_DATA_LOADING == instance_map_data_state());
    /* RETRY after a failed fetch forgets the failure and fetches again; it does nothing while a fetch runs. */
    FetchResponse failed = { .asset_id = s_fetch_id, .success = false };
    s_fetch_done(&failed);
    assert(IMAP_DATA_ERROR == instance_map_data_state());
    instance_map_data_retry();
    assert(2 == s_forgotten && 5 == s_fetches && IMAP_DATA_LOADING == instance_map_data_state());
    instance_map_data_retry();
    assert(5 == s_fetches);
    modal_instance_map_close();
    instance_map_data_refresh();
    assert(3 == s_forgotten && 5 == s_fetches && IMAP_DATA_IDLE == instance_map_data_state());
    puts("modal_instance_map_test OK");
    return 0;
}
