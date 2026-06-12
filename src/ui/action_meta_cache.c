/**
 * action_meta_cache — see header. Mirrors quest_metadata_cache's REST path.
 */

#include "action_meta_cache.h"

#include "network/engine_client.h"
#include "util/log.h"

#include <cJSON.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static ActionMetaEntry s_cache[ACTION_META_CACHE_CAP];
static int             s_count = 0;

static ActionMetaEntry* find_by_code(const char* code) {
    if (!code) return NULL;
    for (int i = 0; i < s_count; ++i) {
        if (0 == strcmp(s_cache[i].code, code)) return &s_cache[i];
    }
    return NULL;
}

static ActionMetaEntry* find_or_create(const char* code) {
    ActionMetaEntry* e = find_by_code(code);
    if (e) return e;
    if (s_count >= ACTION_META_CACHE_CAP) return NULL;
    e = &s_cache[s_count++];
    memset(e, 0, sizeof(ActionMetaEntry));
    strncpy(e->code, code, ACTION_META_CODE_MAX - 1);
    return e;
}

void action_meta_cache_reset(void) {
    s_count = 0;
}

static void copy_str(char* dst, size_t cap, const char* src) {
    if (!src) { dst[0] = '\0'; return; }
    strncpy(dst, src, cap - 1);
    dst[cap - 1] = '\0';
}

const ActionMetaEntry* action_meta_cache_get(const char* code) {
    if (!code) return NULL;
    return find_by_code(code);
}

static void ingest_doc(ActionMetaEntry* e, const cJSON* doc) {
    const cJSON* label = cJSON_GetObjectItemCaseSensitive(doc, "label");
    if (cJSON_IsString(label)) copy_str(e->label, ACTION_META_LABEL_MAX, label->valuestring);

    const cJSON* dlg = cJSON_GetObjectItemCaseSensitive(doc, "dialogCode");
    if (cJSON_IsString(dlg)) copy_str(e->dialog_code, ACTION_META_CODE_MAX, dlg->valuestring);

    const cJSON* smc = cJSON_GetObjectItemCaseSensitive(doc, "sourceMapCode");
    if (cJSON_IsString(smc)) copy_str(e->source_map_code, ACTION_META_CODE_MAX, smc->valuestring);
    const cJSON* scx = cJSON_GetObjectItemCaseSensitive(doc, "sourceCellX");
    e->source_cell_x = cJSON_IsNumber(scx) ? scx->valueint : 0;
    const cJSON* scy = cJSON_GetObjectItemCaseSensitive(doc, "sourceCellY");
    e->source_cell_y = cJSON_IsNumber(scy) ? scy->valueint : 0;

    e->quest_count = 0;
    const cJSON* qds = cJSON_GetObjectItemCaseSensitive(doc, "questDialogueCodes");
    if (cJSON_IsArray(qds)) {
        const cJSON* qd = NULL;
        cJSON_ArrayForEach(qd, qds) {
            if (e->quest_count >= ACTION_META_QUEST_MAX) break;
            const cJSON* qc = cJSON_GetObjectItemCaseSensitive(qd, "questCode");
            const cJSON* dc = cJSON_GetObjectItemCaseSensitive(qd, "dialogCode");
            if (!cJSON_IsString(qc)) continue;
            ActionQuestDlg* slot = &e->quests[e->quest_count];
            copy_str(slot->quest_code, ACTION_META_CODE_MAX, qc->valuestring);
            if (cJSON_IsString(dc)) copy_str(slot->dialog_code, ACTION_META_CODE_MAX, dc->valuestring);
            e->quest_count++;
        }
    }
}

static void on_action_fetched(const FetchResponse* r) {
    ActionMetaEntry* e = find_by_code(r->asset_id);
    if (!e) { free(r->data); return; }

    if (!r->success) {
        e->state = ACTION_META_ERROR;
        LOG_WARN("action metadata fetch failed for %s", r->asset_id);
        free(r->data);
        return;
    }

    cJSON* root = cJSON_ParseWithLength((const char*)r->data, r->size);
    const cJSON* status = root ? cJSON_GetObjectItemCaseSensitive(root, "status") : NULL;
    const cJSON* doc = root ? cJSON_GetObjectItemCaseSensitive(root, "data") : NULL;
    if (!cJSON_IsString(status) || 0 != strcmp(status->valuestring, "success") || !cJSON_IsObject(doc)) {
        e->state = ACTION_META_ERROR;
    } else {
        ingest_doc(e, doc);
        e->state = ACTION_META_READY;
    }
    cJSON_Delete(root);
    free(r->data);
}

void action_meta_cache_fetch(const char* code) {
    if (!code || '\0' == code[0]) return;

    ActionMetaEntry* e = find_or_create(code);
    if (!e) return;
    if (ACTION_META_READY == e->state || ACTION_META_LOADING == e->state) return;

    e->state = ACTION_META_LOADING;
    char url[512];
    snprintf(url, sizeof url, "/api/cyberia-action/code/%s", code);
    fetch_request_start(code, url, on_action_fetched);
}
