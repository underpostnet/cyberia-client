#include "map_detail.h"

#include "map_marker.h"
#include "modal.h"
#include "text.h"
#include "ui_icon.h"
#include "ui_rect.h"

#include "content/presentation_runtime.h"
#include "data/instance_map_data.h"
#include "data/map_route.h"
#include "data/quest_cache.h"
#include "data/quest_progress_store.h"
#include "data/quest_target.h"
#include "utils.h"
#include "world/world_types.h"

#include <raylib.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#define DETAIL_LINES_MAX 18
#define ICON_SIDE        ((float)MAP_DETAIL_FONT_BODY + 2.0f)
#define PLACE_ROWS_MAX   12
#define RELATED_MAX      3
#define DESTINATION_ROWS_MAX 6

typedef struct {
    const char* icon;
    ImapPresenceStatus glyph;   /* drawn in the icon slot when not NONE */
    char  text[160];
    Color color;
    int   font;
    int   row;      /* -1: plain text; else the id map_detail_row_at gives for it */
} DetailLine;

static DetailLine s_lines[DETAIL_LINES_MAX];
static int        s_line_count;
static char       s_title[IMAP_NAME_MAX * 2];
static Rectangle  s_row_rects[DETAIL_LINES_MAX];
static int        s_row_ids[DETAIL_LINES_MAX];
static int        s_row_count;
static int        s_next_page;

static const char* const SERVICE_NAMES[IMAP_SERVICE_COUNT] = {
    [IMAP_SERVICE_SHOP] = "SHOP",       [IMAP_SERVICE_CRAFT] = "ASSEMBLY", [IMAP_SERVICE_STORAGE] = "STORAGE",
    [IMAP_SERVICE_MARKET] = "MARKET",   [IMAP_SERVICE_BUYBACK] = "BUYBACK", [IMAP_SERVICE_BURN] = "SALVAGE",
    [IMAP_SERVICE_TALK] = "TALK",
};
static const char* const SERVICE_ICONS[IMAP_SERVICE_COUNT] = {
    [IMAP_SERVICE_SHOP] = "home-red",   [IMAP_SERVICE_CRAFT] = "engine",    [IMAP_SERVICE_STORAGE] = "grid",
    [IMAP_SERVICE_MARKET] = "market",   [IMAP_SERVICE_BUYBACK] = "buyback", [IMAP_SERVICE_BURN] = "burn",
    [IMAP_SERVICE_TALK] = "chat",
};

static const char* const OBJECTIVE_ICONS[QUEST_OBJECTIVE_TYPE_COUNT] = {
    [QUEST_OBJECTIVE_KILL] = "skull", [QUEST_OBJECTIVE_TALK] = "chat", [QUEST_OBJECTIVE_COLLECT] = "bag",
};

const char* map_detail_service_name(ImapService service) { return SERVICE_NAMES[service]; }
const char* map_detail_service_icon(ImapService service) { return SERVICE_ICONS[service]; }

const char* map_detail_quest_word(ImapQuestState state) {
    switch (state) {
        case IMAP_QUEST_ACTIVE:    return "IN PROGRESS";
        case IMAP_QUEST_AVAILABLE: return "AVAILABLE";
        case IMAP_QUEST_COMPLETED: return "COMPLETED";
        case IMAP_QUEST_LOCKED:
        default:                   return "LOCKED";
    }
}

const char* map_detail_quest_icon(ImapQuestState state) {
    switch (state) {
        case IMAP_QUEST_ACTIVE:    return "clock";
        case IMAP_QUEST_AVAILABLE: return "quest";
        case IMAP_QUEST_COMPLETED: return "check";
        case IMAP_QUEST_LOCKED:
        default:                   return "lock";
    }
}

static const char* plural(int count) { return 1 == count ? "" : "S"; }

/* Appends a line; NULL when the card is full. */
static DetailLine* add_line(const char* icon, Color color, int font, int row, const char* format, ...) {
    if (DETAIL_LINES_MAX <= s_line_count) return NULL;
    DetailLine* line = &s_lines[s_line_count++];
    *line = (DetailLine){ .icon = icon, .color = color, .font = font, .row = row };
    va_list args;
    va_start(args, format);
    vsnprintf(line->text, sizeof line->text, format, args);
    va_end(args);
    return line;
}

/* A line led by the glyph of a presence. */
static void add_presence_line(ImapPresenceStatus glyph, Color color, const char* text) {
    DetailLine* line = add_line("", color, MAP_DETAIL_FONT_BODY, -1, "%s", text);
    if (NULL != line) line->glyph = glyph;
}

