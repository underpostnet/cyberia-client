/**
 * quest_progress_store — client-local quest state for the Quest Journal.
 *
 * The Go server's init_data snapshot seeds the store on connect; subsequent
 * dlg_ack events keep it live (granted quests appear, completed quests move
 * sections).  Only AUTHORITATIVE data travels over AOI — code, status, the
 * step a quest is on and the count of each of its objectives.  Metadata
 * (title, description, steps, rewards) is fetched by quest_cache from the
 * engine REST endpoint GET /api/v1/cyberia-quest/:code.
 *
 * Cleared and repopulated on reconnect.
 */

#ifndef QUEST_PROGRESS_STORE_H
#define QUEST_PROGRESS_STORE_H

#include <stdbool.h>
#include "meta_cache.h"
#include "world/object_layer.h"

#define QUEST_TITLE_MAX       96
#define QUEST_DESC_MAX        256
#define QUEST_STEP_MAX        160
#define QUEST_OBJECTIVES_MAX  160
#define QUEST_PROGRESS_STORE_CAP       256
#define QUEST_STEP_OBJECTIVES_MAX 4

typedef enum {
    QUEST_ACTIVE = 0,
    QUEST_COMPLETED,
    QUEST_FAILED,
    QUEST_STATUS_COUNT,
} QuestStatus;

/* What a live update did to a quest. The quest card shows it once. */
typedef enum {
    QUEST_CHANGE_NONE = 0,
    QUEST_CHANGE_ACCEPTED,
    QUEST_CHANGE_STEP_DONE,
    QUEST_CHANGE_COMPLETED,
    QUEST_CHANGE_REWARDS_HELD,  /* every step is done; the rewards wait for room */
} QuestChange;

/* What an objective asks: kill a bot of a skin, talk to an NPC of a skin, or
 * hold an item. */
typedef enum {
    QUEST_OBJECTIVE_KILL = 0,
    QUEST_OBJECTIVE_TALK,
    QUEST_OBJECTIVE_COLLECT,
    QUEST_OBJECTIVE_TYPE_COUNT,
} QuestObjectiveType;

/* One objective of the step a quest is on, as the server counts it. */
typedef struct {
    QuestObjectiveType type;
    char item_id[MAX_ITEM_ID_LENGTH];  /* kill, talk: a skin; collect: an item */
    int  current, required;
} QuestObjectiveProgress;

/* One quest of a server snapshot (init_data, dialog_ack). */
typedef struct {
    const char* code;
    const char* status;       /* "active" | "completed" | "failed" */
    const char* active_step;  /* the description of the step it is on */
    const char* step_id;      /* that step; "" once every step is done */
    QuestObjectiveProgress objectives[QUEST_STEP_OBJECTIVES_MAX];
    int  objective_count;
    bool rewards_held;
} QuestSnapshot;

/* The type an objective names ("kill", "talk", "collect"), and its name. An
 * unknown name gives QUEST_OBJECTIVE_TYPE_COUNT. */
QuestObjectiveType quest_objective_type(const char* name);
const char* quest_objective_type_name(QuestObjectiveType type);

/* What the player does for an objective: "Defeat", "Talk to", "Collect". */
const char* quest_objective_verb(QuestObjectiveType type);

typedef struct {
    char        code[META_CACHE_CODE_MAX];
    char        title[QUEST_TITLE_MAX];
    char        description[QUEST_DESC_MAX];
    char        active_step[QUEST_STEP_MAX];
    char        step_id[META_CACHE_CODE_MAX];
    QuestObjectiveProgress objectives[QUEST_STEP_OBJECTIVES_MAX];
    int         objective_count;
    char        objectives_text[QUEST_OBJECTIVES_MAX];  /* "1/2 kill rat, 0/1 talk sage" */
    QuestStatus status;
    bool        rewards_held; /* the rewards wait for a free inventory slot */
    QuestChange change;     /* not yet shown; see quest_progress_store_take_change */
} QuestProgressEntry;

void quest_progress_store_reset(void);

/* Rises with every change of a status or a progress; readers recompute what
 * they derive from the store when it moves. */
int  quest_progress_store_revision(void);

/* Insert or update by code, from init_data. An unknown status is active. A new
 * entry takes its title and description from quest_cache. Records no change. */
void quest_progress_store_upsert(const QuestSnapshot* snapshot);

/* Upsert from dialog_ack, and record the change against the prior state.
 * `granted` is true for the quest the server just granted. */
void quest_progress_store_apply(const QuestSnapshot* snapshot, bool granted);

/* Entry with an unshown change, which is cleared. NULL when none is left. */
const QuestProgressEntry* quest_progress_store_take_change(QuestChange* change);

/* Number of entries currently in a given section. */
int quest_progress_store_count(QuestStatus status);

/* index is 0-based within the section filter; NULL if out of range. */
const QuestProgressEntry* quest_progress_store_get(QuestStatus status, int index);

/* True if a quest with this code is already tracked as completed. */
bool quest_progress_store_is_completed(const char* code);

/* Look up an entry by code (any status), or NULL. */
const QuestProgressEntry* quest_progress_store_find(const char* code);

/* Update title + description on an existing quest entry (from metadata cache).
 * Returns true if the entry existed.  No-op when code is unknown. */
bool quest_progress_store_set_meta(const char* code, const char* title,
                          const char* description);

#endif /* QUEST_PROGRESS_STORE_H */
