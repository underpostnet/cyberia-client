#include "engine_client.h"

#include <emscripten/fetch.h>

#include <assert.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    char*            asset_id;
    FetchCompletedCb on_completed;
} FetchContext;

static void on_fetch_success(emscripten_fetch_t* f) {
    FetchContext* ctx = f->userData;

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

    emscripten_fetch_attr_t attr;
    emscripten_fetch_attr_init(&attr);
    strcpy(attr.requestMethod, "GET");
    attr.attributes = EMSCRIPTEN_FETCH_LOAD_TO_MEMORY;
    attr.onsuccess  = on_fetch_success;
    attr.onerror    = on_fetch_error;
    attr.userData   = ctx;
    emscripten_fetch(&attr, url);
}