void map_detail_entry_text(const ImapEntrySpawn* spawn, char* out, size_t size) {
    switch (spawn->mode) {
        case IMAP_SPAWN_FIXED: {
            const char* map = '\0' != spawn->map_name[0] ? spawn->map_name : spawn->map_code;
            snprintf(out, size, "%s (%d, %d)", map, spawn->cell_x, spawn->cell_y);
            break;
        }
        case IMAP_SPAWN_RANDOM:     snprintf(out, size, "RANDOM MAP AND CELL"); break;
        case IMAP_SPAWN_UNRESOLVED: snprintf(out, size, "UNRESOLVED - RANDOM CELL"); break;
        case IMAP_SPAWN_NONE:
        default:                    out[0] = '\0'; break;
    }
}

/* Appends `text` to `out`, after ", " when `out` holds a word already. */
static void append_word(char* out, size_t size, const char* text) {
    size_t length = strlen(out);
    snprintf(out + length, size - length, "%s%s", 0 == length ? "" : ", ", text);
}

/* A quest of the graph by code; NULL when this world has none. */
static const ImapProvider* graph_quest(const char* code) {
    const ImapGraph* gr = instance_map_data_graph();
    for (int q = 0; gr->quest_count > q; ++q)
        if (0 == strcmp(code, gr->quests[q].code)) return &gr->quests[q];
    return NULL;
}

/* The title of a quest: the one the graph sends, else the one of the player's quest, else its code. */
static const char* quest_title(const char* code) {
    const ImapProvider* quest = graph_quest(code);
    if (NULL != quest && '\0' != quest->label[0]) return quest->label;
    const QuestProgressEntry* own = quest_progress_store_find(code);
    return NULL != own && '\0' != own->title[0] ? own->title : code;
}

static void place_name(const ImapPresencePoi* poi, char* out, size_t size) {
    char map[IMAP_NAME_MAX * 2];
    instance_map_node_label(instance_map_data_graph(), poi->node, map, sizeof map);
    snprintf(out, size, "%s (%d, %d)", map, poi->cell_x, poi->cell_y);
}

/* The objectives a POI serves, one row each, at most RELATED_MAX; a row follows its objective there. */
static void add_target_lines(int poi) {
    int shown = 0;
    for (int i = 0; quest_target_count() > i && RELATED_MAX > shown; i++) {
        const QuestTarget* target = quest_target_get(i);
        for (int d = 0; target->destination_count > d; d++) {
            if (poi != target->destinations[d].poi) continue;
            char action[IMAP_NAME_MAX + 48];
            quest_target_action_text(target, d, action, sizeof action);
            add_line(OBJECTIVE_ICONS[target->objective.type], MODAL_QUEST, MAP_DETAIL_FONT_LABEL, MAP_DETAIL_FOLLOW + i,
                     "%s - %s", action, quest_title(target->quest_code));
            shown++;
            break;
        }
    }
}

/* Item `item` of a place: a quest when `is_quest`, else an action. */
static const ImapProvider* place_item(int poi, int item, bool* is_quest) {
    const ImapGraph* gr = instance_map_data_graph();
    for (int q = 0; gr->quest_count > q; ++q)
        if (poi == gr->quests[q].poi && 0 == item--) {
            *is_quest = true;
            return &gr->quests[q];
        }
    for (int a = 0; gr->action_count > a; ++a)
        if (poi == gr->actions[a].poi && 0 == item--) {
            *is_quest = false;
            return &gr->actions[a];
        }
    return NULL;
}

int map_detail_place_items(int poi) {
    const ImapPresencePoi* p = &instance_map_data_graph()->presence_pois[poi];
    return p->quest_count + p->action_count;
}

/* How the player reaches a map: it is here, a known route leaves by its first portal, or none does. */
static void add_route_line(int node) {
    int hops = map_route_hops(node);
    if (0 == hops) {
        add_line("map", MAP_MARKER_ROUTE, MAP_DETAIL_FONT_LABEL, -1, "ROUTE: YOU ARE HERE");
    } else if (0 > hops) {
        add_line(NULL, MODAL_LABEL, MAP_DETAIL_FONT_LABEL, -1, "NO KNOWN FIXED ROUTE FROM YOUR MAP");
    } else {
        const ImapEdge* first = &instance_map_data_graph()->edges[map_route_first(node)];
        add_line("forward", MAP_MARKER_ROUTE, MAP_DETAIL_FONT_LABEL, -1, "ROUTE: %d PORTAL%s - TAKE (%d, %d)", hops,
                 plural(hops), first->source_cell_x, first->source_cell_y);
    }
}

