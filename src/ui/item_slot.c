#include "item_slot.h"
#include "text.h"

#include "domain/local_player.h"
#include "game_state.h"
#include "ol_as_animated_ico.h"
#include "world_types.h"
#include "ui_button.h"

#include <stdio.h>

static const Color C_SLOT_BG      = {  25,  25,  40, 200 };
static const Color C_SLOT_BORDER  = {  70,  70, 100, 180 };
static const Color C_ACTIVE_GLOW  = { 100, 200, 255, 240 };
static const Color C_QTY_BG       = {   0,   0,   0, 190 };
static const Color C_QTY_TEXT     = { 255, 230,  80, 255 };

bool item_slot_hit(Rectangle r, int mx, int my) {
    return ((float)mx >= r.x && (float)mx < r.x + r.width &&
            (float)my >= r.y && (float)my < r.y + r.height);
}

static Color lerp_color(Color from, Color to, float t) {
    if (t <= 0.0f) return from;
    if (t >= 1.0f) return to;
    return (Color){
        (unsigned char)(from.r + (to.r - from.r) * t),
        (unsigned char)(from.g + (to.g - from.g) * t),
        (unsigned char)(from.b + (to.b - from.b) * t),
        (unsigned char)(from.a + (to.a - from.a) * t),
    };
}

void item_slot_draw(Rectangle r, const ObjectLayerState* ols, ObjectLayersManager* mgr) {
    item_slot_draw_ex(r, ols, mgr, WHITE, 0.0f, false);
}

void item_slot_draw_ex(Rectangle r, const ObjectLayerState* ols, ObjectLayersManager* mgr,
                       Color highlight, float highlight_t, bool sprite_full_color) {
    int pad = (int)(r.width * 0.07f);
    if (pad < 3) pad = 3;
    int fs = (int)(r.width * 0.26f);
    if (fs < 13) fs = 13;

    bool active    = ols->active;
    bool activable = true;
    if (mgr && ols->item_id[0] != '\0') {
        ObjectLayer* ol_data = lookup_cached_layer(ols->item_id);
        if (ol_data) activable = ol_data->data.item.activable;
    }
    /* Dead-state (Fragmentation) items equip only during the Fragmented
     * State — lock badge while the player is alive. */
    if (game_state_is_dead_item(ols->item_id) &&
        STATUS_ICON_DEAD != local_player_status_icon())
        activable = false;

    /* Pixel-retro style: black outer border, flat fill, highlight/shadow edges,
     * white inner outline on hover/active. */
    bool hovered = CheckCollisionPointRec(GetMousePosition(), r);
    Color base = (active && activable) ? (Color){ 40, 60, 100, 235 } : C_SLOT_BG;
    Color fill = hovered ? (Color){
        (unsigned char)(base.r + (255 - base.r) * 0.14f),
        (unsigned char)(base.g + (255 - base.g) * 0.14f),
        (unsigned char)(base.b + (255 - base.b) * 0.14f),
        base.a
    } : base;
    Color highlight_edge = (Color){
        (unsigned char)(fill.r + (255 - fill.r) * 0.45f),
        (unsigned char)(fill.g + (255 - fill.g) * 0.45f),
        (unsigned char)(fill.b + (255 - fill.b) * 0.45f),
        fill.a
    };
    Color shadow_edge = (Color){
        (unsigned char)(fill.r * 0.55f),
        (unsigned char)(fill.g * 0.55f),
        (unsigned char)(fill.b * 0.55f),
        fill.a
    };

    /* highlight_t blends the slot's neutral bg/border toward `highlight` — used
     * to briefly "color" a slot (e.g. a fresh reward settling in). */
    Color slot_bg = lerp_color(fill, (Color){ highlight.r, highlight.g, highlight.b, fill.a }, highlight_t);

    /* Black outer border */
    DrawRectangleRec(r, BLACK);
    /* Inner fill */
    Rectangle inner = { r.x + 2.0f, r.y + 2.0f, r.width - 4.0f, r.height - 4.0f };
    DrawRectangleRec(inner, slot_bg);
    /* Top highlight edge */
    DrawRectangle((int)(inner.x + 4.0f), (int)inner.y, (int)(inner.width - 8.0f), 2, highlight_edge);
    /* Bottom shadow edge */
    DrawRectangle((int)(inner.x + 4.0f), (int)(inner.y + inner.height - 2.0f),
                  (int)(inner.width - 8.0f), 2, shadow_edge);
    /* White inner outline on hover or active+activable */
    if (hovered || (active && activable))
        DrawRectangleLinesEx(inner, 1.0f, (active && activable) ? C_ACTIVE_GLOW : WHITE);

    if (ols->item_id[0] == '\0') {
        DrawCircle((int)(r.x + r.width * 0.5f), (int)(r.y + r.height * 0.5f),
                   3.0f, (Color){ 80, 80, 100, 120 });
        return;
    }

    Color tint = (sprite_full_color || (active && activable))
               ? WHITE : (Color){ 180, 180, 180, 160 };
    int inner_sprite = (int)r.width - pad * 2;
    ol_as_ico_draw(mgr, ols->item_id,
                   (int)(r.x + pad), (int)(r.y + pad), inner_sprite,
                   OL_ICO_DEFAULT_DIR, 0, tint);

    if (ols->quantity > 1) {
        char buf[16];
        if (ols->quantity >= 1000) snprintf(buf, sizeof(buf), "%dk", ols->quantity / 1000);
        else                       snprintf(buf, sizeof(buf), "%d", ols->quantity);
        int tw = MeasureText(buf, fs);
        int bx = (int)(r.x + r.width - tw - 3);
        int by = (int)(r.y + r.height - fs - 2);
        DrawRectangle(bx - 1, by - 1, tw + 2, fs + 2, C_QTY_BG);
        DrawText(buf, bx, by, fs, C_QTY_TEXT);
    }

    if (!activable) {
        int lx = (int)(r.x + 3), ly = (int)(r.y + 3);
        DrawRectangle(lx - 1, ly - 1, fs + 2, fs + 2, (Color){ 0, 0, 0, 160 });
        DrawText("-", lx + 1, ly, fs, (Color){ 255, 165, 0, 220 });
    }
}