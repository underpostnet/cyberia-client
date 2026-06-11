#include "ui_dispatch.h"

#include <raylib.h>

#include "interaction_bubble.h"
#include "inventory_bar.h"
#include "inventory_modal.h"
#include "modal_dialogue.h"
#include "modal_interact.h"
#include "modal_notification.h"
#include "quest_journal.h"
#include "game_render.h"
#include "js/interact_bridge.h"

bool ui_dispatch_tap(int x, int y) {
    /* Notification OK button has highest priority while visible. */
    if (modal_notification_handle_click(x, y)) return true;

    /* Dialogue modal claims everything while open. */
    if (modal_dialogue_handle_click(x, y)) return true;

    /* Intermediate interaction modal sits between dialogue and the JS
     * overlay — the general-purpose entry point opened by a bubble tap. */
    if (modal_interact_handle_click(x, y)) return true;

    /* JS interact overlay handles its own DOM clicks; while open the world
     * never receives the tap. */
    if (js_interact_overlay_is_open()) return true;

    if (inventory_modal_handle_click(x, y)) return true;

    /* Quest Journal (right side) before the bubble column (left side). */
    if (quest_journal_handle_click(x, y)) return true;

    if (interaction_bubble_handle_click(x, y)) return true;

    int bar_hit = inventory_bar_get_tapped_slot(x, y);
    if (bar_hit >= 0) {
        inventory_modal_open(bar_hit);
        return true;
    }

    int zoom_hit = game_render_zoom_btn_hit(x, y);
    if (zoom_hit != 0) {
        extern void camera_zoom_by(float factor);
        camera_zoom_by((zoom_hit > 0) ? 1.1f : 0.9f);
        return true;
    }

    return false;
}

bool ui_dispatch_covers_point(int x, int y) {
    if (modal_dialogue_is_open())   return true;
    if (modal_interact_is_open())   return true;
    if (js_interact_overlay_is_open()) return true;
    if (inventory_modal_is_open())  return true;
    /* Bubble column only blocks taps while expanded (its own predicate
     * accounts for collapse state and the always-present toggle tab). */
    if (interaction_bubble_point_covered(x, y)) return true;
    if (y > GetScreenHeight() - INV_BAR_HEIGHT) return true;
    if (0 != game_render_zoom_btn_hit(x, y))    return true;
    return false;
}