static void build_map(int node) {
    const ImapGraph* gr = instance_map_data_graph();
    const ImapNode* n = &gr->nodes[node];
    instance_map_node_label(gr, node, s_title, sizeof s_title);
    add_line(NULL, MODAL_LABEL, MAP_DETAIL_FONT_LABEL, -1, "SECTOR %s - %dx%d", n->map_code, n->grid_x, n->grid_y);
    add_route_line(node);

    ImapNodeLinks links = instance_map_node_links(gr, node);
    if (0 < links.portals || 0 < links.neighbors)
        add_line(presentation_runtime_status_icon(STATUS_ICON_PORTAL), instance_map_presence_color(IMAP_PRESENCE_PORTAL),
                 MAP_DETAIL_FONT_BODY, -1, "%d PORTAL%s - %d LINKED MAP%s", links.portals, plural(links.portals),
                 links.neighbors, plural(links.neighbors));
    if (0 < links.destination_count) {
        char to[160] = "";
        for (int d = 0; links.destination_count > d; ++d) {
            char name[IMAP_NAME_MAX * 2];
            instance_map_node_label(gr, links.destinations[d], name, sizeof name);
            append_word(to, sizeof to, name);
        }
        add_line("forward", MODAL_TEXT, MAP_DETAIL_FONT_LABEL, -1, "TO %s", to);
    }

    uint8_t services = 0;
    for (int i = 0; gr->presence_poi_count > i; ++i)
        if (node == gr->presence_pois[i].node) services |= gr->presence_pois[i].services;
    if (0 != services) {
        char names[160] = "";
        for (int s = 0; IMAP_SERVICE_COUNT > s; ++s)
            if (services & (1u << s)) append_word(names, sizeof names, SERVICE_NAMES[s]);
        add_line(presentation_runtime_status_icon(STATUS_ICON_ACTION_PROVIDER), MODAL_ACTION, MAP_DETAIL_FONT_BODY, -1,
                 "%s", names);
    }
    if (0 < n->quest_provider_count)
        add_line("quest", MODAL_QUEST, MAP_DETAIL_FONT_BODY, -1, "%d QUEST GIVER%s", n->quest_provider_count,
                 plural(n->quest_provider_count));
    int quests = instance_map_data_node_active_quests(node), actions = instance_map_data_node_active_actions(node);
    if (0 < quests || 0 < actions)
        add_line("check", MODAL_POSITIVE, MAP_DETAIL_FONT_BODY, -1, "FOR YOU: %d QUEST%s - %d SERVICE%s", quests,
                 plural(quests), actions, plural(actions));

    int objectives = 0;
    for (int i = 0; quest_target_count() > i; i++) {
        const QuestTarget* target = quest_target_get(i);
        for (int d = 0; target->destination_count > d; d++) {
            if (node != gr->presence_pois[target->destinations[d].poi].node) continue;
            if (0 == objectives++) add_line("quest", MODAL_QUEST, MAP_DETAIL_FONT_BODY, -1, "QUEST OBJECTIVES HERE");
            if (RELATED_MAX >= objectives)
                add_line(OBJECTIVE_ICONS[target->objective.type], MODAL_QUEST, MAP_DETAIL_FONT_LABEL, -1, "%s %s - %s",
                         quest_objective_verb(target->objective.type), target->objective.item_id,
                         quest_title(target->quest_code));
            break;
        }
    }
    if (RELATED_MAX < objectives)
        add_line(NULL, MODAL_LABEL, MAP_DETAIL_FONT_LABEL, -1, "+%d MORE", objectives - RELATED_MAX);

    const struct { ImapPresenceStatus glyph; int count; const char* name; } placed[] = {
        { IMAP_PRESENCE_HOSTILE, n->population.hostile, "HOSTILE BOT" },
        { IMAP_PRESENCE_PASSIVE, n->population.passive, "PASSIVE BOT" },
        { IMAP_PRESENCE_RESOURCE, n->population.resource, "RESOURCE" },
    };
    for (int i = 0; 3 > i; ++i) {
        if (0 >= placed[i].count) continue;
        char line[48];
        snprintf(line, sizeof line, "%d %s%s PLACED", placed[i].count, placed[i].name, plural(placed[i].count));
        add_presence_line(placed[i].glyph, MODAL_TEXT, line);
    }

    const ImapEntrySpawn* spawn = &gr->entry_spawn;
    if (IMAP_SPAWN_FIXED == spawn->mode && 0 == strcmp(spawn->map_code, n->map_code)) {
        add_line("home", MAP_MARKER_SPAWN, MAP_DETAIL_FONT_BODY, -1, "ENTRY SPAWN (%d, %d)", spawn->cell_x,
                 spawn->cell_y);
    } else if (IMAP_SPAWN_RANDOM == spawn->mode || IMAP_SPAWN_UNRESOLVED == spawn->mode) {
        char entry[IMAP_NAME_MAX + 32];
        map_detail_entry_text(spawn, entry, sizeof entry);
        add_line("home", MODAL_LABEL, MAP_DETAIL_FONT_LABEL, -1, "ENTRY: %s", entry);
    }
}

