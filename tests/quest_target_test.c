/* Objective resolution: a kill target is a mortal bot of the skin, a talk
 * target any bot of the skin, a collect target a cell that drops, sells or
 * assembles the item. Destinations come nearest first by portal hops; a map no
 * route reaches makes a target unreachable; nothing in the world makes it
 * absent, or without a source. Only actionable objectives of active quests are
 * targets, and each keeps every destination. The followed quest moves to its
 * next objective; a destination the player picks stays in focus while it serves
 * the objective. */

#include "data/quest_target.h"
#include "data/engine_client.h"
#include "data/quest_cache.h"
#include "sim/game_state.h"
#include "sim/local_player.h"
#include "utils.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

GameState g_game_state;
LocalPlayer g_local_player;

static FetchCompletedCb s_callback;
static char s_asset_id[64];
void fetch_request_start(const char* id, const char* url, FetchCompletedCb callback) {
    copy_str(s_asset_id, sizeof s_asset_id, id);
    s_callback = callback;
}
void fetch_forget(const char* url) {}
void fetch_data_release(void* data) {}
const QuestMetadataEntry* quest_cache_get(const char* code) { return NULL; }
void audio_event(const char* logic_event_id) {}

/* Three maps: a leads to b; nothing leads to c. */
#define WORLD_HEAD                                                                                          \
    "{\"status\":\"success\",\"data\":{\"instanceCode\":\"test\",\"nodes\":["                             \
    "{\"mapCode\":\"a\",\"name\":\"A\",\"w\":0.5,\"h\":0.5,\"gridX\":16,\"gridY\":16,\"position\":{\"x\":-1,\"y\":0}}," \
    "{\"mapCode\":\"b\",\"name\":\"B\",\"w\":0.5,\"h\":0.5,\"gridX\":16,\"gridY\":16,\"position\":{\"x\":0,\"y\":0}},"  \
    "{\"mapCode\":\"c\",\"name\":\"C\",\"w\":0.5,\"h\":0.5,\"gridX\":16,\"gridY\":16,\"position\":{\"x\":1,\"y\":0}}]," \
    "\"edges\":[{\"sourceMapCode\":\"a\",\"sourceCellX\":9,\"sourceCellY\":9,\"targetMapCode\":\"b\",\"targetCellX\":0,\"targetCellY\":0}],"

static const char* WORLD = WORLD_HEAD
    "\"presencePois\":["
    "{\"mapCode\":\"b\",\"cellX\":3,\"cellY\":3,\"presenceStatus\":\"hostile\",\"bots\":[{\"skin\":\"wolf\",\"mortal\":true}]},"
    "{\"mapCode\":\"a\",\"cellX\":1,\"cellY\":1,\"presenceStatus\":\"hostile\",\"bots\":[{\"skin\":\"wolf\",\"mortal\":true}],"
    "\"yields\":[{\"itemId\":\"pelt\",\"via\":\"drop\"}]},"
    "{\"mapCode\":\"a\",\"cellX\":2,\"cellY\":2,\"presenceStatus\":\"passive\",\"bots\":[{\"skin\":\"sage\",\"mortal\":false}],"
    "\"yields\":[{\"itemId\":\"potion\",\"via\":\"shop\"},{\"itemId\":\"potion\",\"via\":\"craft\"}]},"
    "{\"mapCode\":\"b\",\"cellX\":5,\"cellY\":5,\"presenceStatus\":\"passive\",\"bots\":[{\"skin\":\"sage\",\"mortal\":true}]},"
    "{\"mapCode\":\"c\",\"cellX\":4,\"cellY\":4,\"presenceStatus\":\"hostile\",\"bots\":[{\"skin\":\"ghost\",\"mortal\":true}]}]}}";

/* The same world rebuilt without the wolf of b. */
static const char* REBUILT = WORLD_HEAD
    "\"presencePois\":["
    "{\"mapCode\":\"a\",\"cellX\":1,\"cellY\":1,\"presenceStatus\":\"hostile\",\"bots\":[{\"skin\":\"wolf\",\"mortal\":true}]},"
    "{\"mapCode\":\"a\",\"cellX\":2,\"cellY\":2,\"presenceStatus\":\"passive\",\"bots\":[{\"skin\":\"sage\",\"mortal\":false}]}]}}";

static void deliver(const char* world) {
    FetchResponse response = { .asset_id = s_asset_id, .success = true, .data = (void*)world, .size = strlen(world) };
    s_callback(&response);
    assert(IMAP_DATA_READY == instance_map_data_state());
}

