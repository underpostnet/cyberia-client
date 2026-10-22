#include "container_panel.h"

#include "content/stat_contract_generated.h"
#include "inventory_bar.h"
#include "item_slot_grid.h"
#include "modal.h"
#include "modal_notification.h"
#include "sim/container_ops.h"
#include "sim/local_player.h"
#include "stack_move.h"
#include "text.h"
#include "ui_icon.h"
#include "ui_scroll.h"
#include "utils.h"
#include "view/viewport.h"
#include "world/object_layers_management.h"
#include "world_drop.h"

#include <raylib.h>
#include <stdio.h>
#include <string.h>

#define CP_SCROLL_LANE_W 30.0f
#define CP_SCROLL_ICON   20
#define CP_SCROLL_STEP   96.0f

static const Color C_ACCEPT = { 120, 200, 140, 255 };
static const Color C_REFUSE = { 230, 170, 110, 255 };
static const Color C_BLOCK  = { 210,  90,  80, 255 };

static struct {
    bool                    open;
    Box                     box;       /* BOX_STORAGE or BOX_VAULT */
    char                    action_code[MAX_ID_LENGTH];
    int                     capacity;  /* the open slots of a vault */
    ContainerPanelInspectFn inspect;
    ItemSlotGrid            grid;
    UIScroll                scroll;
    float                   content_h;
    Rectangle               drop_zone;  /* the visible drop zone of the personal storage */
} s_panel;

static int hint_font(void) { return viewport_is_mobile() ? 12 : 14; }

static void hint_line(const char* line, Rectangle area, float y, Color color) {
    int font = hint_font();
    int x = (int)(area.x + (area.width - (float)MeasureText(line, font)) * 0.5f);
    text_draw_outlined(line, x, (int)y, font, color, BLACK, 1);
}

/* The stacks of the open container; NULL for a vault still empty. */
static const Stacks* held(void) {
    if (BOX_STORAGE == s_panel.box) return &g_local_player.storage;
    const Vault* vault = local_player_vault(s_panel.action_code);
    return NULL != vault ? &vault->stacks : NULL;
}

static int open_cells(void) {
    return BOX_STORAGE == s_panel.box ? local_player_open_slots(CYBERIA_STORAGE_CAPACITY) : s_panel.capacity;
}

/* The personal storage locks every cell its level has not opened yet; the
 * level can rise while the panel is open. A vault locks the cells past its
 * capacity. */
static void update_locks(void) {
    int open = open_cells();
    for (int i = 0; i < s_panel.grid.capacity; i++) {
        bool locked = i >= open;
        if (locked == s_panel.grid.locked[i]) continue;
        int level = BOX_STORAGE == s_panel.box ? local_player_unlock_level(CYBERIA_STORAGE_CAPACITY, i) : 0;
        item_slot_grid_set_locked(&s_panel.grid, i, locked, level);
    }
}

/* Each stack shows in the cell of its slot. */
static void sync(void) {
    for (int i = 0; i < s_panel.grid.capacity; i++) item_slot_grid_set(&s_panel.grid, i, NULL);
    const Stacks* stacks = held();
    for (int k = 0; NULL != stacks && k < stacks->count; k++) {
        item_slot_grid_set(&s_panel.grid, stacks->items[k].slot, &stacks->items[k]);
    }
}

static void seed(int cells, ContainerPanelInspectFn inspect) {
    s_panel.open = true;
    s_panel.inspect = inspect;
    s_panel.content_h = 0.0f;
    ui_scroll_reset(&s_panel.scroll);
    item_slot_grid_init(&s_panel.grid, cells);
    update_locks();
    sync();
}

void container_panel_open_vault(EntityID terminal_id, const char* action_code, int capacity,
                                ContainerPanelInspectFn inspect) {
    container_ops_bind_vault(terminal_id, action_code, capacity);
    s_panel.box = BOX_VAULT;
    s_panel.capacity = capacity;
    copy_str(s_panel.action_code, sizeof s_panel.action_code, action_code);
    /* A vault over its capacity shows every stack. */
    int cells = capacity;
    const Stacks* stacks = held();
    for (int k = 0; NULL != stacks && k < stacks->count; k++) {
        if (stacks->items[k].slot >= cells) cells = stacks->items[k].slot + 1;
    }
    seed(cells, inspect);
}

void container_panel_open_storage(ContainerPanelInspectFn inspect) {
    s_panel.box = BOX_STORAGE;
    s_panel.action_code[0] = '\0';
    seed(CYBERIA_STORAGE_SLOTS_MAX, inspect);
}

void container_panel_close(void) { s_panel.open = false; }

bool container_panel_is_open(void) { return s_panel.open; }

/* Where the dragged stack was released: the ghost under the pointer. */
static Rectangle release_rect(const ItemSlotGridEvent* ev) {
    float size = s_panel.grid.cell_size;
    return (Rectangle){ ev->point.x - size * 0.5f, ev->point.y - size * 0.5f, size, size };
}