static void build_portal(int index) {
    const ImapGraph* gr = instance_map_data_graph();
    const ImapEdge* edge = &gr->edges[index];
    bool random = 0 > edge->target_cell_x || 0 > edge->target_cell_y;
    copy_str(s_title, sizeof s_title, "Portal");

    char from[IMAP_NAME_MAX * 2], to[IMAP_NAME_MAX * 3];
    instance_map_node_label(gr, edge->source_node, from, sizeof from);
    instance_map_edge_destination(gr, edge, to, sizeof to);
    Color portal = instance_map_presence_color(IMAP_PRESENCE_PORTAL);
    add_line(presentation_runtime_status_icon(STATUS_ICON_PORTAL), MODAL_LABEL, MAP_DETAIL_FONT_BODY, -1,
             "FROM %s (%d, %d)", from, edge->source_cell_x, edge->source_cell_y);
    add_line("forward", portal, MAP_DETAIL_FONT_BODY, -1, "TO %s", to);
    const char* kind = edge->intra ? random ? "RANDOM CELL OF THIS MAP" : "LEADS INSIDE THIS MAP"
                                   : random ? "RANDOM CELL OF ANOTHER MAP" : "LEADS TO ANOTHER MAP";
    add_line(NULL, MODAL_LABEL, MAP_DETAIL_FONT_LABEL, -1, "%s", kind);
    if (edge->intra) return;
    for (int i = 0; gr->edge_count > i; ++i) {
        const ImapEdge* back = &gr->edges[i];
        if (back->source_node != edge->target_node || back->target_node != edge->source_node) continue;
        char back_map[IMAP_NAME_MAX * 2];
        instance_map_node_label(gr, back->source_node, back_map, sizeof back_map);
        add_line(NULL, MODAL_LABEL, MAP_DETAIL_FONT_LABEL, -1, "RETURN PORTAL: %s (%d, %d)", back_map,
                 back->source_cell_x, back->source_cell_y);
        return;
    }
    add_line(NULL, MODAL_LABEL, MAP_DETAIL_FONT_LABEL, -1, "NO RETURN PORTAL");
}

