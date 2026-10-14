#include "stack_move.h"

#include "audio/audio.h"
#include "audio/audio_events.h"
#include "container_panel.h"
#include "fx/fx_inventory_bar_qty.h"
#include "inventory_bar.h"
#include "modal_notification.h"

#include <raylib.h>
#include <string.h>

static const Color C_ASK = { 120, 200, 140, 255 };

/* The drop the picker asks about. */
static struct {
    bool pending;
    Box  from, to;
    int  from_slot, to_slot;
} s_ask;

static Rectangle place_rect(Box box, int slot) {
    return BOX_BAG == box ? inventory_bar_cell_rect(slot) : container_panel_cell_rect(slot);
}

static void slide_in(Box box, int slot, Rectangle origin) {
    if (BOX_BAG == box) inventory_bar_animate_from(slot, origin);
    else                container_panel_animate_from(slot, origin);
}

/* Move qty units and slide them from `origin` into their slot: a split leaves
 * its stack, a stack joins the stack it lands on, and a swap sends the other
 * stack back the way the first came. */
static void land(Box from, int from_slot, Box to, int to_slot, int qty, Rectangle origin) {
    const ObjectLayerState* stack = container_ops_stack(from, from_slot);
    if (NULL == stack) return;
    MoveKind kind = container_ops_check(from, from_slot, to, to_slot, qty);
    if (MOVE_REFUSED == kind) return;
    /* Units that cross between two surfaces in view move: the bar shows no
     * gain or loss for them. */
    if (from != to) fx_inventory_bar_qty_suppress(stack->item_id);
    Rectangle target = place_rect(to, to_slot);
    container_ops_move(from, from_slot, to, to_slot, qty);
    slide_in(to, to_slot, origin);
    if (MOVE_SWAP == kind) slide_in(from, from_slot, target);
}

static void confirmed(const char* item_id, int qty) {
    if (!s_ask.pending) return;
    s_ask.pending = false;
    /* A snapshot may change the place while the picker asks. */
    const ObjectLayerState* stack = container_ops_stack(s_ask.from, s_ask.from_slot);
    if (NULL == stack || 0 != strcmp(stack->item_id, item_id)) return;
    /* The picker hid the drag, so the units part from their stack. */
    land(s_ask.from, s_ask.from_slot, s_ask.to, s_ask.to_slot, qty, place_rect(s_ask.from, s_ask.from_slot));
}

static void declined(void) { s_ask.pending = false; }

bool stack_move_drop(Box from, int from_slot, Box to, int to_slot, Rectangle origin) {
    const ObjectLayerState* stack = container_ops_stack(from, from_slot);
    if (NULL == stack) return false;
    MoveKind kind = container_ops_check(from, from_slot, to, to_slot, stack->quantity);
    if (MOVE_REFUSED == kind) return false;
    audio_event(AUDIO_EVENT_ITEM_PICKUP);
    bool split = MOVE_SWAP != kind && 1 < stack->quantity &&
                 MOVE_REFUSED != container_ops_check(from, from_slot, to, to_slot, 1);
    if (!split) {
        land(from, from_slot, to, to_slot, stack->quantity, origin);
        return true;
    }
    s_ask.pending   = true;
    s_ask.from      = from;
    s_ask.from_slot = from_slot;
    s_ask.to        = to;
    s_ask.to_slot   = to_slot;
    const char* prompt = "How many to move?";
    const char* label  = "Move";
    const char* icon   = "hand";
    if (MOVE_STACK == kind) {
        prompt = "How many to stack here?";
        label  = "Stack";
        icon   = "stack";
    } else if (BOX_BAG == from && BOX_BAG != to) {
        prompt = "How many to store?";
        label  = "Store";
        icon   = "bag";
    } else if (BOX_BAG != from && BOX_BAG == to) {
        prompt = "How many to take?";
        label  = "Take";
    }
    modal_notification_show_split(stack->item_id, prompt, C_ASK, stack->item_id, stack->quantity,
                                  label, icon, confirmed, declined);
    return true;
}
