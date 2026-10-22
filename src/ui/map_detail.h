#ifndef CYBERIA_UI_MAP_DETAIL_H
#define CYBERIA_UI_MAP_DETAIL_H

#include "data/instance_map_data.h"

#include <raylib.h>
#include <stddef.h>

/* map_detail — the detail card of the Instance Map: the chrome every card
 * shares, and what the card says about a selected map, portal, place, entry
 * spawn or followed quest objective.
 *
 * A place is a cell with actions or quests. Its card lists them; a row opens
 * one. A cell with neither is a placed bot or resource. The card shows what the
 * content configures, and the player's own state only where the quest store
 * establishes it. */

#define MAP_DETAIL_PAD      12.0f
#define MAP_DETAIL_HEADER_H 32.0f
#define MAP_DETAIL_LINE_H   20.0f
#define MAP_DETAIL_FONT_TITLE 16
#define MAP_DETAIL_FONT_BODY  13
#define MAP_DETAIL_FONT_LABEL 12

/* What the card shows. `index` is a node, an edge or a POI of the graph; the entry spawn has none. */
typedef enum {
    MAP_DETAIL_NONE = 0, MAP_DETAIL_MAP, MAP_DETAIL_PORTAL, MAP_DETAIL_PLACE, MAP_DETAIL_SPAWN, MAP_DETAIL_OBJECTIVE
} MapDetailKind;

/* `item` is a row of a place: -1 for the list, else the quest or action it
 * opens. On the objective card it is the first destination row shown: -1 for
 * the page that holds the destination in focus. */
typedef struct {
    MapDetailKind kind;
    int index;
    int item;
} MapDetail;

/* A row tap that goes back to the list of a place. */
#define MAP_DETAIL_BACK (-2)

/* A row tap that stops following the quest objective. On the objective card,
 * any other row is a destination. */
#define MAP_DETAIL_UNFOLLOW (-3)

/* A row tap on a place that follows the objective quest_target_get(row -
 * MAP_DETAIL_FOLLOW) there. No item of a place reaches it. */
#define MAP_DETAIL_FOLLOW (2 * IMAP_MAX_PROVIDERS)

/* A row tap on the objective card that shows the next page of destinations,
 * from map_detail_next_page(). */
#define MAP_DETAIL_PAGE (-4)

/* The first destination row of the page after the last built objective card;
 * 0 after the last page. */
int map_detail_next_page(void);

/* The name and the icon of a service, and the word and the icon of a quest state. */
const char* map_detail_service_name(ImapService service);
const char* map_detail_service_icon(ImapService service);
const char* map_detail_quest_word(ImapQuestState state);
const char* map_detail_quest_icon(ImapQuestState state);

/* Where the entry spawn of a world puts a new player: a map and a cell, or a random cell. "" when unknown. */
void map_detail_entry_text(const ImapEntrySpawn* spawn, char* out, size_t size);

/* The items of a place: its quests, then its actions. */
int map_detail_place_items(int poi);

/* The height of the card of `detail`. */
float map_detail_height(MapDetail detail);

/* Draws the card of `detail` in `card`, and keeps its rows for map_detail_row_at. */
void map_detail_draw(Rectangle card, MapDetail detail, float fade);

/* The row id of the last drawn card under `at`, or -1. */
int map_detail_row_at(Vector2 at);

/* The chrome of a detail card, with `title` in its header. */
void map_detail_card(Rectangle card, const char* title, float fade);

/* One line of a card at `y`, `inset` px past the padding, led by `icon` when it names one. */
void map_detail_line(Rectangle card, float y, float inset, const char* icon, const char* text, int font, Color color,
                     float fade);

#endif /* CYBERIA_UI_MAP_DETAIL_H */