static void build_quest(const ImapProvider* quest, const char* place) {
    ImapQuestState state = instance_map_quest_state(quest->code);
    add_line(NULL, MODAL_LABEL, MAP_DETAIL_FONT_LABEL, -1, "QUEST %s - %s", quest->code, place);

    char needs[160] = "";
    for (int i = 0; quest->link_count > i; ++i) {
        char word[IMAP_NAME_MAX + 16];
        snprintf(word, sizeof word, "%s%s", quest_title(quest->links[i]),
                 quest_progress_store_is_completed(quest->links[i]) ? " (DONE)" : "");
        append_word(needs, sizeof needs, word);
    }
    add_line(map_detail_quest_icon(state), IMAP_QUEST_LOCKED == state ? MODAL_LABEL : MODAL_QUEST, MAP_DETAIL_FONT_BODY,
             -1, "%s", map_detail_quest_word(state));
    if ('\0' != needs[0]) add_line(NULL, MODAL_LABEL, MAP_DETAIL_FONT_LABEL, -1, "NEEDS: %s", needs);

    const QuestMetadataEntry* meta = quest_cache_get(quest->code);
    const QuestProgressEntry* progress = quest_progress_store_find(quest->code);
    if (NULL == meta || '\0' == meta->title[0]) {
        add_line(NULL, MODAL_LABEL, MAP_DETAIL_FONT_LABEL, -1, "LOADING QUEST DETAILS...");
    } else {
        if ('\0' != meta->description[0]) add_line(NULL, MODAL_TEXT, MAP_DETAIL_FONT_LABEL, -1, "%s", meta->description);
        if (IMAP_QUEST_ACTIVE == state && NULL != progress) {
            add_line(NULL, MODAL_TEXT, MAP_DETAIL_FONT_LABEL, -1, "NOW: %s", progress->active_step);
            if ('\0' != progress->objectives_text[0])
                add_line(NULL, MODAL_QUEST, MAP_DETAIL_FONT_LABEL, -1, "%s", progress->objectives_text);
        } else if (0 < meta->step_count) {
            add_line(NULL, MODAL_TEXT, MAP_DETAIL_FONT_LABEL, -1, "FIRST: %s", meta->steps[0].description);
        }
        char rewards[160] = "";
        for (int r = 0; meta->reward_count > r; ++r) {
            char word[MAX_ITEM_ID_LENGTH + 16];
            snprintf(word, sizeof word, "%s x%d", meta->rewards[r].item_id, meta->rewards[r].quantity);
            append_word(rewards, sizeof rewards, word);
        }
        if ('\0' != rewards[0]) add_line(NULL, MODAL_POSITIVE, MAP_DETAIL_FONT_LABEL, -1, "REWARDS: %s", rewards);
    }

    char unlocks[160] = "";
    const ImapGraph* gr = instance_map_data_graph();
    for (int q = 0; gr->quest_count > q; ++q)
        for (int i = 0; gr->quests[q].link_count > i; ++i)
            if (0 == strcmp(quest->code, gr->quests[q].links[i]))
                append_word(unlocks, sizeof unlocks, quest_title(gr->quests[q].code));
    if ('\0' != unlocks[0]) add_line(NULL, MODAL_LABEL, MAP_DETAIL_FONT_LABEL, -1, "UNLOCKS: %s", unlocks);
}

static void build_action(const ImapProvider* action, const ImapPresencePoi* poi, const char* place) {
    add_line(NULL, MODAL_LABEL, MAP_DETAIL_FONT_LABEL, -1, "ACTION %s - %s", action->code, place);
    if (0 == action->services) add_line(NULL, MODAL_LABEL, MAP_DETAIL_FONT_LABEL, -1, "NO SERVICES CONFIGURED");
    else add_line(NULL, MODAL_LABEL, MAP_DETAIL_FONT_LABEL, -1, "CONFIGURED SERVICES");
    for (int s = 0; IMAP_SERVICE_COUNT > s; ++s)
        if (action->services & (1u << s))
            add_line(SERVICE_ICONS[s], MODAL_TEXT, MAP_DETAIL_FONT_BODY, -1, "%s", SERVICE_NAMES[s]);
    for (int i = 0; action->link_count > i && RELATED_MAX > i; ++i) {
        ImapQuestState state = instance_map_quest_state(action->links[i]);
        add_line(map_detail_quest_icon(state), MODAL_QUEST, MAP_DETAIL_FONT_LABEL, -1, "QUEST %s - %s",
                 quest_title(action->links[i]), map_detail_quest_word(state));
    }
    if (poi->action_active) add_line("check", MODAL_POSITIVE, MAP_DETAIL_FONT_BODY, -1, "FOR YOU: OPEN NOW");
}

/* A bot or a resource the content places: what it is, its class, its stat sum as
 * placed, its place, and what the mark shows. */
static void build_presence(const ImapPresencePoi* poi, const char* place) {
    ImapPresenceStatus status = poi->presence_status;
    Color color = instance_map_presence_color(status);
    switch (status) {
        case IMAP_PRESENCE_HOSTILE:
            copy_str(s_title, sizeof s_title, "Hostile bot");
            add_presence_line(status, color, "HOSTILE - CAN ATTACK YOU");
            break;
        case IMAP_PRESENCE_RESOURCE:
            copy_str(s_title, sizeof s_title, "Resource");
            add_presence_line(status, color, "RESOURCE - YOU CAN EXTRACT IT");
            break;
        case IMAP_PRESENCE_PASSIVE:
        default:
            copy_str(s_title, sizeof s_title, "Passive bot");
            add_presence_line(status, color, "PASSIVE - NON-AGGRESSIVE");
            break;
    }
    if (0 <= poi->first_placed) {
        const ImapPlaced* placed = &instance_map_data_graph()->placed[poi->first_placed];
        copy_str(s_title, sizeof s_title, placed->item_id);
        add_line("stack", MODAL_TEXT, MAP_DETAIL_FONT_LABEL, -1, "STAT SUM %d AS PLACED", placed->stats);
    }
    add_line("map", MODAL_TEXT, MAP_DETAIL_FONT_BODY, -1, "%s", place);
    const char* note = IMAP_PRESENCE_RESOURCE == status ? "PLACED HERE - IT STAYS ON ITS CELL"
                                                        : "PLACED HERE - IT MOVES IN PLAY";
    add_line(NULL, MODAL_LABEL, MAP_DETAIL_FONT_LABEL, -1, "%s", note);
    add_target_lines((int)(poi - instance_map_data_graph()->presence_pois));
}

