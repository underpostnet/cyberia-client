/* The detail card of the Instance Map: a portal names where it leads and how;
 * a place lists its quests and actions, and a row opens one; a quest shows its
 * state, needs, metadata and unlocks; a bot or a resource shows its class and
 * place; a map counts what it places and names the entry spawn, which names a
 * cell only when it is fixed; only a quest card reads quest details; the
 * followed objective lists its destinations nearest first; a quest target and
 * its map name the objectives they hold; every icon keeps its own colours. */

#include "ui/map_detail.c"

#include "data/engine_client.h"
#include "sim/game_state.h"
#include "sim/local_player.h"

#include <assert.h>
#include <raylib.h>
#define RAYMATH_IMPLEMENTATION
#include <raymath.h>
#include <stdio.h>
#include <string.h>

GameState g_game_state;
LocalPlayer g_local_player;
const Color MODAL_TEXT = { 220, 220, 230, 240 }, MODAL_LABEL = { 150, 160, 190, 220 }, MODAL_POSITIVE = { 0 },
            MODAL_ACTION = { 0 }, MODAL_QUEST = { 0 }, MODAL_BUTTON = { 0 }, MODAL_PANEL_BORDER = { 0 },
            MAP_MARKER_SPAWN = { 70, 230, 120, 255 }, MAP_MARKER_ROUTE = { 255, 190, 90, 255 };

static FetchCompletedCb s_callback;
static char s_asset_id[64];
static QuestMetadataEntry s_meta;
static int s_icons, s_tinted, s_glyphs, s_quest_reads;
static ImapPresenceStatus s_glyph;

void fetch_request_start(const char* id, const char* url, FetchCompletedCb callback) {
    copy_str(s_asset_id, sizeof s_asset_id, id);
    s_callback = callback;
}
void fetch_data_release(void* data) {}
void fetch_forget(const char* url) {}
void audio_event(const char* logic_event_id) {}
const QuestMetadataEntry* quest_cache_get(const char* code) {
    s_quest_reads++;
    return '\0' != s_meta.title[0] ? &s_meta : NULL;
}
void map_marker_draw_presence(Vector2 at, float side, ImapPresenceStatus status, int count, float alpha) {
    s_glyphs++;
    s_glyph = status;
}
const char* presentation_runtime_status_icon(uint8_t status_id) { return "portal"; }
Vector2 graph_layout_pack(const Vector2* cells, const Vector2* sizes, int count, float gap, Vector2* centers) {
    return (Vector2){ 0 };
}
void modal_draw_card(Rectangle card, bool anchored, float age, float overlay_h, Color border, float header_h) {}
void modal_draw_clipped_text(const char* text, int x, int y, int max_w, int font_size, Color color) {}
void ui_icon_draw_ex(const char* icon_id, float cx, float cy, float size, float rotation_deg, Color tint) {
    s_icons++;
    s_tinted += 255 != tint.r || 255 != tint.g || 255 != tint.b;
}
void DrawRectangleRec(Rectangle rect, Color color) {}
void text_draw_compat(const char* text, int x, int y, int size, Color color) {}
int text_measure_compat(const char* text, int size) { return (int)strlen(text) * size / 2; }
Vector2 GetMousePosition(void) { return (Vector2){ -1, -1 }; }
bool CheckCollisionPointRec(Vector2 p, Rectangle r) {
    return p.x >= r.x && p.x < r.x + r.width && p.y >= r.y && p.y < r.y + r.height;
}
Color Fade(Color color, float alpha) { color.a = (unsigned char)(color.a * alpha); return color; }

