/**
 * quest_cache — client-side cache of quest metadata fetched from REST.
 *
 * The Go server transmits only AUTHORITATIVE quest data over AOI (code,
 * status, progress).  All presentation metadata (title, description, steps,
 * rewards) is fetched lazily from the engine REST endpoint
 * GET /api/cyberia-quest/code/:code and cached here so the quest journal and
 * action tab can render rich details without blocking.  Metadata is immutable,
 * so each code is fetched at most once per session.
 */

#include "quest_cache.h"
#include "quest_progress_store.h"

#include "meta_cache.h"
#include "util/utils.h"

#include <cJSON.h>
#include <string.h>

static QuestMetadataEntry s_entries[QUEST_CACHE_CAP];
static void ingest_quest_doc(void* entry, const cJSON* doc);

static MetaCache s_cache = {
    .entries    = s_entries,
    .elem_size  = sizeof(QuestMetadataEntry),
    .cap        = QUEST_CACHE_CAP,
    .url_prefix = "/api/cyberia-quest/code/",
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

/* Parse the quest doc (`data` object of the engine envelope) into entry, then
 * mirror title and description into quest_progress_store so the journal and
 * action tab render without polling. */
static void ingest_quest_doc(void* entry, const cJSON* doc) {
    QuestMetadataEntry* e = entry;

    const cJSON* title = cJSON_GetObjectItemCaseSensitive(doc, "title");
    if (cJSON_IsString(title)) copy_str(e->title, QUEST_CACHE_TITLE_MAX, title->valuestring);

    const cJSON* desc = cJSON_GetObjectItemCaseSensitive(doc, "description");
    if (cJSON_IsString(desc)) copy_str(e->description, QUEST_CACHE_DESC_MAX, desc->valuestring);

    e->step_count = 0;
    const cJSON* steps = cJSON_GetObjectItemCaseSensitive(doc, "steps");
    if (cJSON_IsArray(steps)) {
        const cJSON* st = NULL;
        cJSON_ArrayForEach(st, steps) {
            if (e->step_count >= QUEST_CACHE_STEP_MAX) break;
            QuestStepMeta* sm = &e->steps[e->step_count];
            memset(sm, 0, sizeof(*sm));
            const cJSON* id = cJSON_GetObjectItemCaseSensitive(st, "id");
            if (cJSON_IsString(id)) copy_str(sm->id, QUEST_CACHE_CODE_MAX, id->valuestring);
            const cJSON* sdesc = cJSON_GetObjectItemCaseSensitive(st, "description");
            if (cJSON_IsString(sdesc)) copy_str(sm->description, QUEST_CACHE_STEPDESC_MAX, sdesc->valuestring);

            const cJSON* objs = cJSON_GetObjectItemCaseSensitive(st, "objectives");
            if (cJSON_IsArray(objs)) {
                const cJSON* o = NULL;
                cJSON_ArrayForEach(o, objs) {
                    if (sm->objective_count >= QUEST_CACHE_OBJ_MAX) break;
                    QuestObjectiveMeta* om = &sm->objectives[sm->objective_count];
                    const cJSON* type = cJSON_GetObjectItemCaseSensitive(o, "type");
                    const cJSON* item = cJSON_GetObjectItemCaseSensitive(o, "itemId");
                    const cJSON* qty = cJSON_GetObjectItemCaseSensitive(o, "quantity");
                    if (cJSON_IsString(type)) copy_str(om->type, sizeof(om->type), type->valuestring);
                    if (cJSON_IsString(item)) copy_str(om->item_id, QUEST_CACHE_ITEM_MAX, item->valuestring);
                    om->quantity = cJSON_IsNumber(qty) ? qty->valueint : 1;
                    sm->objective_count++;
                }
            }
            e->step_count++;
        }
    }

    const cJSON* smc = cJSON_GetObjectItemCaseSensitive(doc, "sourceMapCode");
    if (cJSON_IsString(smc)) copy_str(e->source_map_code, QUEST_CACHE_CODE_MAX, smc->valuestring);
    const cJSON* scx = cJSON_GetObjectItemCaseSensitive(doc, "sourceCellX");
    e->source_cell_x = cJSON_IsNumber(scx) ? scx->valueint : 0;
    const cJSON* scy = cJSON_GetObjectItemCaseSensitive(doc, "sourceCellY");
    e->source_cell_y = cJSON_IsNumber(scy) ? scy->valueint : 0;

    e->reward_count = 0;
    const cJSON* rewards = cJSON_GetObjectItemCaseSensitive(doc, "rewards");
    if (cJSON_IsArray(rewards)) {
        const cJSON* r = NULL;
        cJSON_ArrayForEach(r, rewards) {
            if (e->reward_count >= QUEST_CACHE_REWARD_MAX) break;
            const cJSON* item_id = cJSON_GetObjectItemCaseSensitive(r, "itemId");
            if (!cJSON_IsString(item_id)) continue;
            const cJSON* qty = cJSON_GetObjectItemCaseSensitive(r, "quantity");
            QuestRewardMeta* rm = &e->rewards[e->reward_count];
            copy_str(rm->item_id, QUEST_CACHE_ITEM_MAX, item_id->valuestring);
            rm->quantity = cJSON_IsNumber(qty) ? qty->valueint : 1;
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
            copy_str(e->prerequisites[e->prerequisite_count], QUEST_CACHE_CODE_MAX, p->valuestring);
            e->prerequisite_count++;
        }
    }

    quest_progress_store_set_meta(e->head.code, e->title, e->description);
}