/* A stack that does not land slides back into its cell from the release. */
static void slide_back(const ItemSlotGridEvent* ev) { container_panel_animate_from(ev->from_index, release_rect(ev)); }

/* A stack released over the bar lands on the cell under it, one released on
 * the drop zone of the personal storage drops on the ground, and anywhere else
 * it goes back. Over a full bag a refusal says why. */
static void drop_out(const ItemSlotGridEvent* ev) {
    int slot = 0;
    if (BOX_STORAGE == s_panel.box && CheckCollisionPointRec(ev->point, s_panel.drop_zone)) {
        slide_back(ev);
        world_drop_ask(BOX_STORAGE, ev->from_index, ev->payload.item_id, ev->payload.quantity, (Vector2){ -1.0f, -1.0f });
        return;
    }
    if (inventory_bar_cell_at((int)ev->point.x, (int)ev->point.y, &slot) &&
        stack_move_drop(s_panel.box, ev->from_index, BOX_BAG, slot, release_rect(ev))) return;
    slide_back(ev);
    if (inventory_bar_point_covered((int)ev->point.x, (int)ev->point.y) &&
        !local_player_inventory_has_room(ev->payload.item_id)) {
        modal_notification_show("Bag Full", "Free a bag slot, or reach a higher level to open one.", C_REFUSE);
    }
}

static void tap(int at) {
    if (0 > at) return;
    if ('\0' != s_panel.grid.cells[at].item_id[0]) {
        ObjectLayerState ols = s_panel.grid.cells[at];
        if (NULL != s_panel.inspect) s_panel.inspect(&ols);
        return;
    }
    if (!s_panel.grid.locked[at]) return;
    int level = s_panel.grid.unlock_level[at];
    char body[96];
    if (BOX_VAULT == s_panel.box) copy_str(body, sizeof(body), "This vault holds no more stacks.");
    else if (0 < level)          snprintf(body, sizeof(body), "This storage slot opens at level %d.", level);
    else                         copy_str(body, sizeof(body), "This storage slot does not open in this world.");
    modal_notification_show("Locked Slot", body, C_REFUSE);
}

static void handle(const ItemSlotGridEvent* ev) {
    switch (ev->type) {
        case ITEM_SLOT_GRID_EVENT_MOVE:
        case ITEM_SLOT_GRID_EVENT_SWAP:
            if (!stack_move_drop(s_panel.box, ev->from_index, s_panel.box, ev->to_index, release_rect(ev)))
                slide_back(ev);
            return;
        case ITEM_SLOT_GRID_EVENT_DROP_OUT: drop_out(ev); return;
        case ITEM_SLOT_GRID_EVENT_TAP:      tap(ev->to_index); return;
        case ITEM_SLOT_GRID_EVENT_NONE:     return;
    }
}

void container_panel_update(Rectangle content, bool ready, float dt) {
    if (!s_panel.open) return;
    update_locks();
    sync();
    /* A cell lifted out of the scrolled content owns the gesture; without this
     * the touch fallback in ui_scroll would scroll the grid out from under a
     * drag aimed at the inventory bar. */
    if (s_panel.grid.pressed) ui_scroll_cancel_press(&s_panel.scroll);
    ui_scroll_update(&s_panel.scroll, content, s_panel.content_h, dt);
    item_slot_grid_update(&s_panel.grid, dt);
    ItemSlotGridEvent ev;
    if (item_slot_grid_take_event(&s_panel.grid, &ev) && ready) handle(&ev);
}

static Rectangle scroll_lane(Rectangle content) {
    float width = content.width > CP_SCROLL_LANE_W ? CP_SCROLL_LANE_W : 0.0f;
    return (Rectangle){ content.x + content.width - width, content.y, width, content.height };
}