static const char* WORLD =
    "{\"status\":\"success\",\"data\":{\"instanceCode\":\"test\","
    "\"entrySpawn\":{\"mode\":\"fixed\",\"mapCode\":\"a\",\"mapName\":\"A\",\"cellX\":4,\"cellY\":5},\"nodes\":["
    "{\"mapCode\":\"a\",\"name\":\"A\",\"w\":0.5,\"h\":0.5,\"gridX\":16,\"gridY\":16,\"position\":{\"x\":-1,\"y\":0},"
    "\"placed\":{\"hostile\":1,\"passive\":2,\"resource\":3}},"
    "{\"mapCode\":\"b\",\"name\":\"B\",\"w\":0.5,\"h\":0.5,\"gridX\":16,\"gridY\":16,\"position\":{\"x\":1,\"y\":0}}],"
    "\"edges\":["
    "{\"sourceMapCode\":\"a\",\"sourceCellX\":1,\"sourceCellY\":2,\"targetMapCode\":\"b\",\"targetCellX\":3,\"targetCellY\":4},"
    "{\"sourceMapCode\":\"a\",\"sourceCellX\":5,\"sourceCellY\":6,\"targetMapCode\":\"b\"},"
    "{\"sourceMapCode\":\"b\",\"sourceCellX\":7,\"sourceCellY\":8,\"targetMapCode\":\"b\",\"targetCellX\":5,\"targetCellY\":6},"
    "{\"sourceMapCode\":\"b\",\"sourceCellX\":3,\"sourceCellY\":4,\"targetMapCode\":\"a\",\"targetCellX\":1,\"targetCellY\":2}],"
    "\"presencePois\":["
    "{\"mapCode\":\"a\",\"cellX\":1,\"cellY\":1,\"presenceStatus\":\"passive\",\"capabilities\":[\"action\",\"quest\"],"
    "\"quests\":[{\"code\":\"q1\",\"title\":\"First\",\"prerequisiteCodes\":[]},"
    "{\"code\":\"q2\",\"title\":\"Second\",\"prerequisiteCodes\":[\"q1\"]}],"
    "\"actions\":[{\"code\":\"vendor\",\"label\":\"Vendor\",\"standalone\":true,\"questCodes\":[\"q2\"],"
    "\"services\":[\"shop\",\"market\",\"talk\"]}],\"yields\":[{\"itemId\":\"potion\",\"via\":\"shop\"}]},"
    "{\"mapCode\":\"b\",\"cellX\":2,\"cellY\":2,\"presenceStatus\":\"passive\",\"capabilities\":[\"action\"],"
    "\"actions\":[{\"code\":\"idle\",\"services\":[]}]},"
    "{\"mapCode\":\"b\",\"cellX\":7,\"cellY\":7,\"presenceStatus\":\"hostile\",\"bots\":[{\"skin\":\"wolf\",\"mortal\":true,\"stats\":54}]},"
    "{\"mapCode\":\"b\",\"cellX\":8,\"cellY\":8,\"presenceStatus\":\"hostile\",\"bots\":[{\"skin\":\"wolf\",\"mortal\":true}]},"
    "{\"mapCode\":\"b\",\"cellX\":9,\"cellY\":9,\"presenceStatus\":\"hostile\",\"bots\":[{\"skin\":\"wolf\",\"mortal\":true}]}]}}";

static bool has(const char* text) {
    for (int i = 0; s_line_count > i; ++i)
        if (0 == strcmp(text, s_lines[i].text)) return true;
    return false;
}

