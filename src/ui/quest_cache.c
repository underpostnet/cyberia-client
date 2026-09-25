/**
 * quest_cache — client-side cache of quest metadata fetched from REST.
 *
 * The Go server transmits only AUTHORITATIVE quest data over AOI (code,
 * status, progress).  All presentation metadata (title, description, steps,
 * rewards) is fetched lazily from the engine REST endpoint
 * GET /api/v1/cyberia-quest/code/:code and cached here so the quest journal and
 * action tab can render rich details without blocking.  Metadata is immutable,
 * so each code is fetched at most once per session.
 */

#include "quest_cache.h"
#include "quest_progress_store.h"

#include "meta_cache.h"
#include "network/data/engine_client.h"
#include "util/utils.h"

#include <cJSON.h>
#include <string.h>

static QuestMetadataEntry s_entries[QUEST_CACHE_CAP];
static void ingest_quest_doc(void* entry, const cJSON* doc);

static MetaCache s_cache = {
    .entries    = s_entries,
    .elem_size  = sizeof(QuestMetadataEntry),
    .cap        = QUEST_CACHE_CAP,
    .url_prefix = ENGINE_API_BASE "/cyberia-quest/code/",
    .label      = "quest",
    .ingest     = ingest_quest_doc,
};

const QuestMetadataEntry* quest_cache_get(const char* code) {
    return meta_cache_find(&s_cache, code);
}

static void on_quest_fetched(const FetchResponse* r) {
    meta_cache_on_fetched(&s_cache, r);
}

void quest_cache_fetch(const char* code) {
    meta_cache_fetch(&s_cache, code, on_quest_fetched);
}

int quest_active_step_index(const QuestMetadataEntry* metadata,
                            const QuestProgressEntry* progress) {
    if (NULL == metadata || NULL == progress || '\0' == progress->active_step[0]) return 0;
    for (int i = 0; i < metadata->step_count; i++) {
        const QuestStepMeta* step = &metadata->steps[i];
        if (0 == strcmp(step->id, progress->active_step) ||
            0 == strcmp(step->description, progress->active_step)) {
            return i;
        }
    }
    return 0;
}

/* Parse the quest doc (`data` object of the engine envelope) into entry, then
 * mirror title and description into quest_progress_store so the journal and
 * action tab render without polling. */
static void ingest_quest_doc(void* entry, const cJSON* doc) {
    QuestMetadataEntry* e = entry;

    copy_str(e->title, QUEST_TITLE_MAX, json_str(doc, "title"));
    copy_str(e->description, QUEST_DESC_MAX, json_str(doc, "description"));

    e->step_count = 0;
    const cJSON* steps = cJSON_GetObjectItemCaseSensitive(doc, "steps");
    if (cJSON_IsArray(steps)) {
        const cJSON* st = NULL;
        cJSON_ArrayForEach(st, steps) {
            if (e->step_count >= QUEST_CACHE_STEP_MAX) break;
            QuestStepMeta* sm = &e->steps[e->step_count];
            memset(sm, 0, sizeof(*sm));
            copy_str(sm->id, META_CACHE_CODE_MAX, json_str(st, "id"));
            copy_str(sm->description, QUEST_STEP_MAX, json_str(st, "description"));

            const cJSON* objs = cJSON_GetObjectItemCaseSensitive(st, "objectives");
            if (cJSON_IsArray(objs)) {
                const cJSON* o = NULL;
                cJSON_ArrayForEach(o, objs) {
                    if (sm->objective_count >= QUEST_CACHE_OBJ_MAX) break;
                    QuestObjectiveMeta* om = &sm->objectives[sm->objective_count];
                    copy_str(om->type, sizeof(om->type), json_str(o, "type"));
                    copy_str(om->item_id, MAX_ITEM_ID_LENGTH, json_str(o, "itemId"));
                    om->quantity = json_int(o, "quantity", 1);
                    sm->objective_count++;
                }
            }
            e->step_count++;
        }
    }

    e->reward_count = 0;
    const cJSON* rewards = cJSON_GetObjectItemCaseSensitive(doc, "rewards");
    if (cJSON_IsArray(rewards)) {
        const cJSON* r = NULL;
        cJSON_ArrayForEach(r, rewards) {
            if (e->reward_count >= QUEST_CACHE_REWARD_MAX) break;
            const char* item_id = json_str(r, "itemId");
            if (NULL == item_id) continue;
            QuestRewardMeta* rm = &e->rewards[e->reward_count];
            copy_str(rm->item_id, MAX_ITEM_ID_LENGTH, item_id);
            rm->quantity = json_int(r, "quantity", 1);
            e->reward_count++;
        }
    }

    e->prerequisite_count = 0;
    const cJSON* prereqs = cJSON_GetObjectItemCaseSensitive(doc, "prerequisiteCodes");
    if (cJSON_IsArray(prereqs)) {
        const cJSON* p = NULL;
        cJSON_ArrayForEach(p, prereqs) {
            if (e->prerequisite_count >= QUEST_CACHE_PREREQ_MAX) break;
            if (!cJSON_IsString(p) || p->valuestring[0] == '\0') continue;
            copy_str(e->prerequisites[e->prerequisite_count], META_CACHE_CODE_MAX, p->valuestring);
            e->prerequisite_count++;
        }
    }

    quest_progress_store_set_meta(e->head.code, e->title, e->description);
}
