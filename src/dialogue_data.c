/**
 * @file dialogue_data.c
 * @brief Async dialogue data fetcher — same pattern as atlas metadata REST fetch.
 *
 * Uses the shared JS bridge (js_start_fetch_binary / js_get_fetch_result)
 * to issue non-blocking GET requests to the Engine's cyberia-dialogue API.
 */

#include "dialogue_data.h"
#include "config.h"
#include "cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "js/services.h"

/* ── Internal types ──────────────────────────────────────────────────── */

#define DLG_HASH_SIZE 128

typedef struct DlgCacheEntry {
    DialogueDataSet     data;
    int                 request_id;
    struct DlgCacheEntry* next;
} DlgCacheEntry;

/* ── Module state ────────────────────────────────────────────────────── */

static DlgCacheEntry* s_buckets[DLG_HASH_SIZE];
static int            s_next_req_id = 20000; /* offset to avoid collisions with atlas IDs */

/* ── Hash helper ─────────────────────────────────────────────────────── */

static unsigned long hash_str(const char* s) {
    unsigned long h = 5381;
    while (*s) h = ((h << 5) + h) + (unsigned char)*s++;
    return h;
}

/* ── Lookup ──────────────────────────────────────────────────────────── */

static DlgCacheEntry* lookup(const char* item_id) {
    unsigned long idx = hash_str(item_id) % DLG_HASH_SIZE;
    DlgCacheEntry* e = s_buckets[idx];
    while (e) {
        if (strcmp(e->data.item_id, item_id) == 0) return e;
        e = e->next;
    }
    return NULL;
}

/* ── URL builder ─────────────────────────────────────────────────────── */

static void build_url(char* buf, size_t buf_size, const char* item_id) {
    /* Lookup route: GET /api/cyberia-dialogue/code/default-<item-id>
     * Returns { status, data: [ { code, order, speaker, text, mood }, ... ] }
     * sorted by order.  The "default-" prefix is the seeded dialogue default itemId group
     * convention;
     */
      snprintf(buf, buf_size, "%s/api/cyberia-dialogue/code/default-%s", API_BASE_URL, item_id);
}

/* ── JSON parser ─────────────────────────────────────────────────────── */

static void parse_response(DlgCacheEntry* entry, const unsigned char* data, int size) {
    cJSON* root = cJSON_ParseWithLength((const char*)data, size);
    if (!root) {
        entry->data.state = DLG_DATA_ERROR;
        return;
    }

    cJSON* status = cJSON_GetObjectItemCaseSensitive(root, "status");
    if (!cJSON_IsString(status) || strcmp(status->valuestring, "success") != 0) {
        entry->data.state = DLG_DATA_ERROR;
        cJSON_Delete(root);
        return;
    }

    /* Response shape: { status, data: [ {...}, ... ] }
     * data is a direct array of dialogue lines sorted by order. 
     */
    cJSON* arr = cJSON_GetObjectItemCaseSensitive(root, "data");

    if (!arr || !cJSON_IsArray(arr)) {
        entry->data.state = DLG_DATA_EMPTY;
        entry->data.line_count = 0;
        cJSON_Delete(root);
        return;
    }

    int count = 0;
    cJSON* item = NULL;
    cJSON_ArrayForEach(item, arr) {
        if (count >= DIALOGUE_MAX_LINES) break;

        DialogueLine* line = &entry->data.lines[count];
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

    entry->data.line_count = count;
    entry->data.state = (count > 0) ? DLG_DATA_READY : DLG_DATA_EMPTY;

    cJSON_Delete(root);
    printf("[DIALOGUE_DATA] Fetched %d lines for item '%s'\n", count, entry->data.item_id);
}

/* ── Public API ──────────────────────────────────────────────────────── */

void dialogue_data_init(void) {
    memset(s_buckets, 0, sizeof(s_buckets));
    s_next_req_id = 20000;
}

void dialogue_data_cleanup(void) {
    for (int i = 0; i < DLG_HASH_SIZE; i++) {
        DlgCacheEntry* e = s_buckets[i];
        while (e) {
            DlgCacheEntry* next = e->next;
            free(e);
            e = next;
        }
        s_buckets[i] = NULL;
    }
}

void dialogue_data_request(const char* item_id) {
    if (!item_id || item_id[0] == '\0') return;
    if (lookup(item_id)) return; /* already in cache or in-flight */

    DlgCacheEntry* entry = (DlgCacheEntry*)calloc(1, sizeof(DlgCacheEntry));
    if (!entry) return;

    strncpy(entry->data.item_id, item_id, sizeof(entry->data.item_id) - 1);
    entry->data.state = DLG_DATA_FETCHING;
    entry->request_id = s_next_req_id++;

    unsigned long idx = hash_str(item_id) % DLG_HASH_SIZE;
    entry->next = s_buckets[idx];
    s_buckets[idx] = entry;

    char url[1024];
    build_url(url, sizeof(url), item_id);
    js_start_fetch_binary(url, entry->request_id);
    printf("[DIALOGUE_DATA] Fetch started for '%s' (req %d)\n", item_id, entry->request_id);
}

void dialogue_data_poll(void) {
    for (int i = 0; i < DLG_HASH_SIZE; i++) {
        DlgCacheEntry* e = s_buckets[i];
        while (e) {
            if (e->data.state == DLG_DATA_FETCHING) {
                int size = 0;
                unsigned char* buf = js_get_fetch_result(e->request_id, &size);
                if (buf && size > 0) {
                    parse_response(e, buf, size);
                    free(buf);
                } else if (size < 0) {
                    /* fetch error */
                    e->data.state = DLG_DATA_ERROR;
                    fprintf(stderr, "[DIALOGUE_DATA] Fetch error for '%s'\n", e->data.item_id);
                }
                /* size == 0 → still pending, keep polling */
            }
            e = e->next;
        }
    }
}

const DialogueDataSet* dialogue_data_get(const char* item_id) {
    assert(item_id);
    DlgCacheEntry* e = lookup(item_id);
    return e ? &e->data : NULL;
}

bool dialogue_data_available(const char* item_id) {
    const DialogueDataSet* d = dialogue_data_get(item_id);
    return d && d->state == DLG_DATA_READY && d->line_count > 0;
}
