#ifndef CYBERIA_DATA_QUEST_TARGET_H
#define CYBERIA_DATA_QUEST_TARGET_H

#include "instance_map_data.h"
#include "quest_progress_store.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* quest_target — where the player can advance each objective of its active
 * quests in this world. It reads the server's counts from the quest store and
 * the authored placements of the static world map; it never decides progress.
 *
 *   kill    — a cell with a mortal bot that wears the skin the objective names
 *   talk    — a cell with a bot that wears that skin
 *   collect — a cell that gives the item: what is placed there drops it, or
 *             its action sells or assembles it
 *
 * An objective is actionable while its quest is active, it belongs to the step
 * the quest is on, and its count is under the required one. Its identity is the
 * quest code, the step id, its type and its item id. The followed quest keeps
 * one objective in focus, and one of its destinations: a destination the player
 * picks stays in focus while it serves the objective. */

#define QUEST_TARGETS_MAX      32

typedef enum {
    QUEST_TARGET_PENDING = 0,  /* the world map has not loaded           */
    QUEST_TARGET_FOUND,        /* cells on maps a portal route reaches   */
    QUEST_TARGET_UNREACHABLE,  /* cells, none on a map a known portal
                                * route from the player's map reaches    */
    QUEST_TARGET_ABSENT,       /* kill, talk: no such bot in this world  */
    QUEST_TARGET_NO_SOURCE,    /* collect: nothing in this world gives it */
} QuestTargetState;

/* How a cell advances an objective. */
typedef enum {
    QUEST_VIA_KILL = 0,
    QUEST_VIA_TALK,
    QUEST_VIA_DROP,
    QUEST_VIA_SHOP,
    QUEST_VIA_CRAFT,
    QUEST_VIA_COUNT,
} QuestVia;

typedef struct {
    int     poi;    /* presence POI of the world map */
    uint8_t vias;   /* 1 << QuestVia */
    int     hops;   /* portals from the player's map; -1 without a known route */
} QuestDestination;

typedef struct {
    char quest_code[META_CACHE_CODE_MAX];
    char step_id[META_CACHE_CODE_MAX];
    QuestObjectiveProgress objective;
    QuestTargetState state;
    QuestDestination destinations[IMAP_MAX_PRESENCE_POIS];  /* every cell, nearest first */
    int  destination_count;
} QuestTarget;

/* Resolves the targets again when the quest store, the world map or the
 * player's map changed. Call once per frame from each view that shows them;
 * it updates map_route first. */
void quest_target_update(void);

int quest_target_count(void);
const QuestTarget* quest_target_get(int index);

/* The target of one objective, or NULL when it is not actionable. */
const QuestTarget* quest_target_find(const char* quest_code, const char* step_id, QuestObjectiveType type,
                                     const char* item_id);

/* Follows the quest of `target`, with `target` and its destination `destination` in focus. */
void quest_target_track(const QuestTarget* target, int destination);
void quest_target_untrack(void);

/* The followed objective, or NULL; and the index of its destination in focus. */
const QuestTarget* quest_target_tracked(void);
int quest_target_tracked_destination(void);

/* The ways a destination advances its objective: "DEFEAT", "BUY/ASSEMBLE". */
void quest_target_ways(const QuestDestination* destination, char* out, size_t size);

/* What the player does at a destination: its ways and its item, "BUY/ASSEMBLE
 * potion"; a talk names the NPC by the label of its action when it has one. */
void quest_target_action_text(const QuestTarget* target, int destination, char* out, size_t size);

#endif /* CYBERIA_DATA_QUEST_TARGET_H */
