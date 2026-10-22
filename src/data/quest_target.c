#include "quest_target.h"

#include "instance_map_data.h"
#include "map_route.h"
#include "quest_progress_store.h"
#include "utils.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static QuestTarget s_targets[QUEST_TARGETS_MAX];
static int         s_target_count;

/* What the targets were resolved from. */
static int  s_resolved_revision = -1;
static int  s_resolved_route = -1;

/* The followed quest: the objective in focus, and the cell of its destination in focus. */
static struct {
    bool on;
    char quest_code[META_CACHE_CODE_MAX];
    char step_id[META_CACHE_CODE_MAX];
    QuestObjectiveType type;
    char item_id[MAX_ITEM_ID_LENGTH];
    int  node, cell_x, cell_y;
} s_tracked;

static const char* const VIA_WORDS[QUEST_VIA_COUNT] = {
    [QUEST_VIA_KILL] = "DEFEAT", [QUEST_VIA_TALK] = "TALK TO", [QUEST_VIA_DROP] = "LOOT",
    [QUEST_VIA_SHOP] = "BUY",    [QUEST_VIA_CRAFT] = "ASSEMBLE",
};

void quest_target_ways(const QuestDestination* destination, char* out, size_t size) {
    out[0] = '\0';
    for (int v = 0; QUEST_VIA_COUNT > v; v++)
        if (destination->vias & (1u << v))
            snprintf(out + strlen(out), size - strlen(out), "%s%s", '\0' == out[0] ? "" : "/", VIA_WORDS[v]);
}

void quest_target_action_text(const QuestTarget* target, int destination, char* out, size_t size) {
    const ImapGraph* gr = instance_map_data_graph();
    const QuestDestination* d = &target->destinations[destination];
    char words[48];
    quest_target_ways(d, words, sizeof words);
    const char* name = target->objective.item_id;
    for (int a = 0; QUEST_OBJECTIVE_TALK == target->objective.type && gr->action_count > a; a++)
        if (d->poi == gr->actions[a].poi && '\0' != gr->actions[a].label[0]) {
            name = gr->actions[a].label;
            break;
        }
    snprintf(out, size, "%s %s", words, name);
}

/* The destination of each POI in the target being resolved, -1 for none. */
static int s_slot[IMAP_MAX_PRESENCE_POIS];

/* One destination per POI, with the ways it gives. */
static void add_destination(QuestTarget* t, int poi, QuestVia via) {
    if (0 > s_slot[poi]) {
        s_slot[poi] = t->destination_count;
        t->destinations[t->destination_count++] =
            (QuestDestination){ poi, 0, map_route_hops(instance_map_data_graph()->presence_pois[poi].node) };
    }
    t->destinations[s_slot[poi]].vias |= (uint8_t)(1u << via);
}

/* A destination a route reaches first, the fewest portals away, then in map order and in cell order. */
static int nearer(const void* left, const void* right) {
    const QuestDestination *a = left, *b = right;
    const ImapPresencePoi* pa = &instance_map_data_graph()->presence_pois[a->poi];
    const ImapPresencePoi* pb = &instance_map_data_graph()->presence_pois[b->poi];
    if ((0 > a->hops) != (0 > b->hops)) return 0 > a->hops ? 1 : -1;
    if (a->hops != b->hops) return a->hops - b->hops;
    if (pa->node != pb->node) return pa->node - pb->node;
    if (pa->cell_y != pb->cell_y) return pa->cell_y - pb->cell_y;
    return pa->cell_x - pb->cell_x;
}

static void resolve(QuestTarget* t) {
    static const QuestVia YIELD_VIA[IMAP_YIELD_VIA_COUNT] = {
        [IMAP_YIELD_DROP] = QUEST_VIA_DROP, [IMAP_YIELD_SHOP] = QUEST_VIA_SHOP, [IMAP_YIELD_CRAFT] = QUEST_VIA_CRAFT,
    };
    const ImapGraph* gr = instance_map_data_graph();
    const char* item = t->objective.item_id;
    bool collect = QUEST_OBJECTIVE_COLLECT == t->objective.type, kill = QUEST_OBJECTIVE_KILL == t->objective.type;
    for (int i = 0; gr->presence_poi_count > i; i++) s_slot[i] = -1;
    for (int i = 0; collect && gr->yield_count > i; i++)
        if (0 == strcmp(item, gr->yields[i].item_id))
            add_destination(t, gr->yields[i].poi, YIELD_VIA[gr->yields[i].via]);
    for (int i = 0; !collect && gr->placed_count > i; i++) {
        const ImapPlaced* placed = &gr->placed[i];
        if (placed->bot && 0 == strcmp(item, placed->item_id) && (!kill || placed->mortal))
            add_destination(t, placed->poi, kill ? QUEST_VIA_KILL : QUEST_VIA_TALK);
    }
    qsort(t->destinations, (size_t)t->destination_count, sizeof t->destinations[0], nearer);
    t->state = 0 == t->destination_count ? (collect ? QUEST_TARGET_NO_SOURCE : QUEST_TARGET_ABSENT)
             : 0 > t->destinations[0].hops ? QUEST_TARGET_UNREACHABLE
                                            : QUEST_TARGET_FOUND;
}

