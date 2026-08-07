#include "ui/item_slot_grid.h"

#include "item_slot.h"

#include <math.h>
#include <string.h>

/* Pixels the pointer must travel before a press becomes a drag rather than a
 * tap — the same disambiguation ui_scroll applies to its strip. */
#define GRID_DRAG_SLOP 6.0f
#define GRID_GAP       6.0f
/* Seconds an occupant takes to slide into a cell it was just moved to. */
#define GRID_SETTLE_DUR 0.70f

static const Color GRID_CELL_EMPTY  = {  18,  24,  38, 200 };
static const Color GRID_CELL_BORDER = {  58,  78, 110, 200 };
static const Color GRID_HOVER_FREE  = { 120, 200, 140, 255 };
static const Color GRID_HOVER_SWAP  = { 235, 190,  70, 255 };

/* ── Pointer ──────────────────────────────────────────────────────────── */

static Vector2 grid_pointer_position(void) {
    if (GetTouchPointCount() > 0) return GetTouchPosition(0);
    return GetMousePosition();
}

static bool grid_pointer_down(void) {
    return GetTouchPointCount() > 0 || IsMouseButtonDown(MOUSE_BUTTON_LEFT);
}

/* ── Lifecycle ────────────────────────────────────────────────────────── */

void item_slot_grid_init(ItemSlotGrid* g, int capacity) {
    if (!g) return;
    if (capacity < 0) capacity = 0;
    if (capacity > ITEM_SLOT_GRID_MAX_SLOTS) capacity = ITEM_SLOT_GRID_MAX_SLOTS;
    memset(g, 0, sizeof(*g));
    g->capacity = capacity;
    for (int i = 0; i < ITEM_SLOT_GRID_MAX_SLOTS; i++) g->settle_age[i] = GRID_SETTLE_DUR;
    g->gap = GRID_GAP;
    g->drag_index = -1;
    g->hover_index = -1;
}

void item_slot_grid_set(ItemSlotGrid* g, int index, const ObjectLayerState* ols) {
    if (!g || index < 0 || index >= g->capacity) return;
    if (ols && '\0' != ols->item_id[0]) g->cells[index] = *ols;
    else                               memset(&g->cells[index], 0, sizeof(g->cells[index]));
}

void item_slot_grid_clear(ItemSlotGrid* g) {
    if (!g) return;
    memset(g->cells, 0, sizeof(g->cells));
}

/* ── Layout ───────────────────────────────────────────────────────────── */

static int grid_rows(const ItemSlotGrid* g) {
    if (!g || g->cols < 1) return 0;
    return (g->capacity + g->cols - 1) / g->cols;
}

void item_slot_grid_layout(ItemSlotGrid* g, Rectangle area) {
    if (!g || g->capacity < 1) return;
    /* As many columns as the width admits without any cell exceeding the cap,
     * then never more columns than there are cells to put in them. */
    int cols = (int)ceilf((area.width + g->gap) / (ITEM_SLOT_GRID_CELL_MAX + g->gap));
    if (cols < 1) cols = 1;
    if (cols > g->capacity) cols = g->capacity;

    float cell = (area.width - (float)(cols - 1) * g->gap) / (float)cols;
    if (cell > ITEM_SLOT_GRID_CELL_MAX) cell = ITEM_SLOT_GRID_CELL_MAX;
    if (cell < 1.0f) cell = 1.0f;

    g->cols = cols;
    g->cell_size = cell;
    g->bounds = (Rectangle){ area.x, area.y,
                             (float)cols * cell + (float)(cols - 1) * g->gap,
                             item_slot_grid_height(g) };
}

float item_slot_grid_height(const ItemSlotGrid* g) {
    int rows = grid_rows(g);
    if (rows < 1) return 0.0f;
    return (float)rows * g->cell_size + (float)(rows - 1) * g->gap;
}

void item_slot_grid_set_clip(ItemSlotGrid* g, Rectangle clip) {
    if (!g) return;
    g->clip = clip;
}

Rectangle item_slot_grid_cell_rect(const ItemSlotGrid* g, int index) {
    if (!g || g->cols < 1 || index < 0 || index >= g->capacity) return (Rectangle){ 0 };
    int col = index % g->cols;
    int row = index / g->cols;
    return (Rectangle){ g->bounds.x + (float)col * (g->cell_size + g->gap),
                        g->bounds.y + (float)row * (g->cell_size + g->gap),
                        g->cell_size, g->cell_size };
}

