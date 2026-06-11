/**
 * quest_metadata_cache — client-side cache of quest metadata fetched from REST.
 *
 * The Go server transmits only AUTHORITATIVE quest data over AOI (code,
 * status, progress).  All presentation metadata (title, description, steps,
 * rewards) is fetched lazily from the engine REST endpoint
 * GET /api/cyberia-quest/:code.
 */

#ifndef QUEST_METADATA_CACHE_H
#define QUEST_METADATA_CACHE_H

#include <stdbool.h>

#define QUEST_META_CODE_MAX   64
#define QUEST_META_TITLE_MAX  96
#define QUEST_META_DESC_MAX   256
#define QUEST_META_ITEM_MAX   64
#define QUEST_META_REWARD_MAX 6
#define QUEST_META_PREREQ_MAX 6
#define QUEST_META_STEP_MAX   8
#define QUEST_META_OBJ_MAX    4
#define QUEST_META_STEPDESC_MAX 160
#define QUEST_METADATA_CACHE_CAP 64

typedef enum {
    QUEST_META_NONE = 0,
    QUEST_META_LOADING,
    QUEST_META_READY,
    QUEST_META_ERROR,
} QuestMetaState;

typedef struct {
    char item_id[QUEST_META_ITEM_MAX];
    int  quantity;
} QuestRewardMeta;

typedef struct {
    char type[16]; /* talk | collect | kill */
    char item_id[QUEST_META_ITEM_MAX];
    int  quantity;
} QuestObjectiveMeta;

typedef struct {
    char id[QUEST_META_CODE_MAX];
    char description[QUEST_META_STEPDESC_MAX];
    QuestObjectiveMeta objectives[QUEST_META_OBJ_MAX];
    int  objective_count;
} QuestStepMeta;

typedef struct {
    char    code[QUEST_META_CODE_MAX];
    char    title[QUEST_META_TITLE_MAX];
    char    description[QUEST_META_DESC_MAX];
    int     step_count;
    QuestStepMeta steps[QUEST_META_STEP_MAX];
    QuestRewardMeta rewards[QUEST_META_REWARD_MAX];
    int     reward_count;
    char    prerequisites[QUEST_META_PREREQ_MAX][QUEST_META_CODE_MAX];
    int     prerequisite_count;
    QuestMetaState state;
} QuestMetadataEntry;

void quest_metadata_cache_reset(void);

/* Look up cached metadata by code. Returns NULL if not present. */
const QuestMetadataEntry* quest_metadata_cache_get(const char* code);

/* Return the fetch state for a quest code. */
QuestMetaState quest_metadata_cache_state(const char* code);

/* Schedule an async REST fetch (GET /api/cyberia-quest/code/:code) via
 * engine_client if not already cached/loading. Parses the
 * `{ status, data: <quest doc> }` envelope on completion. */
void quest_metadata_cache_fetch(const char* code);

#endif /* QUEST_METADATA_CACHE_H */
