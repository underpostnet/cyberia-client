#include "hud_minimap_overlay.h"

#include "instance_map_data.h"
#include "text.h"
#include "toolbar.h"
#include "ui_button.h"

#include "game_state.h"
#include "input/input.h"

#include <math.h>
#include <raylib.h>
#include <stdio.h>
#include <string.h>

#define MINIMAP_NODE_SIDE_LEVEL_1 100.0f
#define MINIMAP_NODE_SIDE_STEP     50.0f
#define MINIMAP_NODE_GAP           18.0f
#define MINIMAP_ZOOM_MIN            1
#define MINIMAP_ZOOM_DEFAULT        4
#define MINIMAP_ZOOM_MAX            7

static const Color MINIMAP_BG           = { 28, 42, 58, 112 };
static const Color MINIMAP_BORDER       = { 70, 190, 240, 180 };
static const Color MINIMAP_GRID         = { 80, 150, 190, 38 };
static const Color MINIMAP_TEXT         = { 215, 230, 245, 235 };
static const Color MINIMAP_TEXT_DIM     = { 140, 165, 190, 205 };
static const Color MINIMAP_PLAYER       = { 105, 255, 145, 255 };
static const Color MINIMAP_OTHER_PLAYER = { 100, 205, 255, 245 };
static const Color MINIMAP_QUEST        = { 250, 205, 70, 255 };
static const Color MINIMAP_ACTION       = { 90, 230, 235, 255 };

static bool s_visible = false;
static int  s_zoom_level = MINIMAP_ZOOM_DEFAULT;

static float clampf(float value, float low, float high) {
    return value < low ? low : (value > high ? high : value);
}

static Rectangle overlay_bounds(void) {
    return (Rectangle){ (float)GetScreenWidth() - HUD_MINIMAP_OVERLAY_SIZE,
                        toolbar_height(),
                        HUD_MINIMAP_OVERLAY_SIZE, HUD_MINIMAP_OVERLAY_SIZE };
}

static Vector2 overlay_center(Rectangle bounds) {
    return (Vector2){ bounds.x + bounds.width * 0.5f,
                      bounds.y + bounds.height * 0.5f };
}

static Rectangle header_button_bounds(Rectangle bounds, int index_from_right) {
    float side = 24.0f;
    float gap = 4.0f;
    return (Rectangle){ bounds.x + bounds.width - side - 4.0f -
                            (float)index_from_right * (side + gap),
                        bounds.y + 2.0f, side, side };
}

static Rectangle close_bounds(Rectangle bounds) {
    return header_button_bounds(bounds, 0);
}

static Rectangle zoom_in_bounds(Rectangle bounds) {
    return header_button_bounds(bounds, 1);
}

static Rectangle zoom_out_bounds(Rectangle bounds) {
    return header_button_bounds(bounds, 2);
}

static float node_side(void) {
    return MINIMAP_NODE_SIDE_LEVEL_1 +
           (float)(s_zoom_level - MINIMAP_ZOOM_MIN) * MINIMAP_NODE_SIDE_STEP;
}

static Rectangle centered_square(Vector2 at, float side) {
    return (Rectangle){ floorf(at.x - side * 0.5f), floorf(at.y - side * 0.5f),
                        side, side };
}

static void draw_square_marker(Vector2 at, float side, Color color) {
    DrawRectangleRec(centered_square(at, side + 4.0f), (Color){ 3, 7, 16, 210 });
    DrawRectangleRec(centered_square(at, side), color);
}

static int current_node_index(const ImapGraph* graph) {
    int node = instance_map_data_find_node(g_game_state.player.map_code);
    if (0 <= node) return node;
    return 0 < graph->node_count ? 0 : -1;
}

static Rectangle node_rect(const ImapGraph* graph, int node_index,
                           int current_index, Rectangle bounds) {
    const ImapNode* node = &graph->nodes[node_index];
    const ImapNode* current = &graph->nodes[current_index];
    float grid_x = 0 < current->grid_x ? (float)current->grid_x : 1.0f;
    float grid_y = 0 < current->grid_y ? (float)current->grid_y : 1.0f;
    float player_x = g_game_state.player.base.interp_pos.x +
                     g_game_state.player.base.dims.x * 0.5f;
    float player_y = g_game_state.player.base.interp_pos.y +
                     g_game_state.player.base.dims.y * 0.5f;
    float player_fx = clampf(player_x / grid_x, 0.0f, 1.0f);
    float player_fy = clampf(player_y / grid_y, 0.0f, 1.0f);
    Vector2 center = overlay_center(bounds);
    float side = node_side();
    float stride = side + MINIMAP_NODE_GAP;
    float x = center.x - player_fx * side +
              (float)(node->grid_col - current->grid_col) * stride;
    float y = center.y - player_fy * side +
              (float)(node->grid_row - current->grid_row) * stride;
    return (Rectangle){ x, y, side, side };
}

