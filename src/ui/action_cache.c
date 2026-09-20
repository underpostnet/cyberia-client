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
        const char* item = json_str(row, "itemId");
        if (NULL == item) continue;
        copy_str(out[count].item_id, ACTION_CACHE_CODE_MAX, item);
        out[count].qty = json_int(row, "qty", 1);
        count++;
    }
    return count;
}

static void ingest_doc(void* entry, const cJSON* doc) {
    ActionMetadataEntry* e = entry;

    copy_str(e->label, ACTION_CACHE_LABEL_MAX, json_str(doc, "label"));

    /* shopItems[] is the vendor catalog. A non-empty list is what makes the
     * entity a vendor — there is no action type flag. */
    e->shop_count = 0;
    const cJSON* sis = cJSON_GetObjectItemCaseSensitive(doc, "shopItems");
    if (cJSON_IsArray(sis)) {
        const cJSON* si = NULL;
        cJSON_ArrayForEach(si, sis) {
            if (e->shop_count >= ACTION_CACHE_SHOP_MAX) break;
            const char* item = json_str(si, "itemId");
            if (NULL == item) continue;
            const char* price_item = json_str(si, "priceItemId");
            ActionShopItem* slot = &e->shop_items[e->shop_count];
            copy_str(slot->item_id, ACTION_CACHE_CODE_MAX, item);
            copy_str(slot->price_item_id, ACTION_CACHE_CODE_MAX,
                     price_item ? price_item : "coin");
            slot->price_qty = json_int(si, "priceQty", 1);
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
            slot->craft_time_ms = json_int(r, "craftTimeMs", 0);
            /* A recipe with nothing to produce is not offerable. */
            if (slot->output_count > 0) e->craft_count++;
        }
    }

    /* storageSlots is the vault capacity; a positive value is what makes the
     * entity a storage terminal. */
    e->storage_slots = json_int(doc, "storageSlots", 0);
}
