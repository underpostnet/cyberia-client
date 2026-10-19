#include "hud_minimap_overlay.h"
#include "modal.h"

#include "hud_side_stack.h"
#include "data/instance_map_data.h"
#include "data/engine_client.h"
#include "text.h"
#include "ui_button.h"
#include "ui_rect.h"

#include "sim/local_player.h"
#include "sim/game_state.h"
#include "input/input.h"
#include "view/local_player_view.h"

#include <math.h>
#include <raylib.h>
#include <raymath.h>
#include <stdio.h>
#include <string.h>

#define MINIMAP_HEADER_H           36.0f
#define MINIMAP_ZOOM_MIN            1
#define MINIMAP_ZOOM_DEFAULT        1
#define MINIMAP_ZOOM_MAX            4
#define MINIMAP_OPACITY_MIN         0
#define MINIMAP_OPACITY_DEFAULT     2
#define MINIMAP_OPACITY_MAX        10
#define MINIMAP_CONTROL_SIDE       28.0f
#define MINIMAP_CONTROL_GAP         4.0f
#define MINIMAP_CONTROL_INSET       6.0f
#define MINIMAP_LABEL_FONT         11
#define MINIMAP_LABEL_MAX_W       120.0f
#define MINIMAP_MARKER_ROOM        22.0f   /* what a label keeps clear of a marker */

/* The map body stays transparent; controls and markers keep their contrast. */
static const Color MINIMAP_BG           = { 8, 14, 24, 48 };
static const Color MINIMAP_HEADER       = { 3, 7, 16, 160 };
static const Color MINIMAP_TEXTURE      = { 255, 255, 255, 64 };
static const Color MINIMAP_EDGE         = { 150, 200, 235, 150 };
static const Color MINIMAP_GRID         = { 80, 150, 190, 38 };
static const Color MINIMAP_OUTLINE      = { 3, 7, 16, 230 };
static const Color MINIMAP_RAIL         = { 3, 7, 16, 190 };
static const Color MINIMAP_TEXT         = { 215, 230, 245, 235 };
static const Color MINIMAP_TEXT_DIM     = { 140, 165, 190, 205 };
static const Color MINIMAP_OFF          = { 255, 110, 90, 235 };
static const Color MINIMAP_PLAYER       = { 105, 255, 145, 255 };
static const Color MINIMAP_OTHER_PLAYER = { 100, 205, 255, 245 };
static const Color MINIMAP_EXIT         = { 60, 200, 255, 255 };
static const Color MINIMAP_LINK         = { 170, 110, 255, 255 };

/* The controls along the right edge of the map, top to bottom. */
typedef enum {
    MINIMAP_TEXTURE_TOGGLE, MINIMAP_ZOOM_IN, MINIMAP_ZOOM_OUT,
    MINIMAP_TRANSPARENCY_MORE, MINIMAP_TRANSPARENCY_LESS, MINIMAP_CONTROL_COUNT,
} MinimapControl;

static bool s_visible = false;
static bool s_texture_visible = true;
static int  s_zoom_level = MINIMAP_ZOOM_DEFAULT;
static int  s_opacity_level = MINIMAP_OPACITY_DEFAULT;
static char s_exit_hint[IMAP_NAME_MAX * 2 + 16]; /* the exit under the pointer, this frame */

static Rectangle overlay_bounds(void) {
    return hud_side_stack_map_slot();
}

static Rectangle close_bounds(Rectangle bounds) {
    float side = 32.0f;
    return (Rectangle){ bounds.x + bounds.width - side - 4.0f, bounds.y + 2.0f, side, side };
}

/* A column of controls in the top right corner of the map; a row when the map
 * is too short for a column. */
