#include "hud_side_stack.h"

#include "chat_panel.h"
#include "hud_minimap_overlay.h"
#include "inventory_bar.h"
#include "quest_journal.h"
#include "social_panel.h"
#include "toolbar.h"

#include <raylib.h>

static float column_x(void) {
    return (float)GetScreenWidth() - HUD_SIDE_STACK_W;
}

static float hud_side_stack_available(void) {
    float bottom = (float)GetScreenHeight() - inventory_bar_visible_height() - HUD_SIDE_STACK_MARGIN;
    float avail = bottom - toolbar_height();
    return 0.0f < avail ? avail : 0.0f;
}

/* The chat or the social panel shows: it takes the whole column. */
static bool column_covered(void) { return chat_panel_is_visible() || social_panel_is_visible(); }

static Rectangle column_slot(bool shown) {
    return (Rectangle){ column_x(), toolbar_height(), HUD_SIDE_STACK_W, shown ? hud_side_stack_available() : 0.0f };
}

static float map_height(void) {
    if (!hud_minimap_overlay_is_visible() || column_covered()) return 0.0f;
    float avail = hud_side_stack_available();
    float h = HUD_MINIMAP_OVERLAY_SIZE < avail ? HUD_MINIMAP_OVERLAY_SIZE : avail;
    if (quest_journal_is_visible()) {
        float shared = avail - HUD_SIDE_STACK_JOURNAL_RESERVE - HUD_SIDE_STACK_GAP;
        float floor = avail * 0.5f;
        float cap = shared > floor ? shared : floor;
        if (h > cap) h = cap;
    }
    return HUD_SIDE_STACK_MAP_MIN > h ? 0.0f : h;
}

Rectangle hud_side_stack_map_slot(void) {
    return (Rectangle){ column_x(), toolbar_height(), HUD_SIDE_STACK_W, map_height() };
}

Rectangle hud_side_stack_journal_slot(float desired_h) {
    float top = toolbar_height();
    float room = hud_side_stack_available();
    float map_h = map_height();
    if (0.0f < map_h) {
        top += map_h + HUD_SIDE_STACK_GAP;
        room -= map_h + HUD_SIDE_STACK_GAP;
    }
    if (!quest_journal_is_visible() || column_covered() || 0.0f >= room)
        return (Rectangle){ column_x(), top, HUD_SIDE_STACK_W, 0.0f };
    return (Rectangle){ column_x(), top, HUD_SIDE_STACK_W, desired_h < room ? desired_h : room };
}

Rectangle hud_side_stack_chat_slot(void) {
    return column_slot(chat_panel_is_visible() && !social_panel_is_visible());
}

Rectangle hud_side_stack_social_slot(void) { return column_slot(social_panel_is_visible()); }