/* The followed objective: what to do, its step, and where: a page of its
 * destinations nearest first, the one in focus marked, a row to the next page,
 * then a row to stop following. The rows fit in `lines`; the page starts at
 * row `first`, or holds the destination in focus when `first` is -1. */
static void build_objective(int lines, int first) {
    const QuestTarget* target = quest_target_tracked();
    if (NULL == target) return;
    const QuestObjectiveProgress* o = &target->objective;
    copy_str(s_title, sizeof s_title, quest_title(target->quest_code));
    add_line(OBJECTIVE_ICONS[o->type], MODAL_QUEST, MAP_DETAIL_FONT_BODY, -1, "%s %s %d/%d",
             quest_objective_verb(o->type), o->item_id, o->current, o->required);
    const QuestProgressEntry* progress = quest_progress_store_find(target->quest_code);
    if (NULL != progress && '\0' != progress->active_step[0])
        add_line(NULL, MODAL_LABEL, MAP_DETAIL_FONT_LABEL, -1, "STEP: %s", progress->active_step);
    if (QUEST_TARGET_UNREACHABLE == target->state)
        add_line(NULL, MODAL_LABEL, MAP_DETAIL_FONT_LABEL, -1, "NO KNOWN PORTAL ROUTE FROM YOUR MAP");
    int count = target->destination_count, focus = quest_target_tracked_destination();
    DetailLine* places = 0 < count ? add_line(NULL, MODAL_LABEL, MAP_DETAIL_FONT_LABEL, -1, "%s", "") : NULL;
    int shown = DESTINATION_ROWS_MAX < count ? DESTINATION_ROWS_MAX : count;
    int room = lines - s_line_count - 1;                          /* above STOP FOLLOWING */
    if (count > shown || room < shown) room--;                    /* the page row */
    if (room < shown) shown = 0 > room ? 0 : room;
    if (0 > first || count <= first) first = 0 < shown ? focus / shown * shown : 0;
    int last = count < first + shown ? count : first + shown;
    if (NULL != places && shown < count)
        snprintf(places->text, sizeof places->text, "PLACES %d-%d OF %d - NEAREST FIRST", first + 1, last, count);
    else if (NULL != places)
        snprintf(places->text, sizeof places->text, "ALL %d PLACE%s - NEAREST FIRST", count, plural(count));
    for (int d = first; last > d; d++) {
        const QuestDestination* destination = &target->destinations[d];
        const ImapPresencePoi* poi = &instance_map_data_graph()->presence_pois[destination->poi];
        char map[IMAP_NAME_MAX * 2], ways[48], way[52] = "", route[16];
        instance_map_node_label(instance_map_data_graph(), poi->node, map, sizeof map);
        if (QUEST_OBJECTIVE_COLLECT == o->type) {
            quest_target_ways(destination, ways, sizeof ways);
            snprintf(way, sizeof way, "%s ", ways);
        }
        if (0 > destination->hops) snprintf(route, sizeof route, "NO ROUTE");
        else if (0 == destination->hops) snprintf(route, sizeof route, "HERE");
        else snprintf(route, sizeof route, "%d PORTAL%s", destination->hops, plural(destination->hops));
        add_line(d == focus ? "target" : "", d == focus ? MODAL_QUEST : MODAL_TEXT, MAP_DETAIL_FONT_LABEL, d,
                 "%s - %s(%d, %d) %s", route, way, poi->cell_x, poi->cell_y, map);
    }
    s_next_page = count > last ? last : 0;
    if (shown < count && count > last)
        add_line("arrow-right", MODAL_TEXT, MAP_DETAIL_FONT_LABEL, MAP_DETAIL_PAGE, "MORE: %d-%d OF %d", last + 1,
                 count < last + shown ? count : last + shown, count);
    else if (shown < count)
        add_line("arrow-left", MODAL_TEXT, MAP_DETAIL_FONT_LABEL, MAP_DETAIL_PAGE, "BACK TO NEAREST");
    add_line("close", MODAL_LABEL, MAP_DETAIL_FONT_LABEL, MAP_DETAIL_UNFOLLOW, "STOP FOLLOWING");
}