static Rectangle control_bounds(Rectangle bounds, MinimapControl control) {
    float body = bounds.height - MINIMAP_HEADER_H;
    float column = (float)MINIMAP_CONTROL_COUNT * (MINIMAP_CONTROL_SIDE + MINIMAP_CONTROL_GAP) + MINIMAP_CONTROL_INSET;
    float step = (float)control * (MINIMAP_CONTROL_SIDE + MINIMAP_CONTROL_GAP);
    float right = bounds.x + bounds.width - MINIMAP_CONTROL_INSET - MINIMAP_CONTROL_SIDE;
    float top = bounds.y + MINIMAP_HEADER_H + MINIMAP_CONTROL_INSET;
    return column <= body
        ? (Rectangle){ right, top + step, MINIMAP_CONTROL_SIDE, MINIMAP_CONTROL_SIDE }
        : (Rectangle){ right - (float)(MINIMAP_CONTROL_COUNT - 1) * (MINIMAP_CONTROL_SIDE + MINIMAP_CONTROL_GAP) + step,
                       top, MINIMAP_CONTROL_SIDE, MINIMAP_CONTROL_SIDE };
}

/* The backing under the controls, so they read on any texture. */
static Rectangle control_rail(Rectangle bounds) {
    Rectangle first = control_bounds(bounds, MINIMAP_TEXTURE_TOGGLE);
    Rectangle last = control_bounds(bounds, MINIMAP_CONTROL_COUNT - 1);
    return (Rectangle){ first.x - 3.0f, first.y - 3.0f, last.x + last.width - first.x + 6.0f,
                        last.y + last.height - first.y + 6.0f };
}

static Color map_alpha(Color color) {
    int alpha = (int)color.a * s_opacity_level / MINIMAP_OPACITY_DEFAULT;
    color.a = (unsigned char)(255 < alpha ? 255 : alpha);
    return color;
}

static Rectangle centered_square(Vector2 at, float side) {
    return (Rectangle){ floorf(at.x - side * 0.5f), floorf(at.y - side * 0.5f),
                        side, side };
}

static void draw_square_marker(Vector2 at, float side, Color color) {
    DrawRectangleRec(centered_square(at, side + 4.0f), MINIMAP_OUTLINE);
    DrawRectangleRec(centered_square(at, side), color);
}

/* A missing or hidden texture leaves the grid on the container background. */
static void draw_map(Rectangle rect, const ImapNode* node) {
    Texture2D texture = s_texture_visible && '\0' != node->preview_url[0]
        ? fetch_texture(node->preview_url, 1, FETCH_P1, true) : (Texture2D){0};
    if (0 < texture.id) {
        DrawTexturePro(texture, (Rectangle){0, 0, (float)texture.width, (float)texture.height},
                       rect, (Vector2){0}, 0.0f, map_alpha(MINIMAP_TEXTURE));
    } else {
        for (int i = 1; 4 > i; i++) {
            float x = rect.x + rect.width * (float)i / 4.0f;
            float y = rect.y + rect.height * (float)i / 4.0f;
            DrawLine((int)x, (int)rect.y, (int)x, (int)(rect.y + rect.height), MINIMAP_GRID);
            DrawLine((int)rect.x, (int)y, (int)(rect.x + rect.width), (int)y, MINIMAP_GRID);
        }
    }
    DrawRectangleLinesEx(rect, 1.0f, MINIMAP_EDGE);
}

/* Authored presence draws hollow under the filled live markers. Portals draw
 * as exits. */
