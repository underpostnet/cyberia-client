#include "render.h"
#include "fx/fx_death.h"
#include "fx/fx_heal_aura.h"
#include "fx/fx_hit_burst.h"
#include "fx/fx_level_up.h"
#include "fx/fx_portal_aura.h"
#include "fx/fx_obstacle_burst.h"
#include "ui/text.h"

#include "data/dialogue_data.h"
#include "camera.h"
#include "overhead_occlusion.h"
#include "game_render.h"
#include "world/object_layers_management.h"
#include "ui/dev_ui.h"
#include "ui/floating_combat_text.h"
#include "ui/interaction_bubble.h"
#include "fx/fx_assemble.h"
#include "fx/fx_grant_delivery.h"
#include "fx/loot_fx.h"
#include "fx/fx_reward.h"
#include "ui/hud_minimap_overlay.h"
#include "ui/inventory_bar.h"
#include "ui/inventory_modal.h"
#include "ui/modal_dialogue.h"
#include "ui/modal_instance_map.h"
#include "ui/modal_interact.h"
#include "ui/modal_map.h"
#include "ui/quest_journal.h"
#include "ui/chat_panel.h"
#include "ui/modal_notification.h"
#include "fx/fx_dust.h"
#include "fx/fx_footsteps.h"
#include "fx/fx_tap.h"
#include "ui/ui_icon.h"
#include "data/engine_client.h"
#include "log.h"

#include <assert.h>
#include <raylib.h>
#include <emscripten/html5.h>

/* render.c — top-level frame orchestrator. Owns nothing; only sequences
 * the camera update, effect timers, UI ticks, and the world-render pass.
 * The Camera2D itself lives in view/camera.{c,h}. */

/* Browser window resize → raylib viewport + camera. raylib's IsWindowResized
 * does not fire reliably under Emscripten, so we drive it from the DOM event. */
static EM_BOOL on_window_resize(int eventType, const EmscriptenUiEvent* uiEvent, void* userData) {
    int w = uiEvent->windowInnerWidth;
    int h = uiEvent->windowInnerHeight;
    SetWindowSize(w, h);
    game_render_set_screen_size(w, h);
    camera_resize(w, h);
    LOG_INFO("window resized %dx%d", w, h);
    return EM_TRUE;
}

void render_init(int width, int height) {
    emscripten_set_resize_callback(EMSCRIPTEN_EVENT_TARGET_WINDOW, NULL, EM_FALSE, on_window_resize);

    game_render_init(width, height);

    dev_ui_init();
    modal_map_init();
    modal_instance_map_init();
    hud_minimap_overlay_init();

    fx_tap_init();
    fx_dust_init();
    fx_footsteps_init();
    loot_fx_reset();
    fx_grant_delivery_init();
    fx_assemble_init();
    fx_reward_init();
}

void render_on_tick(float delta_time) {
    camera_on_tick(delta_time);
    overhead_occlusion_update(delta_time);

    fct_update(delta_time);
    loot_fx_update(delta_time);
    fx_tap_update(delta_time);
    fx_level_up_update(delta_time);
    fx_heal_aura_update(delta_time);
    fx_portal_aura_update(delta_time);
    fx_hit_burst_update(delta_time);
    fx_death_update(delta_time);
    fx_obstacle_burst_update(delta_time);
    /* Reads the render positions main.c settled this frame, so the dust lands
     * with the sprite that raised it. */
    fx_footsteps_update(delta_time);
    fx_dust_update(delta_time);

    inventory_bar_update(delta_time);
    /* After the bar refreshed its quantity FX, so a landed grant is visible to
     * the sequence on the same frame it arrives. */
    fx_grant_delivery_update(delta_time);
    if (inventory_modal_is_open())  inventory_modal_update(delta_time);
    if (modal_dialogue_is_open())   modal_dialogue_update(delta_time);
    if (modal_interact_is_open())   modal_interact_update(delta_time);
    quest_journal_update(delta_time);
    chat_panel_update(delta_time);
    modal_notification_update(delta_time);
    fx_reward_update(delta_time);
    fx_assemble_update(delta_time);

    interaction_bubble_update();
    dev_ui_on_tick();
    modal_map_update(delta_time);
    modal_instance_map_update(delta_time);
    hud_minimap_overlay_update(delta_time);

    game_render_frame();
}

void render_cleanup(void) {
    fx_tap_reset();
    fx_footsteps_reset();
    fx_dust_reset();
    game_render_cleanup();
    dev_ui_cleanup();
    modal_map_cleanup();
    hud_minimap_overlay_cleanup();
    modal_instance_map_cleanup();
}