/* The entry spawn of the world: where a new player starts, and what it is not. */
static void build_spawn(void) {
    const ImapEntrySpawn* spawn = &instance_map_data_graph()->entry_spawn;
    bool fixed = IMAP_SPAWN_FIXED == spawn->mode;
    copy_str(s_title, sizeof s_title, "Entry spawn");
    char entry[IMAP_NAME_MAX + 32];
    map_detail_entry_text(spawn, entry, sizeof entry);
    add_line("home", fixed ? MAP_MARKER_SPAWN : MODAL_LABEL, MAP_DETAIL_FONT_BODY, -1, "%s", entry);
    if (fixed) {
        add_line(NULL, MODAL_TEXT, MAP_DETAIL_FONT_LABEL, -1, "NEW PLAYERS ENTER THE WORLD HERE");
        add_line(NULL, MODAL_LABEL, MAP_DETAIL_FONT_LABEL, -1, "BLOCKED CELL: RANDOM CELL INSTEAD");
    } else {
        if (IMAP_SPAWN_UNRESOLVED == spawn->mode)
            add_line(NULL, MODAL_LABEL, MAP_DETAIL_FONT_LABEL, -1, "CONFIG CELL NOT IN THIS WORLD");
        add_line(NULL, MODAL_TEXT, MAP_DETAIL_FONT_LABEL, -1, "NEW PLAYERS ENTER ON A RANDOM CELL");
    }
    add_line(NULL, MODAL_LABEL, MAP_DETAIL_FONT_LABEL, -1, "SAVED PLAYERS: BACK TO LAST CELL");
    add_line(NULL, MODAL_LABEL, MAP_DETAIL_FONT_LABEL, -1, "A DEFEAT REVIVES IN PLACE");
}

static void build_place(int index, int item) {
    const ImapPresencePoi* poi = &instance_map_data_graph()->presence_pois[index];
    char place[IMAP_NAME_MAX * 2 + 24];
    place_name(poi, place, sizeof place);
    int items = map_detail_place_items(index);
    if (0 == items) {
        build_presence(poi, place);
        return;
    }
    if (1 == items) item = 0;

    if (0 > item) {
        copy_str(s_title, sizeof s_title, place);
        for (int i = 0; items > i && PLACE_ROWS_MAX > i; ++i) {
            bool is_quest = false;
            const ImapProvider* p = place_item(index, i, &is_quest);
            if (is_quest) {
                ImapQuestState state = instance_map_quest_state(p->code);
                add_line(map_detail_quest_icon(state), IMAP_QUEST_LOCKED == state ? MODAL_LABEL : MODAL_QUEST,
                         MAP_DETAIL_FONT_BODY, i, "%s - %s", quest_title(p->code), map_detail_quest_word(state));
                continue;
            }
            const char* icon = "hand";
            int services = 0;
            for (int s = IMAP_SERVICE_COUNT - 1; 0 <= s; --s)
                if (p->services & (1u << s)) {
                    icon = SERVICE_ICONS[s];
                    services++;
                }
            add_line(icon, MODAL_TEXT, MAP_DETAIL_FONT_BODY, i, "%s - %d SERVICE%s",
                     '\0' != p->label[0] ? p->label : p->code, services, plural(services));
        }
        if (PLACE_ROWS_MAX < items)
            add_line(NULL, MODAL_LABEL, MAP_DETAIL_FONT_LABEL, -1, "+%d MORE", items - PLACE_ROWS_MAX);
        add_target_lines(index);
        return;
    }

    bool is_quest = false;
    const ImapProvider* p = place_item(index, item, &is_quest);
    if (NULL == p) return;
    copy_str(s_title, sizeof s_title, '\0' != p->label[0] ? p->label : p->code);
    if (1 < items) add_line("arrow-left", MODAL_LABEL, MAP_DETAIL_FONT_LABEL, MAP_DETAIL_BACK, "ALL AT THIS PLACE");
    if (is_quest) build_quest(p, place);
    else build_action(p, poi, place);
}