static void draw_static_pois(const ImapGraph* graph, int current_index, Rectangle rect) {
    double time = GetTime();
    for (int i = 0; i < graph->presence_poi_count; ++i) {
        const ImapPresencePoi* poi = &graph->presence_pois[i];
        if (poi->node != current_index || IMAP_PRESENCE_PORTAL == poi->presence_status ||
            IMAP_PRESENCE_PORTAL_RANDOM == poi->presence_status) continue;
        const ImapNode* node = &graph->nodes[poi->node];
        Vector2 at = instance_map_cell_to_card(rect, node, (float)poi->cell_x + 0.5f,
                                   (float)poi->cell_y + 0.5f);
        Color color = instance_map_presence_color(poi->presence_status);
        float side = 7.0f;
        DrawRectangleLinesEx(centered_square(at, side + 2.0f), 3.0f, MINIMAP_OUTLINE);
        DrawRectangleLinesEx(centered_square(at, side), 1.0f, color);
        if (poi->action_active || poi->quest_active) {
            Color active = poi->quest_active ? MODAL_QUEST : MODAL_ACTION;
            float pulse_side = 13.0f + 3.0f * sinf((float)time * 4.0f + (float)i);
            DrawRectangleLinesEx(centered_square(at, pulse_side), 1.0f, active);
        } else if (poi->capabilities & IMAP_CAPABILITY_QUEST) {
            DrawRectangleLinesEx(centered_square(at, side + 4.0f), 1.0f, MODAL_QUEST);
        } else if (poi->capabilities & IMAP_CAPABILITY_ACTION) {
            DrawRectangleLinesEx(centered_square(at, side + 4.0f), 1.0f, MODAL_ACTION);
        }
    }
}

/* A triangle at `at`, `size` long, pointing along unit `dir`. */
static void draw_arrow(Vector2 at, Vector2 dir, float size, Color color) {
    Vector2 side = { -dir.y, dir.x };
    Vector2 tip = Vector2Add(at, Vector2Scale(dir, size));
    Vector2 a = Vector2Add(at, Vector2Scale(side, size * 0.6f));
    Vector2 b = Vector2Subtract(at, Vector2Scale(side, size * 0.6f));
    /* raylib fills one winding; both orders cover either direction. */
    DrawTriangle(tip, a, b, color);
    DrawTriangle(tip, b, a, color);
}

/* `text` cut with an ellipsis to `max_w` pixels. */
static void fit_text(char* out, size_t size, const char* text, int font, float max_w) {
    snprintf(out, size, "%s", text);
    int length = (int)strlen(out);
    if ((float)MeasureText(out, font) <= max_w) return;
    while (3 < length && (float)MeasureText(out, font) > max_w) {
        length = text_utf8_floor(out, length - 1);
        out[length] = '\0';
    }
    if (3 <= length) memcpy(out + text_utf8_floor(out, length - 3), "...", 4);
}

static bool clear_of(Rectangle box, const Rectangle* taken, int count) {
    for (int i = 0; count > i; i++)
        if (CheckCollisionRecs(box, taken[i])) return false;
    return true;
}

/* Each portal of the current map: a gate where it stands, a link inside the
 * map with a ring, an exit to another map with an arrow toward it and a tag
 * naming it. A tag takes the first side of its gate (below, above, right,
 * left) clear of the controls, the markers and the tags before it. The exit
 * under the pointer goes to s_exit_hint in full. */
