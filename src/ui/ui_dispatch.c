#include "ui_dispatch.h"

#include <raylib.h>

#include "interaction_bubble.h"
#include "inventory_bar.h"
#include "inventory_modal.h"
#include "modal_dialogue.h"
#include "modal_instance_map.h"
#include "modal_interact.h"
#include "modal_notification.h"
#include "quest_journal.h"
#include "toolbar.h"
#include "js/interact_bridge.h"

bool ui_dispatch_tap(int x, int y) {
    /* Notification OK button has highest priority while visible. */
    if (modal_notification_handle_click(x, y)) return true;

    /* Toolbar toggles (quest / map / fullscreen) stay live above every
     * modal; the Quest Journal floats above the Instance Map container,
     * which then consumes presses inside its bounds. */
    if (toolbar_handle_click(x, y)) return true;
    if (modal_instance_map_handle_click(x, y)) return true;

    /* Interaction and dialogue modals keep the bottom inventory companion
     * visible. Its toggle remains actionable. While the interact modal is up,
     * bar slots stay live too: tapping one stacks the inventory modal on top
     * and returns to the interact session on close. A standalone dialogue
     * keeps the slots read-only. */
    if (modal_dialogue_is_open() || modal_interact_is_open()) {
        if (inventory_bar_handle_toggle_click(x, y)) return true;
        /* Arms the drag; the slot activates on release via ui_on_tick. The bar
         * is live for the interact modal and for an inventory-lore dialogue
         * (opened from the inventory modal) so a slot can switch the chain. */
        if ((modal_interact_is_open() || modal_dialogue_is_item_lore()) &&
            inventory_bar_handle_click(x, y, NULL)) return true;
        if (inventory_bar_point_covered(x, y)) return true;
    }

    /* Dialogue modal claims everything while open. */
    if (modal_dialogue_handle_click(x, y)) return true;

    /* Intermediate interaction modal sits between dialogue and the JS
     * overlay — the general-purpose entry point opened by a bubble tap. */
    if (modal_interact_handle_click(x, y)) return true;

    /* JS interact overlay handles its own DOM clicks; while open the world
     * never receives the tap. */
    if (js_interact_overlay_is_open()) return true;

    /* While the inventory modal is open the bar stays live underneath it: a
     * slot press arms the switch (activated on release in ui_on_tick), taking
     * precedence over the modal's tap-outside-to-close. Bar hit-tests miss the
     * modal card (which sits above the bar), so card taps still fall through. */
    if (inventory_modal_is_open()) {
        if (inventory_bar_handle_toggle_click(x, y)) return true;
        if (inventory_bar_handle_click(x, y, NULL)) return true;
        /* A tap anywhere on the bar strip stays with the bar, not the modal's
         * close-on-outside-tap. */
        if (inventory_bar_point_covered(x, y)) return true;
    }

    if (inventory_modal_handle_click(x, y)) return true;

    /* Quest Journal (right side) before the bubble column (left side). */
    if (quest_journal_handle_click(x, y)) return true;

    /* Interaction bubbles are blocked whenever a modal is open on top of them
     * (interact / inventory / dialogue / instance map). A click within the
     * bubble column area behind a modal must never open or change a bubble. */
    bool modal_over_bubbles = modal_interact_is_open() || inventory_modal_is_open() ||
                              modal_dialogue_is_open() || modal_instance_map_is_open();
    if (!modal_over_bubbles) {
        if (interaction_bubble_handle_click(x, y)) return true;
    }

    /* Arms the drag; the slot activates on release via ui_on_tick. */
    if (inventory_bar_handle_click(x, y, NULL)) return true;

    return false;
}

bool ui_dispatch_covers_point(int x, int y) {
    if (modal_instance_map_covers_point(x, y)) return true;
    if (modal_dialogue_is_open())   return true;
    if (modal_interact_is_open())   return true;
    if (js_interact_overlay_is_open()) return true;
    if (inventory_modal_is_open())  return true;
    /* Bubble column only blocks taps while expanded (its own predicate
     * accounts for collapse state and the always-present toggle tab). */
    if (interaction_bubble_point_covered(x, y)) return true;
    if (inventory_bar_point_covered(x, y)) return true;
    if (toolbar_covers_point(x, y)) return true;
    return false;
}
