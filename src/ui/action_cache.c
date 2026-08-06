/**
 * action_cache — see header. Mirrors quest_cache's REST path.
 */

#include "action_cache.h"

#include "network/engine_client.h"
#include "util/log.h"

#include <cJSON.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static ActionMetadataEntry s_cache[ACTION_CACHE_CAP];
static int             s_count = 0;

static ActionMetadataEntry* find_by_code(const char* code) {
    if (!code) return NULL;
    for (int i = 0; i < s_count; ++i) {
        if (0 == strcmp(s_cache[i].code, code)) return &s_cache[i];
    }
    return NULL;
}

static ActionMetadataEntry* find_or_create(const char* code) {
    ActionMetadataEntry* e = find_by_code(code);
    if (e) return e;
    if (s_count >= ACTION_CACHE_CAP) return NULL;
    e = &s_cache[s_count++];
    memset(e, 0, sizeof(ActionMetadataEntry));
    strncpy(e->code, code, ACTION_CACHE_CODE_MAX - 1);
    return e;
}

void action_cache_reset(void) {
    s_count = 0;
}

static void copy_str(char* dst, size_t cap, const char* src) {
    if (!src) { dst[0] = '\0'; return; }
    strncpy(dst, src, cap - 1);
    dst[cap - 1] = '\0';
}

const ActionMetadataEntry* action_cache_get(const char* code) {
    if (!code) return NULL;
    return find_by_code(code);
}

/* Read one side of a recipe ({itemId, qty} rows) into `out`; returns the count. */
static int ingest_craft_items(const cJSON* arr, ActionCraftItem* out) {
    if (!cJSON_IsArray(arr)) return 0;
    int count = 0;
    const cJSON* row = NULL;
    cJSON_ArrayForEach(row, arr) {
        if (count >= ACTION_CACHE_CRAFT_ITEMS_MAX) break;
        const cJSON* item = cJSON_GetObjectItemCaseSensitive(row, "itemId");
        if (!cJSON_IsString(item)) continue;
        const cJSON* qty = cJSON_GetObjectItemCaseSensitive(row, "qty");
        copy_str(out[count].item_id, ACTION_CACHE_CODE_MAX, item->valuestring);
        out[count].qty = cJSON_IsNumber(qty) ? qty->valueint : 1;
        count++;
    }
    return count;
}