static void draw_exits(const ImapGraph* graph, int current_index, Rectangle rect, Rectangle view, Vector2 player,
                       Rectangle controls) {
    const ImapNode* node = &graph->nodes[current_index];
    Vector2 gates[IMAP_MAX_EDGES];
    int edges[IMAP_MAX_EDGES], gate_count = 0;
    Rectangle taken[IMAP_MAX_EDGES * 2 + 2];
    int taken_count = 0;
    taken[taken_count++] = controls;
    taken[taken_count++] = centered_square(player, MINIMAP_MARKER_ROOM);
    for (int i = 0; graph->edge_count > i; ++i) {
        const ImapEdge* edge = &graph->edges[i];
        if (edge->source_node != current_index || 0 > edge->source_cell_x || 0 > edge->source_cell_y) continue;
        Vector2 at = instance_map_cell_to_card(rect, node, (float)edge->source_cell_x + 0.5f,
                                               (float)edge->source_cell_y + 0.5f);
        Color color = edge->intra ? MINIMAP_LINK : MINIMAP_EXIT;
        if (edge->intra) {
            DrawCircleLinesV(at, 11.0f, MINIMAP_OUTLINE);
            DrawCircleLinesV(at, 10.0f, color);
        } else {
            Vector2 dir = instance_map_exit_direction(graph, edge);
            draw_arrow(Vector2Add(at, Vector2Scale(dir, 8.0f)), dir, 10.0f, MINIMAP_OUTLINE);
            draw_arrow(Vector2Add(at, Vector2Scale(dir, 9.0f)), dir, 7.0f, color);
        }
        DrawPoly(at, 4, 10.0f, 0.0f, MINIMAP_OUTLINE);
        DrawPoly(at, 4, 8.0f, 0.0f, color);
        DrawPoly(at, 4, 3.0f, 0.0f, WHITE);
        gates[gate_count] = at;
        edges[gate_count++] = i;
        taken[taken_count++] = centered_square(at, MINIMAP_MARKER_ROOM);
    }

    s_exit_hint[0] = '\0';
    float height = (float)text_line_height(MINIMAP_LABEL_FONT) + 4.0f;
    for (int g = 0; gate_count > g; ++g) {
        const ImapEdge* edge = &graph->edges[edges[g]];
        Vector2 at = gates[g];
        if (edge->intra || !CheckCollisionPointRec(at, view)) continue;
        char name[IMAP_NAME_MAX * 2];
        instance_map_node_label(graph, edge->target_node, name, sizeof name);
        char label[IMAP_NAME_MAX * 2];
        fit_text(label, sizeof label, name, MINIMAP_LABEL_FONT, fminf(MINIMAP_LABEL_MAX_W, view.width - 16.0f));
        float width = (float)MeasureText(label, MINIMAP_LABEL_FONT) + 8.0f;
        Vector2 offsets[4] = {
            { -width * 0.5f, 14.0f }, { -width * 0.5f, -14.0f - height },
            { 14.0f, -height * 0.5f }, { -14.0f - width, -height * 0.5f },
        };
        bool placed = false;
        for (int side = 0; 4 > side && !placed; ++side) {
            Rectangle box = { floorf(at.x + offsets[side].x), floorf(at.y + offsets[side].y), width, height };
            box.x = Clamp(box.x, view.x + 2.0f, view.x + view.width - width - 2.0f);
            box.y = Clamp(box.y, view.y + 2.0f, view.y + view.height - height - 2.0f);
            if (!clear_of(box, taken, taken_count)) continue;
            taken[taken_count++] = box;
            DrawRectangleRec(box, MINIMAP_OUTLINE);
            DrawRectangleLinesEx(box, 1.0f, Fade(MINIMAP_EXIT, 0.55f));
            DrawText(label, (int)box.x + 4, (int)box.y + 2, MINIMAP_LABEL_FONT, MINIMAP_EXIT);
            placed = true;
        }
        if (CheckCollisionPointRec(GetMousePosition(), centered_square(at, MINIMAP_MARKER_ROOM)))
            snprintf(s_exit_hint, sizeof s_exit_hint, "EXIT TO %s", name);
    }
}

/* The full name of the exit under the pointer, over every marker. */
static void draw_exit_hint(Rectangle view) {
    if ('\0' == s_exit_hint[0]) return;
    float height = (float)text_line_height(MINIMAP_LABEL_FONT) + 8.0f;
    Rectangle strip = { view.x, view.y + view.height - height - 18.0f, view.width, height };
    DrawRectangleRec(strip, MINIMAP_OUTLINE);
    modal_draw_clipped_text(s_exit_hint, (int)strip.x + 6, (int)strip.y + 4, (int)strip.width - 12,
                            MINIMAP_LABEL_FONT, MINIMAP_TEXT);
}

/* An entity at its drawn position on the current map. */
static Vector2 drawn_map_position(const ImapNode* node, Rectangle rect, EntityID id) {
    Vector2 pos = game_state_render(id)->interp_pos;
    Vector2 size = game_state_transform(id)->size;
    return instance_map_cell_to_card(rect, node, pos.x + size.x * 0.5f, pos.y + size.y * 0.5f);
}

