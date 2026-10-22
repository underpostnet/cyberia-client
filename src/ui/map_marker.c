#include "map_marker.h"

#include "modal.h"
#include "text.h"
#include "ui_icon.h"
#include "ui_rect.h"

#include <math.h>
#include <raylib.h>
#include <raymath.h>
#include <stdbool.h>
#include <stdio.h>

const Color MAP_MARKER_PLAYER = { 255, 52, 64, 255 };
const Color MAP_MARKER_SPAWN = { 70, 230, 120, 255 };
const Color MAP_MARKER_ROUTE = { 255, 190, 90, 255 };

static const Color C_OUTLINE = { 3, 6, 12, 235 };
static const Color C_PLATE   = { 12, 18, 34, 245 };

#define LABEL_PAD   4.0f
#define BADGE_FONT 10

static Color faded(Color color, float alpha) { return Fade(color, alpha * color.a / 255.0f); }

/* raylib fills one winding; both orders cover either direction. */
static void fill_triangle(Vector2 a, Vector2 b, Vector2 c, Color color) {
    DrawTriangle(a, b, c, color);
    DrawTriangle(a, c, b, color);
}

void map_marker_draw_arrow(Vector2 at, Vector2 dir, float size, Color color, float alpha) {
    for (int pass = 0; 2 > pass; pass++) {
        float s = 0 == pass ? size + 2.0f : size;
        Vector2 base = 0 == pass ? Vector2Subtract(at, Vector2Scale(dir, 1.5f)) : at;
        Vector2 side = { -dir.y * s * 0.6f, dir.x * s * 0.6f };
        fill_triangle(Vector2Add(base, Vector2Scale(dir, s)), Vector2Add(base, side), Vector2Subtract(base, side),
                      faded(0 == pass ? C_OUTLINE : color, alpha));
    }
}

void map_marker_draw_gate(Vector2 at, float radius, Color color, float alpha) {
    DrawPoly(at, 4, radius + 5.0f, 0.0f, Fade(color, alpha * 0.28f));
    DrawPoly(at, 4, radius + 2.0f, 0.0f, faded(C_OUTLINE, alpha));
    DrawPoly(at, 4, radius, 0.0f, Fade(color, alpha));
    DrawPoly(at, 4, radius * 0.4f, 0.0f, Fade(WHITE, alpha));
}

void map_marker_draw_pulse(Vector2 at, Color color, float alpha) {
    DrawCircleV(at, 4.5f, faded(C_OUTLINE, alpha));
    DrawCircleV(at, 3.0f, Fade(color, alpha));
}

void map_marker_draw_player(Vector2 at, float radius, Vector2 facing, float alpha, double time) {
    float r = fmaxf(MAP_MARKER_PLAYER_MIN_R, radius);
    float wave = 0.5f + 0.5f * sinf((float)time * 4.0f);
    float ring = r + 4.0f + 5.0f * wave;
    DrawRing(at, ring, ring + 2.0f, 0.0f, 360.0f, 32, Fade(MAP_MARKER_PLAYER, alpha * (1.0f - 0.7f * wave)));
    DrawCircleV(at, r + 3.0f, faded(C_OUTLINE, alpha));
    DrawCircleV(at, r + 2.0f, Fade(WHITE, alpha));
    DrawCircleV(at, r, Fade(MAP_MARKER_PLAYER, alpha));
    map_marker_draw_arrow(Vector2Add(at, Vector2Scale(facing, r + 3.0f)), facing, r * 0.8f, WHITE, alpha);
}

/* A count badge from (`left`, `bottom`) up and right; nothing under 2. */
static void draw_count(float left, float bottom, int count, float alpha) {
    if (2 > count) return;
    char text[8];
    snprintf(text, sizeof text, "%d", count);
    Rectangle badge = { floorf(left), floorf(bottom - BADGE_FONT), (float)MeasureText(text, BADGE_FONT) + 4.0f,
                        (float)BADGE_FONT + 3.0f };
    DrawRectangleRec(badge, faded(C_OUTLINE, alpha));
    DrawText(text, (int)badge.x + 2, (int)badge.y + 1, BADGE_FONT, Fade(WHITE, alpha));
}

/* A dark plate with a dark outline and a 1 px `border`. */
static void draw_plate(Rectangle box, Color border, float alpha) {
    DrawRectangleRec(ui_rect_inset(box, -2.0f), faded(C_OUTLINE, alpha));
    DrawRectangleRec(box, faded(C_PLATE, alpha));
    DrawRectangleLinesEx(box, 1.0f, faded(border, alpha));
}