int item_slot_grid_index_at(const ItemSlotGrid* g, int mx, int my) {
    if (!g) return -1;
    /* A wrapped grid overflows the panel that scrolls it, so cells exist below
     * the viewport — and under the inventory bar. Only what the player can see
     * may be hit, or a drop aimed at the bar would land on a hidden cell. */
    if (g->clip.width > 0.0f && g->clip.height > 0.0f &&
        !CheckCollisionPointRec((Vector2){ (float)mx, (float)my }, g->clip)) return -1;
    for (int i = 0; i < g->capacity; i++) {
        if (item_slot_hit(item_slot_grid_cell_rect(g, i), mx, my)) return i;
    }
    return -1;
}

static bool grid_point_in_visible_bounds(const ItemSlotGrid* g, Vector2 point) {
    if (!g || !CheckCollisionPointRec(point, g->bounds)) return false;
    return 0.0f >= g->clip.width || 0.0f >= g->clip.height ||
           CheckCollisionPointRec(point, g->clip);
}

/* ── Gesture ──────────────────────────────────────────────────────────── */

bool item_slot_grid_handle_press(ItemSlotGrid* g, int mx, int my) {
    if (!g) return false;
    int index = item_slot_grid_index_at(g, mx, my);
    if (index < 0) return false;

    g->pressed = true;
    g->dragging = false;
    g->external = false;
    g->press_point = (Vector2){ (float)mx, (float)my };
    g->pointer = g->press_point;
    /* Only a filled cell can start a drag; an empty one still reports a tap. */
    g->drag_index = '\0' != g->cells[index].item_id[0] ? index : -1;
    g->drag_item = g->cells[index];
    return true;
}

void item_slot_grid_begin_external_drag(ItemSlotGrid* g, const ObjectLayerState* ols,
                                        Vector2 point) {
    if (!g || !ols || '\0' == ols->item_id[0]) return;
    g->pressed = true;
    g->dragging = true;
    g->external = true;
    g->drag_index = -1;
    g->drag_item = *ols;
    g->press_point = point;
    g->pointer = point;
}

static void emit(ItemSlotGrid* g, ItemSlotGridEventType type, int to_index) {
    g->pending.type = type;
    g->pending.from_index = g->external ? -1 : g->drag_index;
    g->pending.to_index = to_index;
    g->pending.point = g->pointer;
    g->pending.payload = g->drag_item;
}

static void end_gesture(ItemSlotGrid* g) {
    g->pressed = false;
    g->dragging = false;
    g->external = false;
    g->drag_index = -1;
    g->hover_index = -1;
    memset(&g->drag_item, 0, sizeof(g->drag_item));
}

void item_slot_grid_animate_from_point(ItemSlotGrid* g, int index, Vector2 origin) {
    if (!g || index < 0 || index >= g->capacity) return;
    g->settle_from[index] = origin;
    g->settle_age[index] = 0.0f;
}

void item_slot_grid_animate_move(ItemSlotGrid* g, int from_index, int to_index) {
    Rectangle from = item_slot_grid_cell_rect(g, from_index);
    item_slot_grid_animate_from_point(g, to_index, (Vector2){ from.x, from.y });
}

static void return_dragged_item(ItemSlotGrid* g) {
    if (!g || 0 > g->drag_index) return;
    float half = g->cell_size * 0.5f;
    item_slot_grid_animate_from_point(g, g->drag_index,
                                      (Vector2){ g->pointer.x - half, g->pointer.y - half });
}

void item_slot_grid_update(ItemSlotGrid* g, float dt) {
    if (!g) return;
    for (int i = 0; i < g->capacity; i++) {
        if (g->settle_age[i] < GRID_SETTLE_DUR) g->settle_age[i] += dt;
    }
    if (!g->pressed) return;

    g->pointer = grid_pointer_position();
    if (!g->dragging && g->drag_index >= 0) {
        float dx = g->pointer.x - g->press_point.x;
        float dy = g->pointer.y - g->press_point.y;
        if (sqrtf(dx * dx + dy * dy) > GRID_DRAG_SLOP) g->dragging = true;
    }

    /* Live preview of where the drop would land. */
    g->hover_index = g->dragging
        ? item_slot_grid_index_at(g, (int)g->pointer.x, (int)g->pointer.y) : -1;

    if (grid_pointer_down()) return;

    if (!g->dragging) {
        /* A press that never travelled is a tap on whatever it landed on. */
        int index = item_slot_grid_index_at(g, (int)g->pointer.x, (int)g->pointer.y);
        if (index >= 0) emit(g, ITEM_SLOT_GRID_EVENT_TAP, index);
        end_gesture(g);
        return;
    }

    int target = item_slot_grid_index_at(g, (int)g->pointer.x, (int)g->pointer.y);
    if (target < 0) {
        /* Outside the grid — the host resolves what container is there. */
        if (!g->external) {
            if (grid_point_in_visible_bounds(g, g->pointer)) return_dragged_item(g);
            else                                             emit(g, ITEM_SLOT_GRID_EVENT_DROP_OUT, -1);
        }
    } else if (g->external) {
        emit(g, ITEM_SLOT_GRID_EVENT_DROP_IN, target);
    } else if (target != g->drag_index) {
        emit(g, '\0' != g->cells[target].item_id[0] ? ITEM_SLOT_GRID_EVENT_SWAP
                                                    : ITEM_SLOT_GRID_EVENT_MOVE,
             target);
    } else {
        return_dragged_item(g);
    }
    end_gesture(g);
}