static void draw_live_presence(const ImapGraph* graph, int current_index,
                               Rectangle rect) {
    const ImapNode* node = &graph->nodes[current_index];

    for (int i = 0; i < g_game_state.other_player_count; ++i) {
        Vector2 at = drawn_map_position(node, rect, g_game_state.other_players[i]);
        draw_square_marker(at, 7.0f, MINIMAP_OTHER_PLAYER);
    }
    for (int i = 0; i < g_game_state.bot_count; ++i) {
        EntityID bot = g_game_state.bots[i];
        Vector2 at = drawn_map_position(node, rect, bot);
        draw_square_marker(at, 5.0f, instance_map_presence_color(
            STATUS_ICON_HOSTILE == game_state_data(bot)->status_icon ? IMAP_PRESENCE_HOSTILE
                                                         : IMAP_PRESENCE_PASSIVE));
    }
    for (int i = 0; i < g_game_state.resource_count; ++i) {
        Vector2 at = drawn_map_position(node, rect, g_game_state.resources[i]);
        draw_square_marker(at, 5.0f,
                           instance_map_presence_color(IMAP_PRESENCE_RESOURCE));
    }
}

/* The own player, its facing as a tick ahead of the marker. */
static void draw_player(Vector2 center) {
    float pulse_side = 15.0f + 3.0f * sinf((float)GetTime() * 4.5f);
    DrawRectangleLinesEx(centered_square(center, pulse_side + 2.0f), 1.0f, MINIMAP_OUTLINE);
    DrawRectangleLinesEx(centered_square(center, pulse_side), 1.0f, MINIMAP_PLAYER);
    draw_square_marker(center, 7.0f, MINIMAP_PLAYER);
    float angle = (float)local_player_view_direction() * PI * 0.25f;
    Vector2 dir = { sinf(angle), -cosf(angle) };
    draw_arrow(Vector2Add(center, Vector2Scale(dir, 11.0f)), dir, 7.0f, MINIMAP_OUTLINE);
    draw_arrow(Vector2Add(center, Vector2Scale(dir, 12.0f)), dir, 5.0f, MINIMAP_PLAYER);
}

static void draw_control_tip(Rectangle bounds, Rectangle control, const char* tip) {
    float width = (float)MeasureText(tip, MINIMAP_LABEL_FONT) + 8.0f;
    float x = fmaxf(bounds.x + 4.0f, control.x - width - 6.0f);
    Rectangle box = { x, control.y + 6.0f, width, (float)text_line_height(MINIMAP_LABEL_FONT) + 4.0f };
    DrawRectangleRec(box, MINIMAP_OUTLINE);
    DrawText(tip, (int)box.x + 4, (int)box.y + 2, MINIMAP_LABEL_FONT, MINIMAP_TEXT);
}

