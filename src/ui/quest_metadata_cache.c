/**
 * quest_metadata_cache — client-side cache of quest metadata fetched from REST.
 *
 * The Go server transmits only AUTHORITATIVE quest data over AOI (code,
 * status, progress).  All presentation metadata (title, description, steps,
 * rewards) is fetched lazily from the engine REST endpoint
 * GET /api/cyberia-quest/code/:code and cached here so the quest journal and
 * action tab can render rich details without blocking.  Metadata is immutable,
 * so each code is fetched at most once per session.
 */

#include "quest_metadata_cache.h"
#include "quest_store.h"

#include "network/engine_client.h"
#include "util/log.h"

#include <cJSON.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static QuestMetadataEntry s_cache[QUEST_METADATA_CACHE_CAP];
static int                s_count = 0;

static QuestMetadataEntry* find_by_code(const char* code) {
    if (!code) return NULL;
    for (int i = 0; i < s_count; ++i) {
        if (0 == strcmp(s_cache[i].code, code)) return &s_cache[i];
    }
    return NULL;
}

static QuestMetadataEntry* find_or_create(const char* code) {
    QuestMetadataEntry* e = find_by_code(code);
    if (e) return e;
    if (s_count >= QUEST_METADATA_CACHE_CAP) return NULL;
    e = &s_cache[s_count++];
    memset(e, 0, sizeof(QuestMetadataEntry));
    strncpy(e->code, code, QUEST_META_CODE_MAX - 1);
    return e;
}

void quest_metadata_cache_reset(void) {
    s_count = 0;
}

static void copy_str(char* dst, size_t cap, const char* src) {
    if (!src) { dst[0] = '\0'; return; }
    strncpy(dst, src, cap - 1);
    dst[cap - 1] = '\0';
}

const QuestMetadataEntry* quest_metadata_cache_get(const char* code) {
    if (!code) return NULL;
    return find_by_code(code);
}

QuestMetaState quest_metadata_cache_state(const char* code) {
    const QuestMetadataEntry* e = quest_metadata_cache_get(code);
    if (!e) return QUEST_META_NONE;
    return e->state;
}

static void on_quest_fetched(const FetchResponse* r);

void quest_metadata_cache_fetch(const char* code) {
    if (!code || '\0' == code[0]) return;

    QuestMetadataEntry* e = find_or_create(code);
    if (!e) return;
    if (QUEST_META_READY == e->state || QUEST_META_LOADING == e->state) return;

    e->state = QUEST_META_LOADING;

    char url[512];
    snprintf(url, sizeof url, "/api/cyberia-quest/code/%s", code);
    fetch_request_start(code, url, on_quest_fetched);
}