void map_marker_draw_presence(Vector2 at, float side, ImapPresenceStatus status, int count, float alpha) {
    Color color = faded(instance_map_presence_color(status), alpha), outline = faded(C_OUTLINE, alpha);
    float r = floorf(side * 0.5f);
    at = (Vector2){ floorf(at.x), floorf(at.y) };
    switch (status) {
        case IMAP_PRESENCE_HOSTILE:
            for (int pass = 0; 2 > pass; pass++) {
                float s = 0 == pass ? r + 2.5f : r;
                fill_triangle((Vector2){ at.x, at.y - s * 1.15f }, (Vector2){ at.x - s * 1.1f, at.y + s * 0.8f },
                              (Vector2){ at.x + s * 1.1f, at.y + s * 0.8f }, 0 == pass ? outline : color);
            }
            break;
        case IMAP_PRESENCE_RESOURCE: {
            Rectangle box = { at.x - r, at.y - r, 2.0f * r, 2.0f * r };
            DrawRectangleRec(ui_rect_inset(box, -2.0f), outline);
            DrawRectangleRec(box, color);
            break;
        }
        case IMAP_PRESENCE_PASSIVE:
            DrawCircleV(at, r + 2.0f, outline);
            DrawCircleV(at, r, color);
            break;
        default:
            DrawCircleV(at, r * 0.6f + 2.0f, outline);
            DrawCircleV(at, r * 0.6f, color);
            break;
    }
    draw_count(at.x + r + 2.0f, at.y + r + 4.0f, count, alpha);
}

void map_marker_draw_spawn(Vector2 at, float side, float alpha) {
    float r = floorf(side * 0.5f);
    DrawPolyLinesEx(at, 6, r + 4.0f, 30.0f, 1.5f, Fade(MAP_MARKER_SPAWN, alpha * 0.55f));
    DrawPoly(at, 6, r + 1.5f, 30.0f, faded(C_OUTLINE, alpha));
    DrawPoly(at, 6, r, 30.0f, Fade(MAP_MARKER_SPAWN, alpha));
    DrawPoly(at, 6, r - 2.5f, 30.0f, faded(C_PLATE, alpha));
    ui_icon_draw_ex("home", at.x, at.y, floorf(side * 0.6f), 0.0f, Fade(WHITE, alpha));
}

Rectangle map_marker_pin_rect(Vector2 cell, float side) {
    float tail = floorf(side * 0.35f);
    return (Rectangle){ floorf(cell.x - side * 0.5f), floorf(cell.y - side - tail), side, side };
}

void map_marker_draw_pin(Vector2 cell, float side, const char* icon, int count, Color accent, float alpha) {
    Rectangle plate = map_marker_pin_rect(cell, side);
    float bottom = plate.y + plate.height;
    fill_triangle(cell, (Vector2){ plate.x + side * 0.25f - 2.0f, bottom },
                  (Vector2){ plate.x + side * 0.75f + 2.0f, bottom }, faded(C_OUTLINE, alpha));
    fill_triangle((Vector2){ cell.x, cell.y - 3.0f }, (Vector2){ plate.x + side * 0.3f, bottom - 1.0f },
                  (Vector2){ plate.x + side * 0.7f, bottom - 1.0f }, faded(accent, alpha));
    draw_plate(plate, accent, alpha);
    if (NULL != icon && '\0' != icon[0])
        ui_icon_draw_ex(icon, plate.x + side * 0.5f, plate.y + side * 0.5f, side - 6.0f, 0.0f, Fade(WHITE, alpha));
    draw_count(plate.x + plate.width - 7.0f, plate.y + plate.height, count, alpha);
}

void map_marker_draw_quest(Rectangle box, const char* icon, bool muted, int count, float alpha) {
    draw_plate(box, muted ? MODAL_LABEL : MODAL_QUEST, alpha);
    if (!muted) DrawRectangleLinesEx(ui_rect_inset(box, 1.0f), 1.0f, Fade(MODAL_QUEST, alpha * 0.45f));
    ui_icon_draw_ex(icon, box.x + box.width * 0.5f, box.y + box.height * 0.5f, box.width - 6.0f, 0.0f,
                    Fade(WHITE, alpha * (muted ? 0.6f : 1.0f)));
    draw_count(box.x + box.width - 7.0f, box.y + box.height, count, alpha);
}

void map_marker_draw_talk(Rectangle box, float alpha) {
    Vector2 at = ui_rect_center(box);
    float r = floorf(fminf(box.width, box.height) * 0.5f);
    DrawCircleV(at, r + 2.0f, faded(C_OUTLINE, alpha));
    DrawCircleV(at, r, Fade(MODAL_QUEST, alpha));
    DrawCircleV(at, r - 2.0f, faded(C_PLATE, alpha));
    ui_icon_draw_ex("chat", at.x, at.y, 2.0f * r - 6.0f, 0.0f, Fade(WHITE, alpha));
}

/* An L of `arm` px and `thick` px from the outer corner (x, y); `sx`, `sy` (1 or -1) point inward. */
static void draw_bracket(float x, float y, float sx, float sy, float arm, float thick, Color color) {
    DrawRectangleRec((Rectangle){ 0 < sx ? x : x - arm, 0 < sy ? y : y - thick, arm, thick }, color);
    DrawRectangleRec((Rectangle){ 0 < sx ? x : x - thick, 0 < sy ? y : y - arm, thick, arm }, color);
}