static void draw_controls(Rectangle bounds, Vector2 mouse) {
    DrawRectangleRec(control_rail(bounds), MINIMAP_RAIL);
    Rectangle texture = control_bounds(bounds, MINIMAP_TEXTURE_TOGGLE);
    bool over_texture = CheckCollisionPointRec(mouse, texture);
    UIButtonStyle toggle = { .icon_id = "map", .icon_size = (int)(texture.width * 0.6f), .border = MINIMAP_EDGE };
    ui_button_draw(texture, &toggle, ui_button_resolve_state(true, s_texture_visible, over_texture));
    if (!s_texture_visible) {
        DrawRectangleRec(ui_rect_inset(texture, 1.0f), Fade(MINIMAP_OUTLINE, 0.55f));
        DrawLineEx((Vector2){ texture.x + 5.0f, texture.y + texture.height - 5.0f },
                   (Vector2){ texture.x + texture.width - 5.0f, texture.y + 5.0f }, 2.0f, MINIMAP_OFF);
    }
    const struct {
        const char* label;
        const char* tip;
        bool enabled;
    } controls[] = {
        { "+", "ZOOM IN", MINIMAP_ZOOM_MAX > s_zoom_level },
        { "-", "ZOOM OUT", MINIMAP_ZOOM_MIN < s_zoom_level },
        { "T+", "MORE TRANSPARENT", MINIMAP_OPACITY_MIN < s_opacity_level },
        { "T-", "LESS TRANSPARENT", MINIMAP_OPACITY_MAX > s_opacity_level },
    };
    for (int i = MINIMAP_ZOOM_IN; MINIMAP_CONTROL_COUNT > i; i++) {
        Rectangle rect = control_bounds(bounds, i);
        bool hovered = CheckCollisionPointRec(mouse, rect);
        UIButtonStyle style = { .text = controls[i - MINIMAP_ZOOM_IN].label,
                                .font_size = MINIMAP_ZOOM_OUT >= i ? 18 : 14, .no_fill = true };
        ui_button_draw(rect, &style, ui_button_resolve_state(controls[i - MINIMAP_ZOOM_IN].enabled, false, hovered));
    }
    if (over_texture) draw_control_tip(bounds, texture, s_texture_visible ? "MAP TEXTURE ON" : "MAP TEXTURE OFF");
    for (int i = MINIMAP_ZOOM_IN; MINIMAP_CONTROL_COUNT > i; i++) {
        Rectangle rect = control_bounds(bounds, i);
        if (CheckCollisionPointRec(mouse, rect)) draw_control_tip(bounds, rect, controls[i - MINIMAP_ZOOM_IN].tip);
    }
}

static void draw_chrome(Rectangle bounds, const char* map_name) {
    DrawRectangle((int)bounds.x, (int)bounds.y, (int)bounds.width, (int)MINIMAP_HEADER_H, map_alpha(MINIMAP_HEADER));
    char title[IMAP_NAME_MAX];
    snprintf(title, sizeof title, "%s", map_name);
    int limit = (int)(close_bounds(bounds).x - bounds.x - 14.0f);
    int length = (int)strlen(title);
    while (0 < length && MeasureText(title, 12) > limit) {
        length = text_utf8_floor(title, length - 1);
        title[length] = '\0';
    }
    DrawText(title, (int)bounds.x + 7, (int)bounds.y + 12, 12, MINIMAP_TEXT);
    UIButtonStyle close_style = { .icon_id = "close-yellow", .no_fill = true };
    ui_button_draw(close_bounds(bounds), &close_style, UI_BUTTON_NORMAL);

    char coords[40];
    snprintf(coords, sizeof(coords), "%.0f, %.0f",
             game_state_own_render()->interp_pos.x,
             game_state_own_render()->interp_pos.y);
    int width = MeasureText(coords, 10);
    DrawRectangle((int)(bounds.x + bounds.width - width - 10),
                  (int)(bounds.y + bounds.height - 16), width + 8, 14,
                  (Color){ 3, 7, 16, 140 });
    DrawText(coords, (int)(bounds.x + bounds.width - width - 6),
             (int)(bounds.y + bounds.height - 14), 10, MINIMAP_TEXT_DIM);
}

void hud_minimap_overlay_init(void) {
    s_visible = false;
    s_zoom_level = MINIMAP_ZOOM_DEFAULT;
    s_opacity_level = MINIMAP_OPACITY_DEFAULT;
}

void hud_minimap_overlay_cleanup(void) {
    hud_minimap_overlay_hide();
}

void hud_minimap_overlay_show(void) {
    if (s_visible) return;
    s_visible = true;
    instance_map_data_open();
    input_gestures_set_blocked(false);
}

void hud_minimap_overlay_hide(void) {
    if (!s_visible) return;
    s_visible = false;
    instance_map_data_close();
    input_gestures_set_blocked(false);
}

bool hud_minimap_overlay_is_visible(void) {
    return s_visible;
}