static Vector2 cell_position(const ImapNode* node, Rectangle rect,
                             float cell_x, float cell_y) {
    float fx = 0 < node->grid_x ? cell_x / (float)node->grid_x : 0.5f;
    float fy = 0 < node->grid_y ? cell_y / (float)node->grid_y : 0.5f;
    return (Vector2){ rect.x + clampf(fx, 0.0f, 1.0f) * rect.width,
                      rect.y + clampf(fy, 0.0f, 1.0f) * rect.height };
}

static Color presence_color(ImapPresenceStatus status) {
    switch (status) {
        case IMAP_PRESENCE_HOSTILE:       return (Color){ 225, 75, 70, 245 };
        case IMAP_PRESENCE_RESOURCE:      return (Color){ 105, 205, 105, 245 };
        case IMAP_PRESENCE_PORTAL:        return (Color){ 70, 200, 255, 245 };
        case IMAP_PRESENCE_PORTAL_RANDOM: return (Color){ 180, 110, 255, 245 };
        case IMAP_PRESENCE_PASSIVE:       return (Color){ 215, 225, 235, 235 };
        case IMAP_PRESENCE_NONE:
        default:                          return MINIMAP_TEXT_DIM;
    }
}

static void draw_node(Rectangle rect, const ImapNode* node, bool current) {
    Color line = current ? MINIMAP_PLAYER : MINIMAP_BORDER;
    line.a = current ? 190 : 105;
    DrawRectangleLinesEx(rect, current ? 2.0f : 1.0f, line);

    for (int i = 1; i < 4; ++i) {
        float x = rect.x + rect.width * (float)i / 4.0f;
        float y = rect.y + rect.height * (float)i / 4.0f;
        DrawLine((int)x, (int)rect.y, (int)x, (int)(rect.y + rect.height), MINIMAP_GRID);
        DrawLine((int)rect.x, (int)y, (int)(rect.x + rect.width), (int)y, MINIMAP_GRID);
    }

    const char* name = '\0' != node->name[0] ? node->name : node->map_code;
    int max_chars = 17;
    char label[20];
    snprintf(label, sizeof(label), "%.*s", max_chars, name);
    DrawText(label, (int)rect.x + 5, (int)rect.y + 3, 10,
             current ? MINIMAP_PLAYER : MINIMAP_TEXT_DIM);
}

static Vector2 edge_endpoint(const ImapGraph* graph, int node_index,
                             int current_index, Rectangle bounds,
                             int cell_x, int cell_y) {
    const ImapNode* node = &graph->nodes[node_index];
    Rectangle rect = node_rect(graph, node_index, current_index, bounds);
    if (0 > cell_x || 0 > cell_y)
        return (Vector2){ rect.x + rect.width * 0.5f, rect.y + rect.height * 0.5f };
    return cell_position(node, rect, (float)cell_x + 0.5f, (float)cell_y + 0.5f);
}

static void draw_edges(const ImapGraph* graph, int current_index, Rectangle bounds) {
    for (int i = 0; i < graph->edge_count; ++i) {
        const ImapEdge* edge = &graph->edges[i];
        Vector2 source = edge_endpoint(graph, edge->source_node, current_index, bounds,
                                       edge->source_cell_x, edge->source_cell_y);
        Vector2 target = edge_endpoint(graph, edge->target_node, current_index, bounds,
                                       edge->target_cell_x, edge->target_cell_y);
        Color color = edge->intra ? (Color){ 180, 110, 255, 145 }
                                  : (Color){ 70, 200, 255, 145 };
        DrawLineEx(source, target, 2.0f, color);
    }
}