static void ingest_doc(ActionMetadataEntry* e, const cJSON* doc) {
    const cJSON* label = cJSON_GetObjectItemCaseSensitive(doc, "label");
    if (cJSON_IsString(label)) copy_str(e->label, ACTION_CACHE_LABEL_MAX, label->valuestring);

    const cJSON* dlg = cJSON_GetObjectItemCaseSensitive(doc, "dialogCode");
    if (cJSON_IsString(dlg)) copy_str(e->dialog_code, ACTION_CACHE_CODE_MAX, dlg->valuestring);

    const cJSON* smc = cJSON_GetObjectItemCaseSensitive(doc, "sourceMapCode");
    if (cJSON_IsString(smc)) copy_str(e->source_map_code, ACTION_CACHE_CODE_MAX, smc->valuestring);
    const cJSON* scx = cJSON_GetObjectItemCaseSensitive(doc, "sourceCellX");
    e->source_cell_x = cJSON_IsNumber(scx) ? scx->valueint : 0;
    const cJSON* scy = cJSON_GetObjectItemCaseSensitive(doc, "sourceCellY");
    e->source_cell_y = cJSON_IsNumber(scy) ? scy->valueint : 0;

    /* quests[] is the action's talk-step dialogue map: which dialogue to show for
     * a quest's `talk` objective handled here. It does NOT define which quests are
     * OFFERED — offers are located by cell via the cyberia-quest API. */
    e->quest_count = 0;
    const cJSON* qds = cJSON_GetObjectItemCaseSensitive(doc, "questDialogueCodes");
    if (cJSON_IsArray(qds)) {
        const cJSON* qd = NULL;
        cJSON_ArrayForEach(qd, qds) {
            if (e->quest_count >= ACTION_CACHE_QUEST_MAX) break;
            const cJSON* qc = cJSON_GetObjectItemCaseSensitive(qd, "questCode");
            const cJSON* dc = cJSON_GetObjectItemCaseSensitive(qd, "dialogCode");
            if (!cJSON_IsString(qc)) continue;
            ActionQuestDlg* slot = &e->quests[e->quest_count];
            copy_str(slot->quest_code, ACTION_CACHE_CODE_MAX, qc->valuestring);
            if (cJSON_IsString(dc)) copy_str(slot->dialog_code, ACTION_CACHE_CODE_MAX, dc->valuestring);
            e->quest_count++;
        }
    }

    /* shopItems[] is the vendor catalog. A non-empty list is what makes the
     * entity a vendor — there is no action type flag. */
    e->shop_count = 0;
    const cJSON* sis = cJSON_GetObjectItemCaseSensitive(doc, "shopItems");
    if (cJSON_IsArray(sis)) {
        const cJSON* si = NULL;
        cJSON_ArrayForEach(si, sis) {
            if (e->shop_count >= ACTION_CACHE_SHOP_MAX) break;
            const cJSON* item = cJSON_GetObjectItemCaseSensitive(si, "itemId");
            if (!cJSON_IsString(item)) continue;
            const cJSON* price_item = cJSON_GetObjectItemCaseSensitive(si, "priceItemId");
            const cJSON* price_qty = cJSON_GetObjectItemCaseSensitive(si, "priceQty");
            ActionShopItem* slot = &e->shop_items[e->shop_count];
            copy_str(slot->item_id, ACTION_CACHE_CODE_MAX, item->valuestring);
            copy_str(slot->price_item_id, ACTION_CACHE_CODE_MAX,
                     cJSON_IsString(price_item) ? price_item->valuestring : "coin");
            slot->price_qty = cJSON_IsNumber(price_qty) ? price_qty->valueint : 1;
            e->shop_count++;
        }
    }

    /* craftRecipes[] is the assembler recipe book. A non-empty list is what
     * makes the entity a fabrication terminal — there is no action type flag. */
    e->craft_count = 0;
    const cJSON* recipes = cJSON_GetObjectItemCaseSensitive(doc, "craftRecipes");
    if (cJSON_IsArray(recipes)) {
        const cJSON* r = NULL;
        cJSON_ArrayForEach(r, recipes) {
            if (e->craft_count >= ACTION_CACHE_CRAFT_MAX) break;
            ActionCraftRecipe* slot = &e->craft_recipes[e->craft_count];
            slot->output_count = ingest_craft_items(cJSON_GetObjectItemCaseSensitive(r, "outputItems"),
                                                    slot->outputs);
            slot->ingredient_count = ingest_craft_items(cJSON_GetObjectItemCaseSensitive(r, "ingredients"),
                                                        slot->ingredients);
            const cJSON* ms = cJSON_GetObjectItemCaseSensitive(r, "craftTimeMs");
            slot->craft_time_ms = cJSON_IsNumber(ms) ? ms->valueint : 0;
            /* A recipe with nothing to produce is not offerable. */
            if (slot->output_count > 0) e->craft_count++;
        }
    }
}

static void on_action_fetched(const FetchResponse* r) {
    ActionMetadataEntry* e = find_by_code(r->asset_id);
    if (!e) { free(r->data); return; }

    if (!r->success) {
        e->state = ACTION_CACHE_ERROR;
        LOG_WARN("action metadata fetch failed for %s", r->asset_id);
        free(r->data);
        return;
    }

    cJSON* root = cJSON_ParseWithLength((const char*)r->data, r->size);
    const cJSON* status = root ? cJSON_GetObjectItemCaseSensitive(root, "status") : NULL;
    const cJSON* doc = root ? cJSON_GetObjectItemCaseSensitive(root, "data") : NULL;
    if (!cJSON_IsString(status) || 0 != strcmp(status->valuestring, "success") || !cJSON_IsObject(doc)) {
        e->state = ACTION_CACHE_ERROR;
    } else {
        ingest_doc(e, doc);
        e->state = ACTION_CACHE_READY;
    }
    cJSON_Delete(root);
    free(r->data);
}

void action_cache_fetch(const char* code) {
    if (!code || '\0' == code[0]) return;

    ActionMetadataEntry* e = find_or_create(code);
    if (!e) return;
    if (ACTION_CACHE_READY == e->state || ACTION_CACHE_LOADING == e->state) return;

    e->state = ACTION_CACHE_LOADING;
    char url[512];
    snprintf(url, sizeof url, "/api/cyberia-action/code/%s", code);
    fetch_request_start(code, url, on_action_fetched);
}