void hud_minimap_overlay_update(float dt) {
    if (!s_visible) return;
    instance_map_data_update(dt);
}

void hud_minimap_overlay_draw(void) {
    if (!s_visible) return;

    Rectangle bounds = overlay_bounds();
    if (MINIMAP_HEADER_H > bounds.height) return;
    DrawRectangleRec(bounds, map_alpha(MINIMAP_BG));
    Rectangle body = { bounds.x, bounds.y + MINIMAP_HEADER_H, bounds.width, bounds.height - MINIMAP_HEADER_H };

    const ImapGraph* graph = instance_map_data_graph();
    int current_index = instance_map_data_find_node(g_local_player.map_code);
    const char* map_name = g_local_player.map_code;
    if (IMAP_DATA_READY == instance_map_data_state() && 0 <= current_index) {
        const ImapNode* node = &graph->nodes[current_index];
        map_name = '\0' != node->name[0] ? node->name : node->map_code;
        const Vector2 pos = game_state_own_render()->interp_pos;
        const Vector2 size = game_state_own_transform()->size;
        Vector2 player = { pos.x + size.x * 0.5f, pos.y + size.y * 0.5f };
        Rectangle rect = instance_map_local_rect(node, body, player, (float)s_zoom_level);
        Rectangle clip = GetCollisionRec(body, rect);
        Vector2 own = instance_map_cell_to_card(rect, node, player.x, player.y);
        BeginScissorMode((int)clip.x, (int)clip.y, (int)clip.width, (int)clip.height);
        draw_map(rect, node);
        draw_static_pois(graph, current_index, rect);
        draw_live_presence(graph, current_index, rect);
        draw_exits(graph, current_index, rect, clip, own, control_rail(bounds));
        draw_player(own);
        draw_exit_hint(clip);
        EndScissorMode();
    } else {
        const char* status = (IMAP_DATA_ERROR == instance_map_data_state() || IMAP_DATA_READY == instance_map_data_state())
                                 ? "MAP DATA UNAVAILABLE"
                                 : "LOADING MAP DATA...";
        int width = MeasureText(status, 12);
        DrawText(status, (int)(bounds.x + (bounds.width - (float)width) * 0.5f),
                 (int)(bounds.y + bounds.height * 0.5f + 16.0f), 12, MINIMAP_TEXT_DIM);
    }

    draw_controls(bounds, GetMousePosition());
    draw_chrome(bounds, map_name);
}

bool hud_minimap_overlay_handle_click(int mx, int my) {
    if (!s_visible) return false;
    Rectangle bounds = overlay_bounds();
    if (MINIMAP_HEADER_H > bounds.height) return false;
    if (ui_button_hit(close_bounds(bounds), mx, my)) {
        hud_minimap_overlay_hide();
        return true;
    }
    if (ui_button_hit(control_bounds(bounds, MINIMAP_TEXTURE_TOGGLE), mx, my)) {
        s_texture_visible = !s_texture_visible;
        return true;
    }
    if (ui_button_hit(control_bounds(bounds, MINIMAP_ZOOM_IN), mx, my)) {
        if (MINIMAP_ZOOM_MAX > s_zoom_level) s_zoom_level++;
        return true;
    }
    if (ui_button_hit(control_bounds(bounds, MINIMAP_ZOOM_OUT), mx, my)) {
        if (MINIMAP_ZOOM_MIN < s_zoom_level) s_zoom_level--;
        return true;
    }
    if (ui_button_hit(control_bounds(bounds, MINIMAP_TRANSPARENCY_MORE), mx, my)) {
        if (MINIMAP_OPACITY_MIN < s_opacity_level) s_opacity_level--;
        return true;
    }
    if (ui_button_hit(control_bounds(bounds, MINIMAP_TRANSPARENCY_LESS), mx, my)) {
        if (MINIMAP_OPACITY_MAX > s_opacity_level) s_opacity_level++;
        return true;
    }
    return false;
}