bool item_slot_grid_take_event(ItemSlotGrid* g, ItemSlotGridEvent* out) {
    if (!g || ITEM_SLOT_GRID_EVENT_NONE == g->pending.type) return false;
    if (out) *out = g->pending;
    g->pending.type = ITEM_SLOT_GRID_EVENT_NONE;
    return true;
}

bool item_slot_grid_is_dragging(const ItemSlotGrid* g) {
    return g && g->dragging;
}

/* ── Draw ─────────────────────────────────────────────────────────────── */

/* Where an occupant is drawn while its settle animation plays: eased from the
 * point it came from toward its own cell. Hit-testing always uses the final
 * rect, so a mid-flight item is still grabbed where it will land. */
static Rectangle settled_rect(const ItemSlotGrid* g, int index, Rectangle cell) {
    float t = g->settle_age[index] / GRID_SETTLE_DUR;
    if (t >= 1.0f) return cell;
    float u = 1.0f - t;
    float e = 1.0f - u * u * u; /* ease-out cubic */
    return (Rectangle){ g->settle_from[index].x + (cell.x - g->settle_from[index].x) * e,
                        g->settle_from[index].y + (cell.y - g->settle_from[index].y) * e,
                        cell.width, cell.height };
}

void item_slot_grid_draw(const ItemSlotGrid* g, ObjectLayersManager* mgr) {
    if (!g || g->capacity < 1 || g->cols < 1) return;

    for (int i = 0; i < g->capacity; i++) {
        Rectangle cell = item_slot_grid_cell_rect(g, i);
        bool source = g->dragging && !g->external && i == g->drag_index;
        bool hovered = g->dragging && i == g->hover_index;
        bool settling = '\0' != g->cells[i].item_id[0] &&
                        GRID_SETTLE_DUR > g->settle_age[i];

        if ('\0' == g->cells[i].item_id[0] || source || settling) {
            /* Draw a well while a cell is empty, lifted, or moving. */
            DrawRectangleRec(cell, BLACK);
            Rectangle inner = { cell.x + 2.0f, cell.y + 2.0f,
                                cell.width - 4.0f, cell.height - 4.0f };
            DrawRectangleRec(inner, GRID_CELL_EMPTY);
            DrawRectangleLinesEx(inner, 1.0f, GRID_CELL_BORDER);
        } else {
            item_slot_draw_ex(settled_rect(g, i, cell), &g->cells[i], mgr,
                              WHITE, 0.0f, true);
        }

        /* Preview: green marks a free landing, amber that the occupant would
         * be displaced, so the outcome reads before the player commits. */
        if (hovered) {
            bool occupied = '\0' != g->cells[i].item_id[0] && !source;
            DrawRectangleLinesEx(cell, 3.0f, occupied ? GRID_HOVER_SWAP : GRID_HOVER_FREE);
        }
    }

    /* Draw moving occupants above settled cells so later indices cannot cover them. */
    for (int i = 0; i < g->capacity; i++) {
        bool source = g->dragging && !g->external && i == g->drag_index;
        if (source || '\0' == g->cells[i].item_id[0] ||
            GRID_SETTLE_DUR <= g->settle_age[i]) continue;
        Rectangle cell = item_slot_grid_cell_rect(g, i);
        item_slot_draw_ex(settled_rect(g, i, cell), &g->cells[i], mgr,
                          WHITE, 0.0f, true);
    }
}

void item_slot_grid_draw_drag(const ItemSlotGrid* g, ObjectLayersManager* mgr) {
    if (!g || !g->dragging || '\0' == g->drag_item.item_id[0]) return;
    float size = g->cell_size;
    Rectangle ghost = { g->pointer.x - size * 0.5f, g->pointer.y - size * 0.5f, size, size };
    item_slot_draw_ex(ghost, &g->drag_item, mgr, GRID_HOVER_FREE, 0.25f, true);
}