int main(void) {
    copy_str(g_game_state.instance_code, sizeof g_game_state.instance_code, "test");
    instance_map_data_open();
    FetchResponse response = { .asset_id = s_asset_id, .success = true, .data = (void*)WORLD, .size = strlen(WORLD) };
    s_callback(&response);
    assert(IMAP_DATA_READY == instance_map_data_state());
    instance_map_data_update(0.0f);

    /* A portal says where it starts, where it leads, how, and where the way back is. */
    float height = map_detail_height((MapDetail){ MAP_DETAIL_PORTAL, 0, -1 });
    assert(0 == strcmp("Portal", s_title) && 4 == s_line_count);
    assert(MAP_DETAIL_HEADER_H + 2.0f * MAP_DETAIL_PAD + 4.0f * MAP_DETAIL_LINE_H == height);
    assert(has("FROM A (1, 2)") && has("TO B (3, 4)") && has("LEADS TO ANOTHER MAP") && has("RETURN PORTAL: B (3, 4)"));
    map_detail_height((MapDetail){ MAP_DETAIL_PORTAL, 1, -1 });
    assert(has("TO B - random cell") && has("RANDOM CELL OF ANOTHER MAP"));
    map_detail_height((MapDetail){ MAP_DETAIL_PORTAL, 2, -1 });
    assert(3 == s_line_count && has("TO This map (5, 6)") && has("LEADS INSIDE THIS MAP"));

    /* A place lists its quests, then its actions; a row opens one. */
    map_detail_height((MapDetail){ MAP_DETAIL_PLACE, 0, -1 });
    assert(0 == strcmp("A (1, 1)", s_title) && 3 == s_line_count);
    assert(has("First - AVAILABLE") && has("Second - LOCKED") && has("Vendor - 3 SERVICES"));
    Rectangle card = { 0, 0, 300, 400 };
    map_detail_draw(card, (MapDetail){ MAP_DETAIL_PLACE, 0, -1 }, 1.0f);
    float row_y = MAP_DETAIL_HEADER_H + MAP_DETAIL_PAD + 2.0f * MAP_DETAIL_LINE_H + MAP_DETAIL_LINE_H * 0.5f;
    assert(2 == map_detail_row_at((Vector2){ 100, row_y }) && -1 == map_detail_row_at((Vector2){ 100, 399 }));

    /* An action lists the services it configures, the quests it fronts, and what is open to the player. */
    s_icons = s_tinted = 0;
    map_detail_draw(card, (MapDetail){ MAP_DETAIL_PLACE, 0, 2 }, 1.0f);
    assert(0 == strcmp("Vendor", s_title) && MAP_DETAIL_BACK == s_lines[0].row);
    assert(has("ACTION vendor - A (1, 1)") && has("CONFIGURED SERVICES") && has("SHOP") && has("MARKET") && has("TALK"));
    assert(!has("STORAGE") && has("QUEST Second - LOCKED") && has("FOR YOU: OPEN NOW"));
    assert(0 < s_icons && 0 == s_tinted);
    assert(MAP_DETAIL_BACK == map_detail_row_at((Vector2){ 100, MAP_DETAIL_HEADER_H + MAP_DETAIL_PAD + 5.0f }));

    /* A quest without its metadata says so; it never claims more than the store establishes. */
    map_detail_height((MapDetail){ MAP_DETAIL_PLACE, 0, 1 });
    assert(has("QUEST q2 - A (1, 1)") && has("LOCKED") && has("NEEDS: First") && has("LOADING QUEST DETAILS..."));
    quest_progress_store_upsert(&(QuestSnapshot){ .code = "q1", .status = "completed", .active_step = "", .step_id = "" });
    copy_str(s_meta.title, sizeof s_meta.title, "Second");
    copy_str(s_meta.description, sizeof s_meta.description, "Go further.");
    s_meta.step_count = 1;
    copy_str(s_meta.steps[0].description, sizeof s_meta.steps[0].description, "Collect a sword.");
    s_meta.reward_count = 1;
    s_meta.rewards[0] = (QuestRewardMeta){ .item_id = "coin", .quantity = 5 };
    map_detail_height((MapDetail){ MAP_DETAIL_PLACE, 0, 1 });
    assert(has("AVAILABLE") && has("NEEDS: First (DONE)") && has("Go further.") && has("FIRST: Collect a sword."));
    assert(has("REWARDS: coin x5") && !has("LOADING QUEST DETAILS..."));
    map_detail_height((MapDetail){ MAP_DETAIL_PLACE, 0, 0 });
    assert(has("COMPLETED") && has("UNLOCKS: Second"));

    /* A place with one item opens it at once; an action without services says so. */
    map_detail_height((MapDetail){ MAP_DETAIL_PLACE, 1, -1 });
    assert(0 == strcmp("idle", s_title) && -1 == s_lines[0].row && has("NO SERVICES CONFIGURED"));

    /* A placed bot or resource: what it is, its class, its stat sum as placed, its place and what the mark
     * shows. No quest detail is read. */
    s_quest_reads = s_glyphs = 0;
    map_detail_draw(card, (MapDetail){ MAP_DETAIL_PLACE, 2, -1 }, 1.0f);
    assert(0 == strcmp("wolf", s_title) && 4 == s_line_count && has("STAT SUM 54 AS PLACED"));
    assert(has("HOSTILE - CAN ATTACK YOU") && has("B (7, 7)") && has("PLACED HERE - IT MOVES IN PLAY"));
    assert(1 == s_glyphs && IMAP_PRESENCE_HOSTILE == s_glyph && -1 == map_detail_row_at((Vector2){ 100, 60 }));

    /* A map counts the bots and resources it places, one line and one glyph per class; the map of a
     * fixed entry spawn names its cell. */
    s_glyphs = 0;
    map_detail_draw(card, (MapDetail){ MAP_DETAIL_MAP, 0, -1 }, 1.0f);
    assert(has("1 HOSTILE BOT PLACED") && has("2 PASSIVE BOTS PLACED") && has("3 RESOURCES PLACED"));
    assert(has("ENTRY SPAWN (4, 5)") && 3 == s_glyphs && IMAP_PRESENCE_RESOURCE == s_glyph);
    map_detail_height((MapDetail){ MAP_DETAIL_MAP, 1, -1 });
    for (int i = 0; s_line_count > i; ++i)
        assert(NULL == strstr(s_lines[i].text, "PLACED") && NULL == strstr(s_lines[i].text, "ENTRY"));

    /* The entry spawn says where a new player starts, and that a defeat never moves a player. */
    map_detail_height((MapDetail){ MAP_DETAIL_SPAWN, 0, -1 });
    assert(0 == strcmp("Entry spawn", s_title) && has("A (4, 5)") && has("NEW PLAYERS ENTER THE WORLD HERE"));
    assert(has("BLOCKED CELL: RANDOM CELL INSTEAD") && has("A DEFEAT REVIVES IN PLACE"));
    assert(0 == s_quest_reads);

    /* A random or an unresolved spawn names no cell, on every map. */
    ImapEntrySpawn* spawn = &((ImapGraph*)instance_map_data_graph())->entry_spawn;
    spawn->mode = IMAP_SPAWN_RANDOM;
    map_detail_height((MapDetail){ MAP_DETAIL_MAP, 1, -1 });
    assert(has("ENTRY: RANDOM MAP AND CELL"));
    map_detail_height((MapDetail){ MAP_DETAIL_SPAWN, 0, -1 });
    assert(has("RANDOM MAP AND CELL") && has("NEW PLAYERS ENTER ON A RANDOM CELL"));
    assert(!has("NEW PLAYERS ENTER THE WORLD HERE"));
    for (int i = 0; s_line_count > i; ++i) assert(NULL == strstr(s_lines[i].text, "(4, 5)"));
    spawn->mode = IMAP_SPAWN_UNRESOLVED;
    map_detail_height((MapDetail){ MAP_DETAIL_MAP, 0, -1 });
    assert(has("ENTRY: UNRESOLVED - RANDOM CELL") && !has("ENTRY SPAWN (4, 5)"));
    map_detail_height((MapDetail){ MAP_DETAIL_SPAWN, 0, -1 });
    assert(has("CONFIG CELL NOT IN THIS WORLD") && has("NEW PLAYERS ENTER ON A RANDOM CELL"));
    char entry[64] = "x";
    map_detail_entry_text(&(ImapEntrySpawn){ .mode = IMAP_SPAWN_NONE }, entry, sizeof entry);
    assert('\0' == entry[0]);

    /* A map card says how the player reaches the map: here, by the first portal of the route, or not at all. */
    copy_str(g_local_player.map_code, sizeof g_local_player.map_code, "a");
    map_route_update();
    map_detail_height((MapDetail){ MAP_DETAIL_MAP, 0, -1 });
    assert(has("ROUTE: YOU ARE HERE"));
    map_detail_height((MapDetail){ MAP_DETAIL_MAP, 1, -1 });
    assert(has("ROUTE: 1 PORTAL - TAKE (1, 2)"));
    copy_str(g_local_player.map_code, sizeof g_local_player.map_code, "elsewhere");
    map_route_update();
    map_detail_height((MapDetail){ MAP_DETAIL_MAP, 1, -1 });
    assert(has("NO KNOWN FIXED ROUTE FROM YOUR MAP"));

    /* Only a quest card reads quest details. */
    map_detail_height((MapDetail){ MAP_DETAIL_PLACE, 0, -1 });
    assert(0 == s_quest_reads);
    map_detail_height((MapDetail){ MAP_DETAIL_PLACE, 0, 0 });
    assert(0 < s_quest_reads);

    /* The followed objective: what to do, its step, how many places it has, each a row nearest first, then a
     * row to stop. */
    copy_str(g_local_player.map_code, sizeof g_local_player.map_code, "a");
    QuestSnapshot hunt = { .code = "q2", .status = "active", .active_step = "Hunt", .step_id = "s", .objective_count = 2,
                           .objectives = { { .type = QUEST_OBJECTIVE_KILL, .item_id = "wolf", .current = 1, .required = 3 },
                                           { .type = QUEST_OBJECTIVE_COLLECT, .item_id = "potion", .required = 1 } } };
    quest_progress_store_upsert(&hunt);
    quest_target_update();
    quest_target_track(quest_target_find("q2", "s", QUEST_OBJECTIVE_KILL, "wolf"), 0);
    s_icons = s_tinted = 0;
    map_detail_draw(card, (MapDetail){ MAP_DETAIL_OBJECTIVE, -1, -1 }, 1.0f);
    assert(0 == strcmp("Second", s_title) && has("Defeat wolf 1/3") && has("STEP: Hunt"));
    assert(has("ALL 3 PLACES - NEAREST FIRST") && has("1 PORTAL - (7, 7) B") && has("STOP FOLLOWING"));
    assert(0 == s_tinted && !has("BACK TO NEAREST"));
    float row = MAP_DETAIL_HEADER_H + MAP_DETAIL_PAD + 3.0f * MAP_DETAIL_LINE_H + MAP_DETAIL_LINE_H * 0.5f;
    assert(0 == map_detail_row_at((Vector2){ 100, row }));
    assert(MAP_DETAIL_UNFOLLOW == map_detail_row_at((Vector2){ 100, row + 3.0f * MAP_DETAIL_LINE_H }));
    /* A short card pages through every place: the page that holds the destination in focus, a row to the next
     * page, and the row to stop following, always. */
    const QuestTarget* wolves = quest_target_find("q2", "s", QUEST_OBJECTIVE_KILL, "wolf");
    quest_target_track(wolves, 2);
    Rectangle short_card = { 0, 0, 300, MAP_DETAIL_HEADER_H + 2.0f * MAP_DETAIL_PAD + 6.0f * MAP_DETAIL_LINE_H };
    map_detail_draw(short_card, (MapDetail){ MAP_DETAIL_OBJECTIVE, -1, -1 }, 1.0f);
    assert(6 == s_line_count && has("PLACES 3-3 OF 3 - NEAREST FIRST") && has("1 PORTAL - (9, 9) B"));
    assert(has("BACK TO NEAREST") && has("STOP FOLLOWING") && 0 == map_detail_next_page());
    assert(2 == map_detail_row_at((Vector2){ 100, row }));
    assert(MAP_DETAIL_PAGE == map_detail_row_at((Vector2){ 100, row + MAP_DETAIL_LINE_H }));
    map_detail_draw(short_card, (MapDetail){ MAP_DETAIL_OBJECTIVE, -1, 0 }, 1.0f);
    assert(has("PLACES 1-1 OF 3 - NEAREST FIRST") && has("1 PORTAL - (7, 7) B") && has("MORE: 2-2 OF 3"));
    assert(1 == map_detail_next_page() && 0 == map_detail_row_at((Vector2){ 100, row }));
    quest_target_track(wolves, 0);
    /* A target names the objectives it serves, each a row that follows it; a map counts the objectives it holds. */
    map_detail_draw(card, (MapDetail){ MAP_DETAIL_PLACE, 2, -1 }, 1.0f);
    assert(0 == strcmp("DEFEAT wolf - Second", s_lines[4].text));
    int wolf = (int)(quest_target_find("q2", "s", QUEST_OBJECTIVE_KILL, "wolf") - quest_target_get(0));
    assert(MAP_DETAIL_FOLLOW + wolf == map_detail_row_at((Vector2){ 100, row + MAP_DETAIL_LINE_H }));
    map_detail_height((MapDetail){ MAP_DETAIL_PLACE, 0, -1 });
    assert(has("BUY potion - Second"));
    map_detail_height((MapDetail){ MAP_DETAIL_MAP, 1, -1 });
    assert(has("QUEST OBJECTIVES HERE") && has("Defeat wolf - Second") && !has("Collect potion - Second"));
    map_detail_height((MapDetail){ MAP_DETAIL_MAP, 0, -1 });
    assert(has("Collect potion - Second"));
    /* Without a followed objective the card says nothing. */
    quest_target_untrack();
    assert(MAP_DETAIL_HEADER_H + 2.0f * MAP_DETAIL_PAD == map_detail_height((MapDetail){ MAP_DETAIL_OBJECTIVE, -1, -1 }));

    puts("map_detail_test OK");
    return 0;
}
