#include "dialogue_data.h"
#include "config.h"
#include "hash_table.h"
#include "network/engine_client.h"
#include "util/log.h"
#include <cJSON.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

static HashTable ht;

static void parse_response(DialogueDataSet* d, const unsigned char* data, int size) {
    cJSON* root = cJSON_ParseWithLength((const char*)data, size);
    if (!root) {
        d->state = DLG_DATA_ERROR;
        return;
    }

    cJSON* status = cJSON_GetObjectItemCaseSensitive(root, "status");
    if (!cJSON_IsString(status) || strcmp(status->valuestring, "success") != 0) {
        d->state = DLG_DATA_ERROR;
        cJSON_Delete(root);
        return;
    }

    /* Response shape: { status, data: [ {...}, ... ] }
     * data is a direct array of dialogue lines sorted by order.
     */
    cJSON* arr = cJSON_GetObjectItemCaseSensitive(root, "data");

    if (!arr || !cJSON_IsArray(arr)) {
        d->state = DLG_DATA_EMPTY;
        d->line_count = 0;
        cJSON_Delete(root);
        return;
    }

    int count = 0;
    cJSON* item = NULL;
    cJSON_ArrayForEach(item, arr) {
        if (count >= DIALOGUE_MAX_LINES) break;

        DialogueLine* line = &d->lines[count];
        memset(line, 0, sizeof(DialogueLine));

        cJSON* speaker = cJSON_GetObjectItemCaseSensitive(item, "speaker");
        if (cJSON_IsString(speaker) && speaker->valuestring)
            strncpy(line->speaker, speaker->valuestring, DIALOGUE_MAX_SPEAKER - 1);

        cJSON* text = cJSON_GetObjectItemCaseSensitive(item, "text");
        if (cJSON_IsString(text) && text->valuestring)
            strncpy(line->text, text->valuestring, DIALOGUE_MAX_TEXT - 1);

        cJSON* mood = cJSON_GetObjectItemCaseSensitive(item, "mood");
        if (cJSON_IsString(mood) && mood->valuestring)
            strncpy(line->mood, mood->valuestring, sizeof(line->mood) - 1);

        cJSON* order = cJSON_GetObjectItemCaseSensitive(item, "order");
        line->order = cJSON_IsNumber(order) ? order->valueint : count;

        count++;
    }

    d->line_count = count;
    d->state = (count > 0) ? DLG_DATA_READY : DLG_DATA_EMPTY;

    cJSON_Delete(root);
    LOG_INFO("[DIALOGUE_DATA] Fetched %d lines for item '%s'", count, d->item_id);
}

static void on_dialogue_fetched(const FetchResponse* r) {
    DialogueDataSet* d = hash_table_get(&ht, r->asset_id);
    if (NULL == d) { free(r->data); return; }

    if (!r->success) {
        d->state = DLG_DATA_ERROR;
        LOG_ERROR("[DIALOGUE_DATA] Fetch error for '%s'", d->item_id);
        free(r->data);
        return;
    }

    parse_response(d, (const unsigned char*)r->data, (int)r->size);
    free(r->data);
}

/* ── Public API ──────────────────────────────────────────────────────── */

void dialogue_data_init(void) { hash_table_init(&ht, 128, free); }
void dialogue_data_cleanup(void) { hash_table_destroy(&ht); }

void dialogue_data_request(const char* item_id) {
    assert(item_id);
    assert(strlen(item_id) > 0);
    if (hash_table_get(&ht, item_id)) return; /* already in cache or in-flight */

    DialogueDataSet* d = calloc(1, sizeof(DialogueDataSet));

    strncpy(d->item_id, item_id, sizeof(d->item_id) - 1);
    d->state = DLG_DATA_FETCHING;

    hash_table_put(&ht, item_id, d);

    char url[1024];
    snprintf(url, sizeof(url), "%s/api/cyberia-dialogue/code/default-%s", API_BASE_URL, item_id);
    fetch_request_start(item_id, url, on_dialogue_fetched);
    LOG_INFO("[DIALOGUE_DATA] Fetch started for '%s'", item_id);
}

const DialogueDataSet* dialogue_data_get(const char* item_id) {
    assert(item_id);
    return hash_table_get(&ht, item_id);
}

bool dialogue_data_available(const char* item_id) {
    assert(item_id);
    const DialogueDataSet* d = dialogue_data_get(item_id);
    return d && d->state == DLG_DATA_READY && d->line_count > 0;
}