static void draw_static_pois(const ImapGraph* graph, int current_index, Rectangle bounds) {
    double time = GetTime();
    for (int i = 0; i < graph->presence_poi_count; ++i) {
        const ImapPresencePoi* poi = &graph->presence_pois[i];
        if (0 > poi->node || graph->node_count <= poi->node) continue;
        const ImapNode* node = &graph->nodes[poi->node];
        Rectangle rect = node_rect(graph, poi->node, current_index, bounds);
        Vector2 at = cell_position(node, rect, (float)poi->cell_x + 0.5f,
                                   (float)poi->cell_y + 0.5f);
        Color color = presence_color(poi->presence_status);
        float side = poi->node == current_index ? 8.0f : 6.0f;
        draw_square_marker(at, side, color);
        if (poi->action_active || poi->quest_active) {
            Color active = poi->quest_active ? MINIMAP_QUEST : MINIMAP_ACTION;
            float pulse_side = 13.0f + 3.0f * sinf((float)time * 4.0f + (float)i);
            DrawRectangleLinesEx(centered_square(at, pulse_side), 1.0f, active);
        } else if (poi->capabilities & IMAP_CAPABILITY_QUEST) {
            DrawRectangleLinesEx(centered_square(at, side + 4.0f), 1.0f, MINIMAP_QUEST);
        } else if (poi->capabilities & IMAP_CAPABILITY_ACTION) {
            DrawRectangleLinesEx(centered_square(at, side + 4.0f), 1.0f, MINIMAP_ACTION);
        }
    }
}

static Rectangle current_map_rect(const ImapGraph* graph, int current_index,
                                  Rectangle bounds) {
    return node_rect(graph, current_index, current_index, bounds);
}

static Vector2 current_map_position(const ImapNode* node, Rectangle rect,
                                    Vector2 pos, Vector2 dims) {
    return cell_position(node, rect, pos.x + dims.x * 0.5f, pos.y + dims.y * 0.5f);
}

static void draw_current_portals(const ImapGraph* graph, int current_index,
                                 Rectangle bounds) {
    const ImapNode* node = &graph->nodes[current_index];
    Rectangle rect = current_map_rect(graph, current_index, bounds);
    for (int i = 0; i < g_game_state.portal_count; ++i) {
        const WorldObject* portal = &g_game_state.portals[i];
        Vector2 at = current_map_position(node, rect, portal->pos, portal->dims);
        draw_square_marker(at, 7.0f, (Color){ 175, 110, 255, 240 });
    }
}

static void draw_live_presence(const ImapGraph* graph, int current_index,
                               Rectangle bounds) {
    const ImapNode* node = &graph->nodes[current_index];
    Rectangle rect = current_map_rect(graph, current_index, bounds);

    for (int i = 0; i < g_game_state.other_player_count; ++i) {
        const PlayerState* player = &g_game_state.other_players[i];
        if (0 != strcmp(player->map_code, node->map_code)) continue;
        Vector2 at = current_map_position(node, rect, player->base.interp_pos, player->base.dims);
        draw_square_marker(at, 7.0f, MINIMAP_OTHER_PLAYER);
    }
    for (int i = 0; i < g_game_state.bot_count; ++i) {
        const BotState* bot = &g_game_state.bots[i];
        Vector2 at = current_map_position(node, rect, bot->base.interp_pos, bot->base.dims);
        Color color = STATUS_ICON_HOSTILE == bot->base.status_icon
                          ? (Color){ 235, 75, 70, 245 }
                          : (Color){ 220, 225, 235, 230 };
        draw_square_marker(at, 5.0f, color);
    }
    for (int i = 0; i < g_game_state.resource_count; ++i) {
        const BotState* resource = &g_game_state.resources[i];
        Vector2 at = current_map_position(node, rect, resource->base.interp_pos,
                                          resource->base.dims);
        draw_square_marker(at, 5.0f, (Color){ 105, 205, 105, 235 });
    }
}

static void draw_player(Rectangle bounds) {
    Vector2 center = overlay_center(bounds);
    float pulse_side = 15.0f + 3.0f * sinf((float)GetTime() * 4.5f);
    draw_square_marker(center, 7.0f, MINIMAP_PLAYER);
    DrawRectangleLinesEx(centered_square(center, pulse_side), 1.0f, MINIMAP_PLAYER);
    DrawLine((int)center.x - 10, (int)center.y, (int)center.x - 6, (int)center.y,
             MINIMAP_PLAYER);
    DrawLine((int)center.x + 6, (int)center.y, (int)center.x + 10, (int)center.y,
             MINIMAP_PLAYER);
}

