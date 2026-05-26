#include "ui_dispatch.h"

#include <raylib.h>

#include "interaction_bubble.h"
#include "inventory_bar.h"
#include "inventory_modal.h"
#include "modal_dialogue.h"
#include "../game_render.h"
#include "../js/interact_bridge.h"

bool ui_dispatch_tap(int x, int y) {
    /* Highest priority: dialogue modal claims everything while open. */
    if (modal_dialogue_handle_click(x, y)) return true;

    /* JS interact overlay handles its own DOM clicks; while open the world
     * never receives the tap. */
    if (js_interact_overlay_is_open()) return true;

    if (inventory_modal_handle_click(x, y)) return true;

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
    if (js_interact_overlay_is_open()) return true;
    if (inventory_modal_is_open())  return true;
    if (interaction_bubble_slot_count() > 0 &&
        x < IBUBBLE_MARGIN_X + IBUBBLE_ICON_SIZE + IBUBBLE_MARGIN_X) {
        return true;
    }
    if (y > GetScreenHeight() - INV_BAR_HEIGHT) return true;
    if (0 != game_render_zoom_btn_hit(x, y))    return true;
    return false;
}
