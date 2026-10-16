/* The slot grid: a press that never travels is a tap, a drag reaches the host
 * over any open cell and never over a locked one, and a locked cell keeps the
 * stack it holds. */
#include "ui/item_slot_grid.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static Vector2 s_pointer;
static bool s_down;

Vector2 GetMousePosition(void) { return s_pointer; }
int GetTouchPointCount(void) { return 0; }
Vector2 GetTouchPosition(int index) { return s_pointer; }
bool IsMouseButtonDown(int button) { return s_down; }
bool CheckCollisionPointRec(Vector2 p, Rectangle r) {
    return p.x >= r.x && p.x < r.x + r.width && p.y >= r.y && p.y < r.y + r.height;
}
bool ui_button_hit(Rectangle r, int mx, int my) { return CheckCollisionPointRec((Vector2){ (float)mx, (float)my }, r); }
void item_slot_draw_ex(Rectangle r, const ObjectLayerState* ols, ObjectLayersManager* mgr,
                       Color highlight, float amount, bool full_color) {}
void item_slot_draw_empty(Rectangle r) {}
void item_slot_draw_locked(Rectangle r, int unlock_level) {}
void DrawRectangleLinesEx(Rectangle r, float thick, Color color) {}

static Vector2 center(const ItemSlotGrid* g, int index) {
    Rectangle r = item_slot_grid_cell_rect(g, index);
    return (Vector2){ r.x + r.width * 0.5f, r.y + r.height * 0.5f };
}

static ObjectLayerState stack(const char* item_id, int qty) {
    ObjectLayerState ols = { .quantity = qty };
    strcpy(ols.item_id, item_id);
    return ols;
}

/* Press cell `from`, release over cell `to`, and take the event. */
static ItemSlotGridEvent gesture(ItemSlotGrid* g, int from, int to) {
    s_pointer = center(g, from);
    s_down = true;
    assert(item_slot_grid_handle_press(g, (int)s_pointer.x, (int)s_pointer.y));
    item_slot_grid_update(g, 0.016f);
    s_pointer = center(g, to);
    item_slot_grid_update(g, 0.016f);
    s_down = false;
    item_slot_grid_update(g, 0.016f);
    ItemSlotGridEvent ev = { 0 };
    item_slot_grid_take_event(g, &ev);
    return ev;
}

int main(void) {
    ItemSlotGrid g;
    item_slot_grid_init(&g, 4);
    item_slot_grid_layout(&g, (Rectangle){ 0, 0, 400, 0 });
    ObjectLayerState ore = stack("ore", 3);
    item_slot_grid_set(&g, 0, &ore);
    item_slot_grid_set(&g, 3, &ore);
    item_slot_grid_set_locked(&g, 3, true, 6);

    ItemSlotGridEvent tap = gesture(&g, 0, 0);
    assert(ITEM_SLOT_GRID_EVENT_TAP == tap.type && 0 == tap.to_index);
    assert(ITEM_SLOT_GRID_EVENT_NONE == gesture(&g, 0, 3).type);
    ItemSlotGridEvent swap = gesture(&g, 3, 0);
    assert(ITEM_SLOT_GRID_EVENT_SWAP == swap.type && 3 == swap.from_index && 0 == swap.to_index);

    /* A locked cell keeps its stack, and the stack can leave it. */
    assert(0 == strcmp("ore", g.cells[3].item_id) && 6 == g.unlock_level[3]);
    ItemSlotGridEvent move = gesture(&g, 3, 2);
    assert(ITEM_SLOT_GRID_EVENT_MOVE == move.type && 3 == move.from_index && 2 == move.to_index);

    puts("item_slot_grid_test OK");
    return 0;
}