/* Builds the lines of `detail` for a card with room for `lines`. */
static void build(MapDetail detail, int lines) {
    s_line_count = 0;
    s_title[0] = '\0';
    switch (detail.kind) {
        case MAP_DETAIL_MAP:    build_map(detail.index); break;
        case MAP_DETAIL_PORTAL: build_portal(detail.index); break;
        case MAP_DETAIL_PLACE:  build_place(detail.index, detail.item); break;
        case MAP_DETAIL_SPAWN:  build_spawn(); break;
        case MAP_DETAIL_OBJECTIVE: build_objective(lines, detail.item); break;
        case MAP_DETAIL_NONE:
        default:                break;
    }
}

float map_detail_height(MapDetail detail) {
    build(detail, DETAIL_LINES_MAX);
    return MAP_DETAIL_HEADER_H + 2.0f * MAP_DETAIL_PAD + MAP_DETAIL_LINE_H * (float)s_line_count;
}

void map_detail_card(Rectangle card, const char* title, float fade) {
    modal_draw_card(card, true, fade * MODAL_POP_DURATION, 0.0f, MODAL_PANEL_BORDER, MAP_DETAIL_HEADER_H);
    modal_draw_clipped_text(title, (int)(card.x + MAP_DETAIL_PAD),
                            (int)(card.y + (MAP_DETAIL_HEADER_H - MAP_DETAIL_FONT_TITLE) * 0.5f),
                            (int)(card.width - 2.0f * MAP_DETAIL_PAD), MAP_DETAIL_FONT_TITLE, Fade(MODAL_TEXT, fade));
}

void map_detail_line(Rectangle card, float y, float inset, const char* icon, const char* text, int font, Color color,
                     float fade) {
    float x = card.x + MAP_DETAIL_PAD + inset;
    if (NULL != icon) {
        if ('\0' != icon[0])
            ui_icon_draw_ex(icon, x + ICON_SIDE * 0.5f, y + MAP_DETAIL_LINE_H * 0.5f, ICON_SIDE, 0.0f,
                            Fade(WHITE, fade));
        x += ICON_SIDE + 6.0f;
    }
    modal_draw_clipped_text(text, (int)x, (int)(y + (MAP_DETAIL_LINE_H - (float)font) * 0.5f),
                            (int)(card.x + card.width - MAP_DETAIL_PAD - x), font, Fade(color, fade * color.a / 255.0f));
}

void map_detail_draw(Rectangle card, MapDetail detail, float fade) {
    build(detail, (int)((card.height - MAP_DETAIL_HEADER_H - 2.0f * MAP_DETAIL_PAD + 0.5f) / MAP_DETAIL_LINE_H));
    s_row_count = 0;
    map_detail_card(card, s_title, fade);
    float y = card.y + MAP_DETAIL_HEADER_H + MAP_DETAIL_PAD;
    float bottom = card.y + card.height - MAP_DETAIL_PAD;
    for (int i = 0; s_line_count > i && bottom + 0.5f >= y + MAP_DETAIL_LINE_H; ++i, y += MAP_DETAIL_LINE_H) {
        const DetailLine* line = &s_lines[i];
        if (-1 != line->row) {
            Rectangle row = { card.x + MAP_DETAIL_PAD - 4.0f, y + 1.0f, card.width - 2.0f * MAP_DETAIL_PAD + 8.0f,
                              MAP_DETAIL_LINE_H - 2.0f };
            bool hovered = CheckCollisionPointRec(GetMousePosition(), row);
            DrawRectangleRec(row, Fade(MODAL_BUTTON, fade * (hovered ? 1.0f : 0.7f)));
            if (0 <= line->row)
                DrawText(">", (int)(row.x + row.width - 10.0f),
                         (int)(y + (MAP_DETAIL_LINE_H - MAP_DETAIL_FONT_LABEL) * 0.5f), MAP_DETAIL_FONT_LABEL,
                         Fade(MODAL_LABEL, fade));
            s_row_rects[s_row_count] = row;
            s_row_ids[s_row_count++] = line->row;
        }
        if (IMAP_PRESENCE_NONE != line->glyph) {
            Vector2 slot = { card.x + MAP_DETAIL_PAD + ICON_SIDE * 0.5f, y + MAP_DETAIL_LINE_H * 0.5f };
            map_marker_draw_presence(slot, MAP_MARKER_PRESENCE_MAX * 0.8f, line->glyph, 1, fade);
        }
        map_detail_line(card, y, 0.0f, line->icon, line->text, line->font, line->color, fade);
    }
}

int map_detail_next_page(void) { return s_next_page; }

int map_detail_row_at(Vector2 at) {
    for (int i = 0; s_row_count > i; ++i)
        if (CheckCollisionPointRec(at, s_row_rects[i])) return s_row_ids[i];
    return -1;
}
