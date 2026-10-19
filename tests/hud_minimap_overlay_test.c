#include "ui/hud_minimap_overlay.c"

#include <assert.h>
#include <raylib.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

static Rectangle s_bounds = { 20, 48, 300, 300 };
static bool s_texture_ready;
static int s_texture_draws, s_fills;
static unsigned char s_texture_alpha;

Rectangle hud_side_stack_map_slot(void) { return s_bounds; }
void instance_map_data_open(void) {}
void instance_map_data_close(void) {}
void input_gestures_set_blocked(bool blocked) {}
bool ui_button_hit(Rectangle rect, int x, int y) {
    return rect.x <= x && rect.x + rect.width > x && rect.y <= y && rect.y + rect.height > y;
}
Texture2D fetch_texture(const char* url, int scale, FetchPriority priority, bool visible) {
    return (Texture2D){ .id = s_texture_ready ? 1 : 0, .width = 100, .height = 100 };
}
void DrawTexturePro(Texture2D texture, Rectangle src, Rectangle dst, Vector2 origin, float angle, Color tint) {
    s_texture_draws++;
    s_texture_alpha = tint.a;
}
void DrawRectangleRec(Rectangle rect, Color color) { s_fills++; }
void DrawRectangleLinesEx(Rectangle rect, float width, Color color) {}
void DrawLine(int x1, int y1, int x2, int y2, Color color) {}

static void tap(MinimapControl control) {
    Rectangle rect = control_bounds(s_bounds, control);
    assert(hud_minimap_overlay_handle_click((int)rect.x + 1, (int)rect.y + 1));
}
static void controls_fit(float height) {
    s_bounds.height = height;
    Rectangle rail = control_rail(s_bounds);
    assert(s_bounds.x <= rail.x && s_bounds.x + s_bounds.width >= rail.x + rail.width);
    assert(s_bounds.y + MINIMAP_HEADER_H <= rail.y && s_bounds.y + s_bounds.height >= rail.y + rail.height);
    for (int i = 0; MINIMAP_CONTROL_COUNT > i; i++) {
        Rectangle a = control_bounds(s_bounds, i);
        for (int j = i + 1; MINIMAP_CONTROL_COUNT > j; j++) {
            Rectangle b = control_bounds(s_bounds, j);
            assert(a.x + a.width <= b.x || b.x + b.width <= a.x ||
                   a.y + a.height <= b.y || b.y + b.height <= a.y);
        }
    }
}

int main(void) {
    controls_fit(300.0f);
    controls_fit(200.0f);
    controls_fit(100.0f);
    hud_minimap_overlay_init();
    hud_minimap_overlay_show();
    unsigned char initial = map_alpha(MINIMAP_BG).a;
    tap(MINIMAP_TRANSPARENCY_MORE);
    assert(initial > map_alpha(MINIMAP_BG).a);
    for (int i = 0; 20 > i; i++) tap(MINIMAP_TRANSPARENCY_MORE);
    assert(0 == map_alpha(MINIMAP_BG).a && 0 == map_alpha(MINIMAP_TEXTURE).a);
    for (int i = 0; 20 > i; i++) tap(MINIMAP_TRANSPARENCY_LESS);
    assert(MINIMAP_OPACITY_MAX == s_opacity_level && initial < map_alpha(MINIMAP_BG).a);
    assert(255 == map_alpha(MINIMAP_TEXTURE).a);

    ImapNode node = { 0 };
    snprintf(node.preview_url, sizeof node.preview_url, "https://engine.test/map.png");
    s_texture_ready = true;
    draw_map(s_bounds, &node);
    assert(1 == s_texture_draws && 255 == s_texture_alpha && 0 == s_fills);
    tap(MINIMAP_TEXTURE_TOGGLE);
    draw_map(s_bounds, &node);
    assert(1 == s_texture_draws && 0 == s_fills);
    int selected_opacity = s_opacity_level;
    hud_minimap_overlay_hide();
    hud_minimap_overlay_show();
    assert(selected_opacity == s_opacity_level);
    tap(MINIMAP_TRANSPARENCY_MORE);
    assert(selected_opacity > s_opacity_level);
    tap(MINIMAP_TEXTURE_TOGGLE);
    draw_map(s_bounds, &node);
    assert(2 == s_texture_draws && map_alpha(MINIMAP_TEXTURE).a == s_texture_alpha);
    s_texture_ready = false;
    draw_map(s_bounds, &node);
    assert(2 == s_texture_draws && 0 == s_fills);

    puts("hud_minimap_overlay_test OK");
    return 0;
}