void map_marker_draw_target(Rectangle box, bool followed, float alpha, double time) {
    Rectangle b = ui_rect_inset(box, -4.0f);
    if (followed) {
        float wave = 0.5f + 0.5f * sinf((float)time * 3.0f);
        float r = fmaxf(b.width, b.height) * 0.5f + 3.0f + 3.0f * wave;
        DrawRing(ui_rect_center(b), r, r + 2.0f, 0.0f, 360.0f, 32, Fade(MODAL_QUEST, alpha * (0.75f - 0.45f * wave)));
    }
    float arm = floorf(fminf(7.0f, fminf(b.width, b.height) * 0.45f));
    for (int corner = 0; 4 > corner; corner++) {
        float sx = 1 & corner ? -1.0f : 1.0f, sy = 2 & corner ? -1.0f : 1.0f;
        float x = 0 < sx ? b.x : b.x + b.width, y = 0 < sy ? b.y : b.y + b.height;
        draw_bracket(x - sx, y - sy, sx, sy, arm + 2.0f, 4.0f, faded(C_OUTLINE, alpha));
        draw_bracket(x, y, sx, sy, arm, 2.0f, Fade(MODAL_QUEST, alpha));
    }
}

void map_marker_draw_focus(Rectangle box, Color color, float alpha) {
    DrawRectangleLinesEx(ui_rect_inset(box, -5.0f), 2.0f, faded(C_OUTLINE, alpha));
    DrawRectangleLinesEx(ui_rect_inset(box, -4.0f), 2.0f, faded(color, alpha));
}

/* The arrow of a label: as wide as two thirds of the text height. */
static float label_arrow(int font) { return floorf((float)font * 0.6f); }

/* The width of the stat sum of a label: a gap, the stack icon and the number. */
static float label_value_w(int value, int font) {
    char number[16];
    snprintf(number, sizeof number, "%d", value);
    return LABEL_PAD + (float)font + 2.0f + 2.0f + (float)MeasureText(number, font);
}

Vector2 map_marker_label_size(const char* text, int font, bool arrow, int value) {
    float extra = (arrow ? label_arrow(font) + LABEL_PAD : 0.0f) + (0 <= value ? label_value_w(value, font) : 0.0f);
    return (Vector2){ (float)MeasureText(text, font) + 2.0f * LABEL_PAD + extra, (float)text_line_height(font) + 4.0f };
}

void map_marker_draw_label(Rectangle box, const char* text, int font, Color color, bool arrow, bool strong, int value,
                           float alpha) {
    DrawRectangleRec(box, faded(C_OUTLINE, alpha));
    DrawRectangleLinesEx(box, 1.0f, Fade(color, alpha * (strong ? 1.0f : 0.55f)));
    float x = box.x + LABEL_PAD;
    if (arrow) {
        float s = label_arrow(font), cy = box.y + box.height * 0.5f;
        fill_triangle((Vector2){ x + s, cy }, (Vector2){ x, cy - s * 0.6f }, (Vector2){ x, cy + s * 0.6f },
                      Fade(color, alpha));
        x += s + LABEL_PAD;
    }
    DrawText(text, (int)x, (int)box.y + 2, font, Fade(strong ? WHITE : color, alpha));
    if (0 > value) return;
    char number[16];
    snprintf(number, sizeof number, "%d", value);
    float icon = (float)font + 2.0f, right = box.x + box.width - LABEL_PAD;
    float nx = right - (float)MeasureText(number, font);
    ui_icon_draw_ex("stack", nx - 2.0f - icon * 0.5f, box.y + box.height * 0.5f, icon, 0.0f, Fade(WHITE, alpha));
    DrawText(number, (int)nx, (int)box.y + 2, font, Fade(WHITE, alpha));
}

bool map_marker_place_label(Vector2 anchor, Vector2 size, float gap, Rectangle bounds, const Rectangle* taken,
                            int count, Rectangle* out) {
    Vector2 offsets[4] = {
        { -size.x * 0.5f, gap },
        { -size.x * 0.5f, -gap - size.y },
        { gap, -size.y * 0.5f },
        { -gap - size.x, -size.y * 0.5f },
    };
    for (int side = 0; 4 > side; side++) {
        Rectangle box = { floorf(anchor.x + offsets[side].x), floorf(anchor.y + offsets[side].y), size.x, size.y };
        box.x = Clamp(box.x, bounds.x + 2.0f, bounds.x + bounds.width - size.x - 2.0f);
        box.y = Clamp(box.y, bounds.y + 2.0f, bounds.y + bounds.height - size.y - 2.0f);
        bool clear = true;
        for (int i = 0; clear && count > i; i++) clear = !CheckCollisionRecs(box, taken[i]);
        if (!clear) continue;
        *out = box;
        return true;
    }
    return false;
}