static QuestObjectiveProgress objective(QuestObjectiveType type, const char* item, int current, int required) {
    QuestObjectiveProgress o = { .type = type, .current = current, .required = required };
    copy_str(o.item_id, sizeof o.item_id, item);
    return o;
}

static void quest(const char* code, const char* status, const char* step, const QuestObjectiveProgress* objectives,
                  int count) {
    QuestSnapshot snapshot = { .code = code, .status = status, .active_step = "", .step_id = step,
                               .objective_count = count };
    memcpy(snapshot.objectives, objectives, sizeof(QuestObjectiveProgress) * (size_t)count);
    quest_progress_store_upsert(&snapshot);
}

static const QuestTarget* target(QuestObjectiveType type, const char* item) {
    return quest_target_find("q1", "hunt", type, item);
}

/* The map and the cell of a destination. */
static bool at(const QuestTarget* t, int d, const char* map, int x, int y) {
    const ImapGraph* gr = instance_map_data_graph();
    const ImapPresencePoi* poi = &gr->presence_pois[t->destinations[d].poi];
    return 0 == strcmp(map, gr->nodes[poi->node].map_code) && x == poi->cell_x && y == poi->cell_y;
}

int main(void) {
    copy_str(g_game_state.instance_code, sizeof g_game_state.instance_code, "test");
    copy_str(g_local_player.map_code, sizeof g_local_player.map_code, "a");
    const QuestObjectiveProgress hunt[] = {
        objective(QUEST_OBJECTIVE_KILL, "wolf", 0, 2),   objective(QUEST_OBJECTIVE_TALK, "sage", 0, 1),
        objective(QUEST_OBJECTIVE_COLLECT, "pelt", 2, 2), objective(QUEST_OBJECTIVE_COLLECT, "potion", 1, 3),
    };
    quest("q1", "active", "hunt", hunt, 4);
    const QuestObjectiveProgress odd[] = {
        objective(QUEST_OBJECTIVE_KILL, "sage", 0, 1), objective(QUEST_OBJECTIVE_KILL, "dragon", 0, 1),
        objective(QUEST_OBJECTIVE_COLLECT, "gold", 0, 1), objective(QUEST_OBJECTIVE_KILL, "ghost", 0, 1),
    };
    quest("q2", "active", "odd", odd, 4);
    const QuestObjectiveProgress done[] = { objective(QUEST_OBJECTIVE_KILL, "wolf", 0, 1) };
    quest("q3", "completed", "", done, 1);

    /* Before the world map loads, every actionable objective waits; a done one and a completed quest are none. */
    instance_map_data_open();
    quest_target_update();
    assert(7 == quest_target_count() && QUEST_TARGET_PENDING == target(QUEST_OBJECTIVE_KILL, "wolf")->state);
    assert(NULL == target(QUEST_OBJECTIVE_COLLECT, "pelt"));
    assert(NULL == quest_target_find("q3", "", QUEST_OBJECTIVE_KILL, "wolf"));

    deliver(WORLD);
    quest_target_update();
    /* A kill target: every mortal bot of the skin, the player's map first. */
    const QuestTarget* wolf = target(QUEST_OBJECTIVE_KILL, "wolf");
    assert(QUEST_TARGET_FOUND == wolf->state && 2 == wolf->destination_count);
    assert(at(wolf, 0, "a", 1, 1) && 0 == wolf->destinations[0].hops);
    assert(at(wolf, 1, "b", 3, 3) && 1 == wolf->destinations[1].hops);
    assert((1u << QUEST_VIA_KILL) == wolf->destinations[0].vias);
    /* A talk target: any bot of the skin, immortal or not. A kill target is never an immortal one. */
    const QuestTarget* talk = target(QUEST_OBJECTIVE_TALK, "sage");
    assert(2 == talk->destination_count && at(talk, 0, "a", 2, 2) && at(talk, 1, "b", 5, 5));
    const QuestTarget* sage = quest_target_find("q2", "odd", QUEST_OBJECTIVE_KILL, "sage");
    assert(1 == sage->destination_count && at(sage, 0, "b", 5, 5));
    /* A collect target: one destination per cell, with every way the cell gives the item. */
    const QuestTarget* potion = target(QUEST_OBJECTIVE_COLLECT, "potion");
    assert(1 == potion->destination_count && at(potion, 0, "a", 2, 2));
    assert(((1u << QUEST_VIA_SHOP) | (1u << QUEST_VIA_CRAFT)) == potion->destinations[0].vias);
    /* Nothing in the world: absent, or without a source; a map no route reaches: unreachable. */
    assert(QUEST_TARGET_ABSENT == quest_target_find("q2", "odd", QUEST_OBJECTIVE_KILL, "dragon")->state);
    assert(QUEST_TARGET_NO_SOURCE == quest_target_find("q2", "odd", QUEST_OBJECTIVE_COLLECT, "gold")->state);
    const QuestTarget* ghost = quest_target_find("q2", "odd", QUEST_OBJECTIVE_KILL, "ghost");
    assert(QUEST_TARGET_UNREACHABLE == ghost->state && -1 == ghost->destinations[0].hops);

    /* A picked destination stays in focus when the nearest order changes under it. */
    quest_target_track(wolf, 1);
    assert(wolf == quest_target_tracked() && 1 == quest_target_tracked_destination());
    copy_str(g_local_player.map_code, sizeof g_local_player.map_code, "b");
    quest_target_update();
    wolf = target(QUEST_OBJECTIVE_KILL, "wolf");
    assert(at(wolf, 0, "b", 3, 3) && 0 == quest_target_tracked_destination());
    copy_str(g_local_player.map_code, sizeof g_local_player.map_code, "a");
    quest_target_update();
    wolf = target(QUEST_OBJECTIVE_KILL, "wolf");
    assert(1 == quest_target_tracked_destination());

    /* A rebuild without that destination keeps the objective, in focus on the nearest one left. */
    instance_map_data_refresh();
    deliver(REBUILT);
    quest_target_update();
    wolf = target(QUEST_OBJECTIVE_KILL, "wolf");
    assert(1 == wolf->destination_count && wolf == quest_target_tracked());
    assert(0 == quest_target_tracked_destination());

    /* The player on another map: hops count from there. */
    deliver(WORLD);
    copy_str(g_local_player.map_code, sizeof g_local_player.map_code, "b");
    quest_target_update();
    wolf = target(QUEST_OBJECTIVE_KILL, "wolf");
    assert(at(wolf, 0, "b", 3, 3) && 0 == wolf->destinations[0].hops && -1 == wolf->destinations[1].hops);

    /* A done objective hands the following to the next objective of its quest; a done quest ends it. */
    const QuestObjectiveProgress killed[] = {
        objective(QUEST_OBJECTIVE_KILL, "wolf", 2, 2), objective(QUEST_OBJECTIVE_TALK, "sage", 0, 1),
    };
    quest("q1", "active", "hunt", killed, 2);
    quest_target_update();
    assert(target(QUEST_OBJECTIVE_TALK, "sage") == quest_target_tracked());
    quest("q1", "completed", "", NULL, 0);
    quest_target_update();
    assert(NULL == quest_target_tracked() && NULL == target(QUEST_OBJECTIVE_TALK, "sage"));

    /* Every cell is a destination, nearest first, however many the world places. */
    static char crowd[8192];
    int at_ = snprintf(crowd, sizeof crowd, "%s\"presencePois\":[", WORLD_HEAD);
    for (int i = 0; 30 > i; i++)
        at_ += snprintf(crowd + at_, sizeof crowd - (size_t)at_,
                        "{\"mapCode\":\"b\",\"cellX\":%d,\"cellY\":1,\"presenceStatus\":\"hostile\","
                        "\"bots\":[{\"skin\":\"rat\",\"mortal\":true}]},", i);
    snprintf(crowd + at_, sizeof crowd - (size_t)at_,
             "{\"mapCode\":\"a\",\"cellX\":7,\"cellY\":7,\"presenceStatus\":\"hostile\","
             "\"bots\":[{\"skin\":\"rat\",\"mortal\":true}]}]}}");
    copy_str(g_local_player.map_code, sizeof g_local_player.map_code, "a");
    const QuestObjectiveProgress rats[] = { objective(QUEST_OBJECTIVE_KILL, "rat", 0, 9) };
    quest("q4", "active", "rats", rats, 1);
    instance_map_data_refresh();
    deliver(crowd);
    quest_target_update();
    const QuestTarget* rat = quest_target_find("q4", "rats", QUEST_OBJECTIVE_KILL, "rat");
    assert(31 == rat->destination_count && at(rat, 0, "a", 7, 7));
    assert(at(rat, 1, "b", 0, 1) && at(rat, 30, "b", 29, 1) && 1 == rat->destinations[30].hops);

    puts("quest_target_test OK");
    return 0;
}