float container_panel_draw(Rectangle content) {
    if (!s_panel.open) return 0.0f;
    bool  storage = BOX_STORAGE == s_panel.box;
    int   font = hint_font();
    float head = (float)text_line_height(font) * (storage ? 2.0f : 1.0f) + 6.0f;
    float top = content.y - ui_scroll_offset(&s_panel.scroll);
    Rectangle lane = scroll_lane(content);
    Rectangle grid = { content.x, content.y, content.width - lane.width, content.height };

    ui_scroll_set_scrollbar_bounds(&s_panel.scroll, lane);
    ui_scroll_begin(&s_panel.scroll);
    if (storage) {
        char used[64];
        snprintf(used, sizeof(used), "%d / %d slots used", g_local_player.storage.count,
                 local_player_open_slots(CYBERIA_STORAGE_CAPACITY));
        hint_line(used, grid, top, MODAL_TEXT);
        hint_line(g_local_player.account ? "Drag to and from your bag. Storage stays with your account."
                                         : "Drag to and from your bag. Storage empties when you leave.",
                  grid, top + (float)text_line_height(font), MODAL_LABEL);
        float zone_h = (float)font + 16.0f;
        Rectangle zone = { grid.x, top + head, grid.width, zone_h };
        s_panel.drop_zone = GetCollisionRec(zone, grid);
        bool over = item_slot_grid_is_dragging(&s_panel.grid) &&
                    CheckCollisionPointRec(s_panel.grid.pointer, s_panel.drop_zone);
        DrawRectangleRec(zone, over ? Fade(C_ACCEPT, 0.35f) : (Color){ 30, 34, 52, 200 });
        DrawRectangleLinesEx(zone, over ? 2.0f : 1.0f, over ? C_ACCEPT : MODAL_LABEL);
        const char* label = "Drag here to drop on the ground";
        float icon = (float)font + 4.0f;
        float w = icon + 6.0f + (float)MeasureText(label, font);
        float lx = zone.x + (zone.width - w) * 0.5f;
        ui_icon_draw("drop", lx + icon * 0.5f, zone.y + zone_h * 0.5f, (int)icon, false, 0.0f);
        text_draw_outlined(label, (int)(lx + icon + 6.0f), (int)(zone.y + (zone_h - (float)font) * 0.5f),
                           font, MODAL_TEXT, BLACK, 1);
        head += zone_h + 6.0f;
    } else {
        hint_line("Drag between the vault and your inventory bar", grid, top, MODAL_LABEL);
    }
    /* Cells flow across the full width and wrap, so a large container is taller
     * than the panel and the content scrolls to reach its lower rows. */
    item_slot_grid_layout(&s_panel.grid, (Rectangle){ grid.x, top + head, grid.width, 0.0f });
    /* The viewport ui_scroll scissors to and taps are gated on — not the
     * tab-pop rect, which is animated. */
    item_slot_grid_set_clip(&s_panel.grid, grid);
    item_slot_grid_draw(&s_panel.grid, obj_layers_mgr_get());
    s_panel.content_h = head + item_slot_grid_height(&s_panel.grid) + 4.0f;
    ui_scroll_end(&s_panel.scroll);
    if (0.0f < lane.width) {
        float center_x = lane.x + lane.width * 0.5f;
        ui_icon_draw("arrow-up", center_x, lane.y + lane.height * 0.25f, CP_SCROLL_ICON, false, 0.0f);
        ui_icon_draw("arrow-down", center_x, lane.y + lane.height * 0.75f, CP_SCROLL_ICON, false, 0.0f);
    }
    return s_panel.content_h;
}

bool container_panel_press(Rectangle content, int mx, int my) {
    if (!s_panel.open) return false;
    Rectangle lane = scroll_lane(content);
    if (CheckCollisionPointRec((Vector2){ (float)mx, (float)my }, lane)) {
        float delta = (float)my < lane.y + lane.height * 0.5f ? -CP_SCROLL_STEP : CP_SCROLL_STEP;
        ui_scroll_cancel_press(&s_panel.scroll);
        ui_scroll_nudge(&s_panel.scroll, content, s_panel.content_h, delta);
        return true;
    }
    /* Cells own the press (that is what arms a drag); the margin around them
     * falls through to the scroll so the panel is still draggable. */
    if (!item_slot_grid_handle_press(&s_panel.grid, mx, my)) {
        ui_scroll_on_press(&s_panel.scroll, mx, my);
    }
    return true;
}

bool container_panel_wheel(Rectangle content, float wheel_delta) {
    return s_panel.open && ui_scroll_on_wheel(&s_panel.scroll, content, s_panel.content_h, wheel_delta);
}

/* A cell dragged from the grid marks the bar cell under it: green where it
 * lands, red where it does not. */
void container_panel_draw_drag(void) {
    if (!s_panel.open) return;
    int slot = 0;
    const ItemSlotGrid* g = &s_panel.grid;
    if (item_slot_grid_is_dragging(g) && inventory_bar_cell_at((int)g->pointer.x, (int)g->pointer.y, &slot)) {
        MoveKind kind = container_ops_check(s_panel.box, g->drag_index, BOX_BAG, slot, g->drag_item.quantity);
        DrawRectangleLinesEx(inventory_bar_cell_rect(slot), 3.0f, MOVE_REFUSED == kind ? C_BLOCK : C_ACCEPT);
    }
    item_slot_grid_draw_drag(g, obj_layers_mgr_get());
}

bool container_panel_cell_at(int mx, int my, Box* box, int* slot) {
    if (!s_panel.open) return false;
    int cell = item_slot_grid_cell_at(&s_panel.grid, mx, my);
    if (0 > cell) return false;
    *box = s_panel.box;
    *slot = cell;
    return true;
}

Rectangle container_panel_cell_rect(int slot) { return item_slot_grid_cell_rect(&s_panel.grid, slot); }

void container_panel_animate_from(int slot, Rectangle origin) {
    item_slot_grid_animate_from_point(&s_panel.grid, slot, (Vector2){ origin.x, origin.y });
}
