#ifndef CYBERIA_UI_ITEM_SLOT_GRID_H
#define CYBERIA_UI_ITEM_SLOT_GRID_H

#include "object_layer.h"
#include "object_layers_management.h"

#include <raylib.h>
#include <stdbool.h>

/* item_slot_grid — reusable wrapping grid of item_slot cells with drag-and-drop.
 *
 * A container orchestrator: it owns layout, hit-testing and the drag gesture,
 * and delegates every cell's pixels to item_slot. Nothing here knows what a
 * storage vault, a bank, or an equipment sheet is — the host supplies the cell
 * contents and answers the drop by mutating its own model.
 *
 * Addressing: a cell is its linear index in 0..capacity-1. Cells flow inline
 * and wrap to the next row when the width runs out, so the column count follows
 * the area the host gives it and the grid is rarely square. Row and column are
 * therefore presentation only — an index is the sole stable identity, and the
 * one a host may put on the wire.
 *
 * Drag model:
 *   - A press on a filled cell arms a drag; it becomes one past a small slop,
 *     so a tap still reaches the host as a click.
 *   - While dragging, the cell under the pointer is previewed: the host is told
 *     which index would receive the drop and whether that would displace an
 *     occupant, so the grid can shift/highlight before the player commits.
 *   - Releasing over another cell emits a drop; releasing outside the grid
 *     emits an external drop with the release point, which the host maps to
 *     whatever container is there (the inventory bar).
 *   - An external container can hand a drag in through
 *     item_slot_grid_begin_external_drag, giving bi-directional transfer.
 *
 * Per-frame contract:
 *   item_slot_grid_update(g, dt);            // advance the gesture
 *   ... drain events with item_slot_grid_take_event ...
 *   item_slot_grid_draw(g, mgr);             // cells only
 *   item_slot_grid_draw_drag(g, mgr);        // the floating item, drawn last
 * and route presses in through item_slot_grid_handle_press. */

#define ITEM_SLOT_GRID_MAX_SLOTS 64
/* A cell never grows past this, however wide the host's area is; surplus width
 * becomes more columns instead. */
#define ITEM_SLOT_GRID_CELL_MAX  100.0f

typedef enum {
    ITEM_SLOT_GRID_EVENT_NONE = 0,
    /* Dropped on an empty cell of the same grid. */
    ITEM_SLOT_GRID_EVENT_MOVE,
    /* Dropped on an occupied cell of the same grid. */
    ITEM_SLOT_GRID_EVENT_SWAP,
    /* Dropped outside the grid — the host decides what is under `point`. */
    ITEM_SLOT_GRID_EVENT_DROP_OUT,
    /* An external drag was released over a cell of this grid. */
    ITEM_SLOT_GRID_EVENT_DROP_IN,
    /* A press that never became a drag. */
    ITEM_SLOT_GRID_EVENT_TAP,
} ItemSlotGridEventType;

typedef struct {
    ItemSlotGridEventType type;
    int     from_index;  /* source cell; -1 for an external drag        */
    int     to_index;    /* target cell; -1 when released outside       */
    Vector2 point;       /* release position, screen pixels             */
    ObjectLayerState payload; /* the dragged item                       */
} ItemSlotGridEvent;

typedef struct {
    /* Layout — `cols` and the row count follow from the last layout area. */
    int       capacity;
    int       cols;
    Rectangle bounds;     /* captured by the last layout                 */
    float     cell_size;
    float     gap;

    /* Viewport a scrolling host clips the grid to. Cells the player cannot
     * see are not hit-testable, so a drag released past the panel edge falls
     * through to whatever is really there. Empty means unclipped. */
    Rectangle clip;

    /* Cell contents, inline order. An empty cell has an empty item_id. */
    ObjectLayerState cells[ITEM_SLOT_GRID_MAX_SLOTS];

    /* Drag state. */
    bool    pressed;        /* pointer down inside a cell                */
    bool    dragging;       /* gesture passed the slop                   */
    bool    external;       /* the drag originated outside this grid     */
    int     drag_index;     /* source cell, -1 while external            */
    Vector2 press_point;
    Vector2 pointer;
    ObjectLayerState drag_item;

    /* Live preview: the cell the drop would land on, -1 when none. */
    int hover_index;

    /* Per-cell settle animation: an occupant that just arrived slides in from
     * where it came, so a reorder or a deposit reads as movement. */
    float   settle_age[ITEM_SLOT_GRID_MAX_SLOTS];
    Vector2 settle_from[ITEM_SLOT_GRID_MAX_SLOTS];

    ItemSlotGridEvent pending;
} ItemSlotGrid;

/* Size the grid to `capacity` cells. Contents are cleared. */
void item_slot_grid_init(ItemSlotGrid* g, int capacity);

/* Replace the cell at `index`. A NULL or empty `ols` clears it. */
void item_slot_grid_set(ItemSlotGrid* g, int index, const ObjectLayerState* ols);

/* Drop every cell's contents (a fresh authoritative snapshot follows). */
void item_slot_grid_clear(ItemSlotGrid* g);

/* Flow the cells across the width of `area`, wrapping into as many rows as the
 * capacity needs. Call before drawing or hit-testing so the two agree. */
void item_slot_grid_layout(ItemSlotGrid* g, Rectangle area);

/* Total height the laid-out grid occupies — what a scrolling host measures. */
float item_slot_grid_height(const ItemSlotGrid* g);

/* Restrict hit-testing to the viewport the host clips drawing to. */
void item_slot_grid_set_clip(ItemSlotGrid* g, Rectangle clip);

/* Route a press. Returns true when it landed inside a visible cell. */
bool item_slot_grid_handle_press(ItemSlotGrid* g, int mx, int my);

/* Adopt a drag started by another container, so its item can be dropped into
 * a cell. `point` is the current pointer position. */
void item_slot_grid_begin_external_drag(ItemSlotGrid* g, const ObjectLayerState* ols,
                                        Vector2 point);

/* Advance the gesture; emits at most one event per frame. */
void item_slot_grid_update(ItemSlotGrid* g, float dt);

/* One-shot: take the event produced this frame, if any. */
bool item_slot_grid_take_event(ItemSlotGrid* g, ItemSlotGridEvent* out);

/* True while a drag is in flight — hosts suppress their own scroll/tap paths. */
bool item_slot_grid_is_dragging(const ItemSlotGrid* g);

/* Cell rect for an index, in the coordinates of the last layout. */
Rectangle item_slot_grid_cell_rect(const ItemSlotGrid* g, int index);

/* Index of the visible cell under a screen point, or -1. */
int item_slot_grid_index_at(const ItemSlotGrid* g, int mx, int my);

/* Slide an occupant into `index` from an arbitrary screen point — where a
 * deposit was released, say. Call after applying the change. */
void item_slot_grid_animate_from_point(ItemSlotGrid* g, int index, Vector2 origin);

/* Slide an occupant that moved between two cells of this grid. */
void item_slot_grid_animate_move(ItemSlotGrid* g, int from_index, int to_index);

/* Cells only. The dragged item is NOT drawn here — see
 * item_slot_grid_draw_drag. */
void item_slot_grid_draw(const ItemSlotGrid* g, ObjectLayersManager* mgr);

/* The floating dragged item, drawn separately so the host can layer it above
 * every other surface (the inventory bar included) rather than inside its own
 * panel. No-op when nothing is in hand. */
void item_slot_grid_draw_drag(const ItemSlotGrid* g, ObjectLayersManager* mgr);

#endif /* CYBERIA_UI_ITEM_SLOT_GRID_H */
