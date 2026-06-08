#include "item_slot.h"

#include "ol_as_animated_ico.h"

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

void item_slot_draw(Rectangle r, const ObjectLayerState* ols, ObjectLayersManager* mgr) {
    int pad = (int)(r.width * 0.07f);
    if (pad < 3) pad = 3;
    int fs = (int)(r.width * 0.17f);
    if (fs < 10) fs = 10;

    DrawRectangleRec(r, C_SLOT_BG);

    bool active    = ols->active;
    bool activable = true;
    if (mgr && ols->item_id[0] != '\0') {
        ObjectLayer* ol_data = lookup_cached_layer(ols->item_id);
        if (ol_data) activable = ol_data->data.item.activable;
    }
    DrawRectangleLinesEx(r, 2.0f, (active && activable) ? C_ACTIVE_GLOW : C_SLOT_BORDER);

    if (ols->item_id[0] == '\0') {
        DrawCircle((int)(r.x + r.width * 0.5f), (int)(r.y + r.height * 0.5f),
                   3.0f, (Color){ 80, 80, 100, 120 });
        return;
    }

    Color tint = (active && activable) ? WHITE : (Color){ 180, 180, 180, 160 };
    int inner  = (int)r.width - pad * 2;
    ol_as_ico_draw(mgr, ols->item_id,
                   (int)(r.x + pad), (int)(r.y + pad), inner,
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
