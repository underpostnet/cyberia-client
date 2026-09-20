/**
 * quest_cache — client-side cache of quest metadata fetched from REST.
 *
 * The Go server transmits only AUTHORITATIVE quest data over AOI (code,
 * status, progress).  All presentation metadata (title, description, steps,
 * rewards) is fetched lazily from the engine REST endpoint
 * GET /api/cyberia-quest/:code.
 */

#ifndef QUEST_CACHE_H
#define QUEST_CACHE_H

#include "meta_cache.h"
#include "object_layer.h"
#include "quest_progress_store.h"

#define QUEST_CACHE_REWARD_MAX 6
#define QUEST_CACHE_PREREQ_MAX 6
#define QUEST_CACHE_STEP_MAX   8
#define QUEST_CACHE_OBJ_MAX    4
#define QUEST_CACHE_CAP 64

typedef struct {
    char item_id[MAX_ITEM_ID_LENGTH];
    int  quantity;
} QuestRewardMeta;

typedef struct {
    char type[16]; /* talk | collect | kill */
    char item_id[MAX_ITEM_ID_LENGTH];
    int  quantity;
} QuestObjectiveMeta;

typedef struct {
    char id[META_CACHE_CODE_MAX];
    char description[QUEST_STEP_MAX];
    QuestObjectiveMeta objectives[QUEST_CACHE_OBJ_MAX];
    int  objective_count;
} QuestStepMeta;

typedef struct {
    MetaCacheHead head;
    char    title[QUEST_TITLE_MAX];
    char    description[QUEST_DESC_MAX];
    int     step_count;
    QuestStepMeta steps[QUEST_CACHE_STEP_MAX];
    QuestRewardMeta rewards[QUEST_CACHE_REWARD_MAX];
    int     reward_count;
    char    prerequisites[QUEST_CACHE_PREREQ_MAX][META_CACHE_CODE_MAX];
    int     prerequisite_count;
} QuestMetadataEntry;

/* Look up cached metadata by code. Returns NULL if not present. */
const QuestMetadataEntry* quest_cache_get(const char* code);

/* Schedule an async REST fetch (GET /api/cyberia-quest/code/:code) via
 * engine_client if not already cached/loading. Parses the
 * `{ status, data: <quest doc> }` envelope on completion. */
void quest_cache_fetch(const char* code);

/* Index of the step the progress entry is on. The server puts the step
 * description into `active_step`, so match the description as well as the id.
 * Returns 0 when there is no match. */
int quest_active_step_index(const QuestMetadataEntry* metadata,
                            const QuestProgressEntry* progress);

#endif /* QUEST_CACHE_H */
