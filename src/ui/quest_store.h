/**
 * quest_store — client-local quest state for the Quest Journal.
 *
 * The Go server's init_data snapshot seeds the store on connect; subsequent
 * dlg_ack events keep it live (granted quests appear, completed quests move
 * sections). No extra REST calls. Cleared and repopulated on reconnect.
 */

#ifndef QUEST_STORE_H
#define QUEST_STORE_H

#include <stdbool.h>

#define QUEST_CODE_MAX        64
#define QUEST_TITLE_MAX       96
#define QUEST_DESC_MAX        256
#define QUEST_STEP_MAX        160
#define QUEST_OBJECTIVES_MAX  160
#define QUEST_STORE_CAP       256

typedef enum {
    QUEST_ACTIVE = 0,
    QUEST_COMPLETED,
    QUEST_FAILED,
    QUEST_STATUS_COUNT,
} QuestStatus;

typedef struct {
    char        code[QUEST_CODE_MAX];
    char        title[QUEST_TITLE_MAX];
    char        description[QUEST_DESC_MAX];
    char        active_step[QUEST_STEP_MAX];
    char        objectives[QUEST_OBJECTIVES_MAX];
    QuestStatus status;
} QuestEntry;

void quest_store_reset(void);

/* Insert or update by code. `status_str` is "active" | "completed" | "failed";
 * unknown values default to active. Returns true if a new entry was added. */
bool quest_store_upsert(const char* code, const char* title, const char* description,
                        const char* status_str, const char* active_step,
                        const char* objectives);

QuestStatus quest_store_parse_status(const char* status_str);

/* Number of entries currently in a given section. */
int quest_store_count(QuestStatus status);

/* index is 0-based within the section filter; NULL if out of range. */
const QuestEntry* quest_store_get(QuestStatus status, int index);

#endif /* QUEST_STORE_H */
