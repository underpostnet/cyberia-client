#include "engine_client.h"

#include <emscripten/fetch.h>

#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "config.h"

typedef struct {
    char*            asset_id;
    FetchCompletedCb on_completed;
} FetchContext;

static int  s_pending_count = 0;
static int  s_total_started = 0;
static char s_last_completed[96] = {0};

int fetch_pending_count(void) { return s_pending_count; }
int fetch_total_started(void) { return s_total_started; }
const char* fetch_last_completed_id(void) { return s_last_completed; }

static void note_completed(const char* asset_id) {
    s_pending_count--;
    if (asset_id) {
        strncpy(s_last_completed, asset_id, sizeof(s_last_completed) - 1);
        s_last_completed[sizeof(s_last_completed) - 1] = '\0';
    }
}

static void on_fetch_success(emscripten_fetch_t* f) {
    FetchContext* ctx = f->userData;
    note_completed(ctx->asset_id);

    void*  buf = NULL;
    size_t sz  = 0;
    bool   ok  = f->numBytes > 0;

    if (ok) {
        sz = (size_t)f->numBytes;
        buf = malloc(sz);
        memcpy(buf, f->data, sz);
    }

    FetchResponse response = (FetchResponse){
        .success  = ok,
        .data     = ok ? buf : NULL,
        .size     = ok ? sz  : 0,
        .asset_id = ctx->asset_id,
    };
    ctx->on_completed(&response);

    free(ctx->asset_id);
    free(ctx);
    emscripten_fetch_close(f);
}

static void on_fetch_error(emscripten_fetch_t* f) {
    FetchContext* ctx = f->userData;
    note_completed(ctx->asset_id);

    FetchResponse response = (FetchResponse){
        .success  = false,
        .data     = NULL,
        .size     = 0,
        .asset_id = ctx->asset_id,
    };
    ctx->on_completed(&response);

    free(ctx->asset_id);
    free(ctx);
    emscripten_fetch_close(f);
}

void fetch_request_start(const char* asset_id, const char* url, FetchCompletedCb on_completed) {
    assert(asset_id);
    assert(url);
    assert(on_completed);

    FetchContext* ctx = malloc(sizeof(FetchContext));
    ctx->asset_id     = strdup(asset_id);
    ctx->on_completed = on_completed;
    s_pending_count++;
    s_total_started++;

    emscripten_fetch_attr_t attr;
    emscripten_fetch_attr_init(&attr);
    strcpy(attr.requestMethod, "GET");
    attr.attributes = EMSCRIPTEN_FETCH_LOAD_TO_MEMORY;
    attr.onsuccess  = on_fetch_success;
    attr.onerror    = on_fetch_error;
    attr.userData   = ctx;


    static char target_url[1024];
    snprintf(target_url, sizeof(target_url), "%s%s", API_BASE_URL, url);
    emscripten_fetch(&attr, target_url);
}
