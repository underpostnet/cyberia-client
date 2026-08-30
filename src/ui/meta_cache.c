#include "meta_cache.h"

#include "util/log.h"
#include "util/utils.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static MetaCacheHead* head_at(MetaCache* c, int i) {
    return (MetaCacheHead*)((char*)c->entries + (size_t)i * c->elem_size);
}

void* meta_cache_find(MetaCache* c, const char* code) {
    assert(c);
    if (!code) return NULL;
    for (int i = 0; i < c->count; ++i) {
        MetaCacheHead* h = head_at(c, i);
        if (0 == strcmp(h->code, code)) return h;
    }
    return NULL;
}

static MetaCacheHead* find_or_create(MetaCache* c, const char* code) {
    MetaCacheHead* h = meta_cache_find(c, code);
    if (h) return h;
    if (c->count >= c->cap) return NULL;
    h = head_at(c, c->count++);
    memset(h, 0, c->elem_size);
    copy_str(h->code, META_CACHE_CODE_MAX, code);
    return h;
}

void meta_cache_fetch(MetaCache* c, const char* code, FetchCompletedCb on_completed) {
    assert(c && on_completed);
    if (!code || '\0' == code[0]) return;

    MetaCacheHead* h = find_or_create(c, code);
    if (!h) return;
    if (META_CACHE_READY == h->state || META_CACHE_LOADING == h->state) return;

    h->state = META_CACHE_LOADING;

    char url[512];
    snprintf(url, sizeof url, "%s%s", c->url_prefix, code);
    fetch_request_start(code, url, on_completed);
}

void meta_cache_on_fetched(MetaCache* c, const FetchResponse* r) {
    assert(c && r);
    MetaCacheHead* h = meta_cache_find(c, r->asset_id);
    if (!h) { free(r->data); return; }

    if (!r->success) {
        h->state = META_CACHE_ERROR;
        LOG_WARN("%s metadata fetch failed for %s", c->label, r->asset_id);
        free(r->data);
        return;
    }

    cJSON* root = cJSON_ParseWithLength((const char*)r->data, r->size);
    const cJSON* doc = envelope_success_doc(root);
    if (!doc) {
        h->state = META_CACHE_ERROR;
    } else {
        c->ingest(h, doc);
        h->state = META_CACHE_READY;
    }
    cJSON_Delete(root);
    free(r->data);
}
