#include "ui/hud_side_stack.h"
#include "ui/hud_minimap_overlay.h"

#include <assert.h>
#include <math.h>
#include <raylib.h>
#include <stdbool.h>
#include <stdio.h>

static int s_width, s_height;
static bool s_map, s_journal, s_chat, s_social;

int   GetScreenWidth(void) { return s_width; }
int   GetScreenHeight(void) { return s_height; }
float toolbar_height(void) { return 48.0f; }
float inventory_bar_visible_height(void) { return 76.0f; }
bool  hud_minimap_overlay_is_visible(void) { return s_map; }
bool  quest_journal_is_visible(void) { return s_journal; }
bool  chat_panel_is_visible(void) { return s_chat; }
bool  social_panel_is_visible(void) { return s_social; }

static bool near(float a, float b) { return 0.5f > fabsf(a - b); }

static void show(int width, int height, bool map, bool journal, bool chat, bool social) {
    s_width = width;
    s_height = height;
    s_map = map;
    s_journal = journal;
    s_chat = chat;
    s_social = social;
}

static float bottom(Rectangle r) { return r.y + r.height; }

int main(void) {
    float column_bottom = 800.0f - 76.0f - HUD_SIDE_STACK_MARGIN;

    /* The social panel takes the whole column and covers every other panel. */
    show(1280, 800, true, true, true, true);
    Rectangle social = hud_side_stack_social_slot();
    assert(near(social.y, 48.0f) && near(bottom(social), column_bottom));
    assert(near(social.x, 1280.0f - HUD_SIDE_STACK_W) && near(social.width, HUD_SIDE_STACK_W));
    assert(0.0f == hud_side_stack_map_slot().height && 0.0f == hud_side_stack_journal_slot(200.0f).height);
    assert(0.0f == hud_side_stack_chat_slot().height);

    /* The chat takes the whole column too, over the map and the journal. */
    show(844, 390, true, true, true, false);
    Rectangle chat = hud_side_stack_chat_slot();
    assert(near(chat.y, 48.0f) && near(chat.height, 390.0f - 48.0f - 76.0f - HUD_SIDE_STACK_MARGIN));
    assert(0.0f == hud_side_stack_map_slot().height && 0.0f == hud_side_stack_journal_slot(200.0f).height);
    assert(0.0f == hud_side_stack_social_slot().height);

    /* The map and the journal share the column: the full map, the journal under it. */
    show(1280, 800, true, true, false, false);
    Rectangle map = hud_side_stack_map_slot();
    Rectangle journal = hud_side_stack_journal_slot(300.0f);
    assert(near(map.y, 48.0f) && near(map.height, HUD_MINIMAP_OVERLAY_SIZE));
    assert(near(journal.y, bottom(map) + HUD_SIDE_STACK_GAP) && near(journal.height, 300.0f));
    assert(near(bottom(hud_side_stack_journal_slot(800.0f)), column_bottom));

    /* A short column keeps the journal its room: the map gives way down to half of the column. */
    show(844, 390, true, true, false, false);
    map = hud_side_stack_map_slot();
    journal = hud_side_stack_journal_slot(400.0f);
    assert(HUD_SIDE_STACK_MAP_MIN <= map.height && map.height < HUD_MINIMAP_OVERLAY_SIZE);
    assert(near(bottom(journal), 390.0f - 76.0f - HUD_SIDE_STACK_MARGIN));

    puts("hud_side_stack_test OK");
    return 0;
}