static void draw_chrome(Rectangle bounds, const char* map_name) {
    DrawRectangle((int)bounds.x, (int)bounds.y, (int)bounds.width, 28,
                  (Color){ 3, 7, 16, 148 });
    char title[48];
    snprintf(title, sizeof(title), "MINIMAP  %.*s", 18, map_name);
    DrawText(title, (int)bounds.x + 7, (int)bounds.y + 8, 11, MINIMAP_TEXT);
    Vector2 mouse = GetMousePosition();
    Rectangle zoom_out = zoom_out_bounds(bounds);
    Rectangle zoom_in = zoom_in_bounds(bounds);
    UIButtonStyle zoom_out_style = { .text = "-", .font_size = 18, .no_fill = true };
    UIButtonStyle zoom_in_style = { .text = "+", .font_size = 18, .no_fill = true };
    ui_button_draw(zoom_out, &zoom_out_style,
                   ui_button_resolve_state(MINIMAP_ZOOM_MIN < s_zoom_level, false,
                                           CheckCollisionPointRec(mouse, zoom_out)));
    ui_button_draw(zoom_in, &zoom_in_style,
                   ui_button_resolve_state(MINIMAP_ZOOM_MAX > s_zoom_level, false,
                                           CheckCollisionPointRec(mouse, zoom_in)));
    UIButtonStyle close_style = { .icon_id = "close-yellow", .no_fill = true };
    ui_button_draw(close_bounds(bounds), &close_style, UI_BUTTON_NORMAL);

    char coords[40];
    snprintf(coords, sizeof(coords), "%.0f, %.0f",
             g_game_state.player.base.interp_pos.x,
             g_game_state.player.base.interp_pos.y);
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
}

void hud_minimap_overlay_cleanup(void) {
    hud_minimap_overlay_hide();
}

void hud_minimap_overlay_show(void) {
    if (s_visible) return;
    s_visible = true;
    s_zoom_level = MINIMAP_ZOOM_DEFAULT;
    instance_map_data_open();
    input_gestures_set_blocked(false);
}

void hud_minimap_overlay_hide(void) {
    if (!s_visible) return;
    s_visible = false;
    instance_map_data_close();
    input_gestures_set_blocked(false);
}

void hud_minimap_overlay_toggle(void) {
    if (s_visible) hud_minimap_overlay_hide();
    else           hud_minimap_overlay_show();
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
    DrawRectangleRec(bounds, MINIMAP_BG);
    BeginScissorMode((int)bounds.x, (int)bounds.y, (int)bounds.width, (int)bounds.height);

    const ImapGraph* graph = instance_map_data_graph();
    int current_index = current_node_index(graph);
    const char* map_name = g_game_state.player.map_code;
    if (IMAP_DATA_READY == instance_map_data_state() && 0 <= current_index) {
        map_name = '\0' != graph->nodes[current_index].name[0]
                       ? graph->nodes[current_index].name
                       : graph->nodes[current_index].map_code;
        for (int i = 0; i < graph->node_count; ++i) {
            Rectangle rect = node_rect(graph, i, current_index, bounds);
            draw_node(rect, &graph->nodes[i], i == current_index);
        }
        draw_edges(graph, current_index, bounds);
        draw_current_portals(graph, current_index, bounds);
        draw_static_pois(graph, current_index, bounds);
        draw_live_presence(graph, current_index, bounds);
    } else {
        const char* status = IMAP_DATA_ERROR == instance_map_data_state()
                                 ? "MAP DATA UNAVAILABLE"
                                 : "LOADING MAP DATA...";
        int width = MeasureText(status, 12);
        DrawText(status, (int)(bounds.x + (bounds.width - (float)width) * 0.5f),
                 (int)(bounds.y + bounds.height * 0.5f + 16.0f), 12, MINIMAP_TEXT_DIM);
    }

    draw_player(bounds);
    EndScissorMode();
    draw_chrome(bounds, map_name);
}

bool hud_minimap_overlay_handle_click(int mx, int my) {
    if (!s_visible) return false;
    Rectangle bounds = overlay_bounds();
    if (ui_button_hit(close_bounds(bounds), mx, my)) {
        hud_minimap_overlay_hide();
        return true;
    }
    if (ui_button_hit(zoom_in_bounds(bounds), mx, my)) {
        if (MINIMAP_ZOOM_MAX > s_zoom_level) s_zoom_level++;
        return true;
    }
    if (ui_button_hit(zoom_out_bounds(bounds), mx, my)) {
        if (MINIMAP_ZOOM_MIN < s_zoom_level) s_zoom_level--;
        return true;
    }
    return false;
}