const QuestTarget* quest_target_find(const char* quest_code, const char* step_id, QuestObjectiveType type,
                                     const char* item_id) {
    for (int i = 0; s_target_count > i; i++) {
        const QuestTarget* t = &s_targets[i];
        if (type == t->objective.type && 0 == strcmp(quest_code, t->quest_code) && 0 == strcmp(step_id, t->step_id) &&
            0 == strcmp(item_id, t->objective.item_id))
            return t;
    }
    return NULL;
}

const QuestTarget* quest_target_tracked(void) {
    return s_tracked.on ? quest_target_find(s_tracked.quest_code, s_tracked.step_id, s_tracked.type, s_tracked.item_id)
                        : NULL;
}

int quest_target_tracked_destination(void) {
    const QuestTarget* t = quest_target_tracked();
    for (int d = 0; NULL != t && t->destination_count > d; d++) {
        const ImapPresencePoi* poi = &instance_map_data_graph()->presence_pois[t->destinations[d].poi];
        if (s_tracked.node == poi->node && s_tracked.cell_x == poi->cell_x && s_tracked.cell_y == poi->cell_y)
            return d;
    }
    return 0;
}

void quest_target_track(const QuestTarget* target, int destination) {
    s_tracked.on = true;
    copy_str(s_tracked.quest_code, sizeof s_tracked.quest_code, target->quest_code);
    copy_str(s_tracked.step_id, sizeof s_tracked.step_id, target->step_id);
    copy_str(s_tracked.item_id, sizeof s_tracked.item_id, target->objective.item_id);
    s_tracked.type = target->objective.type;
    s_tracked.node = -1;
    if (0 <= destination && target->destination_count > destination) {
        const ImapPresencePoi* poi = &instance_map_data_graph()->presence_pois[target->destinations[destination].poi];
        s_tracked.node = poi->node;
        s_tracked.cell_x = poi->cell_x;
        s_tracked.cell_y = poi->cell_y;
    }
}

void quest_target_untrack(void) { s_tracked.on = false; }

int quest_target_count(void) { return s_target_count; }
const QuestTarget* quest_target_get(int index) { return &s_targets[index]; }

void quest_target_update(void) {
    map_route_update();
    if (s_resolved_revision == quest_progress_store_revision() && s_resolved_route == map_route_revision()) return;
    s_resolved_revision = quest_progress_store_revision();
    s_resolved_route = map_route_revision();
    bool ready = IMAP_DATA_READY == instance_map_data_state();

    s_target_count = 0;
    for (int i = 0; quest_progress_store_count(QUEST_ACTIVE) > i; i++) {
        const QuestProgressEntry* e = quest_progress_store_get(QUEST_ACTIVE, i);
        for (int o = 0; e->objective_count > o && QUEST_TARGETS_MAX > s_target_count; o++) {
            if (e->objectives[o].current >= e->objectives[o].required) continue;
            QuestTarget* t = &s_targets[s_target_count++];
            *t = (QuestTarget){ .objective = e->objectives[o] };
            copy_str(t->quest_code, sizeof t->quest_code, e->code);
            copy_str(t->step_id, sizeof t->step_id, e->step_id);
        }
    }
    for (int i = 0; ready && s_target_count > i; i++) resolve(&s_targets[i]);

    /* A done objective hands the focus to the next one of its quest; a done quest ends the following. */
    if (s_tracked.on && NULL == quest_target_tracked()) {
        int next = 0;
        while (s_target_count > next && 0 != strcmp(s_tracked.quest_code, s_targets[next].quest_code)) next++;
        if (s_target_count > next) quest_target_track(&s_targets[next], 0);
        else s_tracked.on = false;
    }
}
