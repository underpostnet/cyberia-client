/**
 * quest_store — client-local quest state for the Quest Journal.
 *
 * The Go server's init_data snapshot seeds the store on connect; subsequent
 * dlg_ack events keep it live (granted quests appear, completed quests move
 * sections).  Only AUTHORITATIVE data travels over AOI — code, status, and
 * progress counters.  Metadata (title, description, steps, rewards) is
 * fetched by quest_metadata_cache from the engine REST endpoint
 * GET /api/cyberia-quest/:code.
 *
 * Cleared and repopulated on reconnect.
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

/* True if a quest with this code is already tracked as completed. */
bool quest_store_is_completed(const char* code);

/* Look up an entry by code (any status), or NULL. */
const QuestEntry* quest_store_find(const char* code);

/* Update title + description on an existing quest entry (from metadata cache).
 * Returns true if the entry existed.  No-op when code is unknown. */
bool quest_store_set_meta(const char* code, const char* title,
                          const char* description);

#endif /* QUEST_STORE_H */