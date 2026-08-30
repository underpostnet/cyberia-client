/**
 * action_cache — see header. Mirrors quest_cache's REST path.
 */

#include "action_cache.h"

#include "meta_cache.h"
#include "util/utils.h"

#include <cJSON.h>

static ActionMetadataEntry s_entries[ACTION_CACHE_CAP];
static void ingest_doc(void* entry, const cJSON* doc);

static MetaCache s_cache = {
    .entries    = s_entries,
    .elem_size  = sizeof(ActionMetadataEntry),
    .cap        = ACTION_CACHE_CAP,
    .url_prefix = "/api/cyberia-action/code/",
    .label      = "action",
    .ingest     = ingest_doc,
};

const ActionMetadataEntry* action_cache_get(const char* code) {
    return meta_cache_find(&s_cache, code);
}

static void on_action_fetched(const FetchResponse* r) {
    meta_cache_on_fetched(&s_cache, r);
}

void action_cache_fetch(const char* code) {
    meta_cache_fetch(&s_cache, code, on_action_fetched);
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

static void ingest_doc(void* entry, const cJSON* doc) {
    ActionMetadataEntry* e = entry;

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

    /* storageSlots is the vault capacity; a positive value is what makes the
     * entity a storage terminal. */
    const cJSON* storage = cJSON_GetObjectItemCaseSensitive(doc, "storageSlots");
    e->storage_slots = cJSON_IsNumber(storage) ? storage->valueint : 0;
}