/* Parse the quest doc (`data` object of the engine envelope) into entry. */
static void ingest_quest_doc(QuestMetadataEntry* e, const cJSON* doc) {
    const cJSON* title = cJSON_GetObjectItemCaseSensitive(doc, "title");
    if (cJSON_IsString(title)) copy_str(e->title, QUEST_META_TITLE_MAX, title->valuestring);

    const cJSON* desc = cJSON_GetObjectItemCaseSensitive(doc, "description");
    if (cJSON_IsString(desc)) copy_str(e->description, QUEST_META_DESC_MAX, desc->valuestring);

    e->step_count = 0;
    const cJSON* steps = cJSON_GetObjectItemCaseSensitive(doc, "steps");
    if (cJSON_IsArray(steps)) {
        const cJSON* st = NULL;
        cJSON_ArrayForEach(st, steps) {
            if (e->step_count >= QUEST_META_STEP_MAX) break;
            QuestStepMeta* sm = &e->steps[e->step_count];
            memset(sm, 0, sizeof(*sm));
            const cJSON* id = cJSON_GetObjectItemCaseSensitive(st, "id");
            if (cJSON_IsString(id)) copy_str(sm->id, QUEST_META_CODE_MAX, id->valuestring);
            const cJSON* sdesc = cJSON_GetObjectItemCaseSensitive(st, "description");
            if (cJSON_IsString(sdesc)) copy_str(sm->description, QUEST_META_STEPDESC_MAX, sdesc->valuestring);

            const cJSON* objs = cJSON_GetObjectItemCaseSensitive(st, "objectives");
            if (cJSON_IsArray(objs)) {
                const cJSON* o = NULL;
                cJSON_ArrayForEach(o, objs) {
                    if (sm->objective_count >= QUEST_META_OBJ_MAX) break;
                    QuestObjectiveMeta* om = &sm->objectives[sm->objective_count];
                    const cJSON* type = cJSON_GetObjectItemCaseSensitive(o, "type");
                    const cJSON* item = cJSON_GetObjectItemCaseSensitive(o, "itemId");
                    const cJSON* qty = cJSON_GetObjectItemCaseSensitive(o, "quantity");
                    if (cJSON_IsString(type)) copy_str(om->type, sizeof(om->type), type->valuestring);
                    if (cJSON_IsString(item)) copy_str(om->item_id, QUEST_META_ITEM_MAX, item->valuestring);
                    om->quantity = cJSON_IsNumber(qty) ? qty->valueint : 1;
                    sm->objective_count++;
                }
            }
            e->step_count++;
        }
    }

    const cJSON* smc = cJSON_GetObjectItemCaseSensitive(doc, "sourceMapCode");
    if (cJSON_IsString(smc)) copy_str(e->source_map_code, QUEST_META_CODE_MAX, smc->valuestring);
    const cJSON* scx = cJSON_GetObjectItemCaseSensitive(doc, "sourceCellX");
    e->source_cell_x = cJSON_IsNumber(scx) ? scx->valueint : 0;
    const cJSON* scy = cJSON_GetObjectItemCaseSensitive(doc, "sourceCellY");
    e->source_cell_y = cJSON_IsNumber(scy) ? scy->valueint : 0;

    e->reward_count = 0;
    const cJSON* rewards = cJSON_GetObjectItemCaseSensitive(doc, "rewards");
    if (cJSON_IsArray(rewards)) {
        const cJSON* r = NULL;
        cJSON_ArrayForEach(r, rewards) {
            if (e->reward_count >= QUEST_META_REWARD_MAX) break;
            const cJSON* item_id = cJSON_GetObjectItemCaseSensitive(r, "itemId");
            if (!cJSON_IsString(item_id)) continue;
            const cJSON* qty = cJSON_GetObjectItemCaseSensitive(r, "quantity");
            QuestRewardMeta* rm = &e->rewards[e->reward_count];
            copy_str(rm->item_id, QUEST_META_ITEM_MAX, item_id->valuestring);
            rm->quantity = cJSON_IsNumber(qty) ? qty->valueint : 1;
            e->reward_count++;
        }
    }

    e->prerequisite_count = 0;
    const cJSON* prereqs = cJSON_GetObjectItemCaseSensitive(doc, "prerequisiteCodes");
    if (cJSON_IsArray(prereqs)) {
        const cJSON* p = NULL;
        cJSON_ArrayForEach(p, prereqs) {
            if (e->prerequisite_count >= QUEST_META_PREREQ_MAX) break;
            if (!cJSON_IsString(p) || p->valuestring[0] == '\0') continue;
            copy_str(e->prerequisites[e->prerequisite_count], QUEST_META_CODE_MAX, p->valuestring);
            e->prerequisite_count++;
        }
    }
}

/* Cache the parsed quest doc under `code` and mirror title/description into
 * quest_store so the journal and action tab render without polling. */
static void store_quest_doc(const char* code, const cJSON* doc) {
    QuestMetadataEntry* e = find_or_create(code);
    if (!e) return;
    copy_str(e->code, QUEST_META_CODE_MAX, code);
    ingest_quest_doc(e, doc);
    e->state = QUEST_META_READY;
    quest_store_set_meta(code, e->title, e->description);
}

/* Returns the envelope's `data` object on success, else NULL. Caller owns
 * `root` and must cJSON_Delete it regardless of the return value. */
static const cJSON* envelope_success_doc(const cJSON* root) {
    if (!root) return NULL;
    const cJSON* status = cJSON_GetObjectItemCaseSensitive(root, "status");
    const cJSON* doc = cJSON_GetObjectItemCaseSensitive(root, "data");
    if (!cJSON_IsString(status) || 0 != strcmp(status->valuestring, "success") ||
        !cJSON_IsObject(doc)) {
        return NULL;
    }
    return doc;
}

static void on_quest_fetched(const FetchResponse* r) {
    QuestMetadataEntry* e = find_by_code(r->asset_id);
    if (!e) { free(r->data); return; }

    if (!r->success) {
        e->state = QUEST_META_ERROR;
        LOG_WARN("quest metadata fetch failed for %s", r->asset_id);
        free(r->data);
        return;
    }

    cJSON* root = cJSON_ParseWithLength((const char*)r->data, r->size);
    const cJSON* doc = envelope_success_doc(root);
    if (!doc) {
        e->state = QUEST_META_ERROR;
    } else {
        store_quest_doc(r->asset_id, doc);
    }
    cJSON_Delete(root);
    free(r->data);
}
