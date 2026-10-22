/* The objectives of the Quest Journal: each one shows the count the server
 * keeps and where it leads in this world. An objective that leads somewhere
 * has a LOCATE button with a tap band at least QJ_LOCATE_BAND tall, which
 * follows it and opens the map on it; a done or an absent one has none. The
 * followed one reads FOLLOWING and keeps the destination the player picked.
 * The journal holds the world map only while it shows. */

#include "ui/quest_journal.c"

#include "data/engine_client.h"
#include "sim/game_state.h"
#include "sim/local_player.h"
#include "utils.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

GameState g_game_state;
LocalPlayer g_local_player;
const Color MODAL_TEXT = { 220, 220, 230, 240 }, MODAL_SELECTED = { 1, 0, 0, 0 }, MODAL_BUTTON = { 2, 0, 0, 0 },
            MODAL_QUEST = { 3, 0, 0, 0 };

static FetchCompletedCb s_callback;
static char s_asset_id[64];
static int s_fetches, s_locates, s_lines;
void fetch_request_start(const char* id, const char* url, FetchCompletedCb callback) {
    copy_str(s_asset_id, sizeof s_asset_id, id);
    s_callback = callback;
    s_fetches++;
}
void fetch_forget(const char* url) {}
void fetch_data_release(void* data) {}
const QuestMetadataEntry* quest_cache_get(const char* code) { return NULL; }
void audio_event(const char* logic_event_id) {}
void modal_instance_map_locate(void) { s_locates++; }
void ui_toggle_init(UIToggle* t, Rectangle anchor, bool initial_expanded, UIToggleChevron chevron) {}
void ui_scroll_reset(UIScroll* s) {}
int text_wrap(const char* text, int x, int y, int maxw, int size, Color col, bool center, bool draw) {
    s_lines++;
    return size + 2;
}
int GetMouseX(void) { return -1; }
int GetMouseY(void) { return -1; }
/* The LOCATE buttons of the last draw. */
static int s_buttons;
static Rectangle s_button;
static UIButtonPixelRetroStyle s_style;
void ui_button_pixel_retro_draw(Rectangle bounds, const UIButtonPixelRetroStyle* style, bool hovered) {
    s_buttons++;
    s_button = bounds;
    s_style = *style;
}
bool ui_button_hit(Rectangle r, int x, int y) { return r.x <= x && r.x + r.width > x && r.y <= y && r.y + r.height > y; }
Color Fade(Color color, float alpha) { color.a = (unsigned char)(color.a * alpha); return color; }

static const char* WORLD =
    "{\"status\":\"success\",\"data\":{\"instanceCode\":\"test\",\"name\":\"Test World\",\"nodes\":["
    "{\"mapCode\":\"a\",\"name\":\"Gate\",\"w\":0.5,\"h\":0.5,\"gridX\":16,\"gridY\":16,\"position\":{\"x\":0,\"y\":0}}],"
    "\"edges\":[],\"presencePois\":["
    "{\"mapCode\":\"a\",\"cellX\":3,\"cellY\":4,\"presenceStatus\":\"hostile\",\"bots\":[{\"skin\":\"wolf\",\"mortal\":true}]},"
    "{\"mapCode\":\"a\",\"cellX\":8,\"cellY\":9,\"presenceStatus\":\"hostile\",\"bots\":[{\"skin\":\"wolf\",\"mortal\":true}]}]}}";

int main(void) {
    copy_str(g_game_state.instance_code, sizeof g_game_state.instance_code, "test");
    copy_str(g_local_player.map_code, sizeof g_local_player.map_code, "a");
    QuestSnapshot hunt = { .code = "q", .status = "active", .active_step = "Hunt", .step_id = "s", .objective_count = 3,
                           .objectives = { { .type = QUEST_OBJECTIVE_KILL, .item_id = "wolf", .current = 1, .required = 3 },
                                           { .type = QUEST_OBJECTIVE_TALK, .item_id = "ghost", .required = 1 },
                                           { .type = QUEST_OBJECTIVE_COLLECT, .item_id = "pelt", .current = 2,
                                             .required = 2 } } };
    quest_progress_store_upsert(&hunt);
    const QuestProgressEntry* e = quest_progress_store_find("q");

    /* Showing the journal holds the world map; hiding it lets go. */
    quest_journal_toggle();
    assert(1 == s_fetches && IMAP_DATA_LOADING == instance_map_data_state());
    quest_target_update();
    char where[128];
    destination_text(quest_target_find("q", "s", QUEST_OBJECTIVE_KILL, "wolf"), where, sizeof where);
    assert(0 == strcmp("Loading the world map...", where));
    FetchResponse response = { .asset_id = s_asset_id, .success = true, .data = (void*)WORLD, .size = strlen(WORLD) };
    s_callback(&response);
    quest_target_update();

    /* Where each objective leads: the nearest of its places, or why it leads nowhere. */
    const QuestTarget* wolf = quest_target_find("q", "s", QUEST_OBJECTIVE_KILL, "wolf");
    destination_text(wolf, where, sizeof where);
    assert(0 == strcmp("2 places - nearest Gate (3, 4)", where));
    destination_text(quest_target_find("q", "s", QUEST_OBJECTIVE_TALK, "ghost"), where, sizeof where);
    assert(0 == strcmp("Not in this world", where));

    /* Three objectives, two lines each but the done one; one LOCATE button with its icon and its word, centred
     * in a band as tall as QJ_LOCATE_BAND. */
    s_lines = s_buttons = 0;
    int height = objectives_layout(JW_DRAW, 0, 0, e, 10, 100, 260);
    assert(5 == s_lines && QJ_LOCATE_BAND + 3 * (QJ_FONT_SMALL + 2) == height && 1 == s_buttons);
    assert(0 == strcmp("LOCATE", s_style.label) && 0 == strcmp("target", s_style.icon_id) && !s_style.selected);
    assert(44 <= QJ_LOCATE_BAND && 44 <= QJ_LOCATE_W && 100.0f + (QJ_LOCATE_BAND - QJ_LOCATE_H) / 2 == s_button.y);
    /* The whole band takes the tap, from its top to its bottom; a tap beside it does nothing. */
    objectives_layout(JW_CLICK, 10 + 260 - QJ_LOCATE_W - 2, 105, e, 10, 100, 260);
    assert(0 == s_locates && NULL == quest_target_tracked());
    objectives_layout(JW_CLICK, 10 + 260 - QJ_LOCATE_W / 2, 100 + QJ_LOCATE_BAND - 1, e, 10, 100, 260);
    assert(1 == s_locates && wolf == quest_target_tracked() && 0 == quest_target_tracked_destination());
    /* The followed objective reads FOLLOWING; a tap shows the destination the player picked, never the nearest. */
    quest_target_track(wolf, 1);
    objectives_layout(JW_DRAW, 0, 0, e, 10, 100, 260);
    assert(0 == strcmp("FOLLOWING", s_style.label) && s_style.selected);
    objectives_layout(JW_CLICK, 10 + 260 - QJ_LOCATE_W / 2, 100, e, 10, 100, 260);
    assert(2 == s_locates && wolf == quest_target_tracked() && 1 == quest_target_tracked_destination());

    quest_journal_toggle();
    assert(IMAP_DATA_IDLE == instance_map_data_state());
    puts("quest_journal_test OK");
    return 0;
}
