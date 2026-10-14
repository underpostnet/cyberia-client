/* The slot grid: a drag from another container reaches the host over any open
 * cell and never over a locked one, and a locked cell keeps the stack it holds. */
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

/* Hand a drag in from outside and release it over cell `index`. */
static ItemSlotGridEvent drop_in(ItemSlotGrid* g, int index) {
    ObjectLayerState gem = stack("gem", 1);
    s_pointer = center(g, index);
    s_down = true;
    item_slot_grid_begin_external_drag(g, &gem, s_pointer);
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

    ItemSlotGridEvent on_stack = drop_in(&g, 0);
    assert(ITEM_SLOT_GRID_EVENT_DROP_IN == on_stack.type && 0 == on_stack.to_index);
    assert(0 == strcmp("ore", g.cells[0].item_id) && 3 == g.cells[0].quantity);
    assert(ITEM_SLOT_GRID_EVENT_NONE == drop_in(&g, 3).type);
    ItemSlotGridEvent in = drop_in(&g, 1);
    assert(ITEM_SLOT_GRID_EVENT_DROP_IN == in.type && 1 == in.to_index && 0 == strcmp("gem", in.payload.item_id));

    /* A locked cell keeps its stack, and the stack can leave it. */
    assert(0 == strcmp("ore", g.cells[3].item_id) && 6 == g.unlock_level[3]);
    s_pointer = center(&g, 3);
    s_down = true;
    assert(item_slot_grid_handle_press(&g, (int)s_pointer.x, (int)s_pointer.y));
    item_slot_grid_update(&g, 0.016f);
    s_pointer = center(&g, 2);
    item_slot_grid_update(&g, 0.016f);
    s_down = false;
    item_slot_grid_update(&g, 0.016f);
    ItemSlotGridEvent move;
    assert(item_slot_grid_take_event(&g, &move));
    assert(ITEM_SLOT_GRID_EVENT_MOVE == move.type && 3 == move.from_index && 2 == move.to_index);

    puts("item_slot_grid_test OK");
    return 0;
}
