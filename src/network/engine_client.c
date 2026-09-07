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
    size_t           max_bytes;
} FetchContext;

/* One queued request: everything dispatch needs, owned until it is dispatched. */
typedef struct {
    char*            asset_id;
    char*            url;
    FetchCompletedCb on_completed;
    size_t           max_bytes;
    unsigned         timeout_ms;
} Queued;

static int  s_pending_count = 0;
static int  s_total_started = 0;
static char s_last_completed[96] = {0};

/*
 * A browser opens a limited number of connections per origin, so requests beyond that limit sit
 * in the browser's own queue where this client can neither see nor order them. Holding the
 * surplus here instead keeps the number in flight deliberate, and keeps the queue observable.
 */
static Queued s_queue[FETCH_QUEUE_CAP];
static int    s_queue_head = 0;
static int    s_queue_count = 0;
static int    s_in_flight = 0;
static int    s_max_concurrent = FETCH_DEFAULT_MAX_CONCURRENT;

static void dispatch(const char* asset_id, const char* url, FetchCompletedCb on_completed,
                     size_t max_bytes, unsigned timeout_ms);

int fetch_pending_count(void) { return s_pending_count; }
int fetch_total_started(void) { return s_total_started; }
int fetch_in_flight_count(void) { return s_in_flight; }
int fetch_queued_count(void) { return s_queue_count; }
int fetch_max_concurrent(void) { return s_max_concurrent; }
const char* fetch_last_completed_id(void) { return s_last_completed; }

void fetch_set_max_concurrent(int max_concurrent) {
    s_max_concurrent = 1 > max_concurrent ? 1 : max_concurrent;
    /* A raised limit takes effect immediately; a lowered one only bounds what starts next. */
    while (s_queue_count > 0 && s_in_flight < s_max_concurrent) {
        Queued next = s_queue[s_queue_head];
        s_queue_head = (s_queue_head + 1) % FETCH_QUEUE_CAP;
        s_queue_count--;
        dispatch(next.asset_id, next.url, next.on_completed, next.max_bytes, next.timeout_ms);
        free(next.asset_id);
        free(next.url);
    }
}

/* Called once per completed request, whatever its outcome, before the next one starts. */
static void note_completed(const char* asset_id) {
    s_pending_count--;
    s_in_flight--;
    if (asset_id) {
        strncpy(s_last_completed, asset_id, sizeof(s_last_completed) - 1);
        s_last_completed[sizeof(s_last_completed) - 1] = '\0';
    }
    if (0 == s_queue_count || s_in_flight >= s_max_concurrent) return;
    Queued next = s_queue[s_queue_head];
    s_queue_head = (s_queue_head + 1) % FETCH_QUEUE_CAP;
    s_queue_count--;
    dispatch(next.asset_id, next.url, next.on_completed, next.max_bytes, next.timeout_ms);
    free(next.asset_id);
    free(next.url);
}

static void on_fetch_success(emscripten_fetch_t* f) {
    FetchContext* ctx = f->userData;
    note_completed(ctx->asset_id);

    void*  buf = NULL;
    size_t sz  = 0;
    bool   ok  = 0 < f->numBytes && (0 == ctx->max_bytes || ctx->max_bytes >= f->numBytes);

    if (ok) {
        sz = (size_t)f->numBytes;
        buf = malloc(sz);
        assert(buf);
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
    fetch_request_start_limited(asset_id, url, on_completed, 0, 0);
}

void fetch_request_start_limited(const char* asset_id, const char* url, FetchCompletedCb on_completed,
                                 size_t max_bytes, unsigned timeout_ms) {
    assert(asset_id);
    assert(url);
    assert(on_completed);

    /* Counted on arrival, not on dispatch: a queued request is still one the caller is waiting
     * for, and the loading screen reads this as its readiness signal. */
    s_pending_count++;
    s_total_started++;

    if (s_in_flight < s_max_concurrent || FETCH_QUEUE_CAP == s_queue_count) {
        /* A full queue dispatches rather than drops: losing an asset is worse than exceeding
         * the concurrency target on a burst this large. */
        dispatch(asset_id, url, on_completed, max_bytes, timeout_ms);
        return;
    }

    Queued* slot = &s_queue[(s_queue_head + s_queue_count) % FETCH_QUEUE_CAP];
    slot->asset_id = strdup(asset_id);
    slot->url = strdup(url);
    assert(slot->asset_id && slot->url);
    slot->on_completed = on_completed;
    slot->max_bytes = max_bytes;
    slot->timeout_ms = timeout_ms;
    s_queue_count++;
}

static void dispatch(const char* asset_id, const char* url, FetchCompletedCb on_completed,
                     size_t max_bytes, unsigned timeout_ms) {
    s_in_flight++;

    FetchContext* ctx = malloc(sizeof(FetchContext));
    assert(ctx);
    ctx->asset_id     = strdup(asset_id);
    assert(ctx->asset_id);
    ctx->on_completed = on_completed;
    ctx->max_bytes    = max_bytes;

    emscripten_fetch_attr_t attr;
    emscripten_fetch_attr_init(&attr);
    strcpy(attr.requestMethod, "GET");
    attr.attributes = EMSCRIPTEN_FETCH_LOAD_TO_MEMORY;
    attr.onsuccess  = on_fetch_success;
    attr.onerror    = on_fetch_error;
    attr.userData   = ctx;
    attr.timeoutMSecs = timeout_ms;


    char target_url[4096];
    int length = snprintf(target_url, sizeof(target_url), "%s%s", config_data_server_url(), url);
    if (0 <= length && sizeof(target_url) > (size_t)length && emscripten_fetch(&attr, target_url)) return;
    note_completed(ctx->asset_id);
    FetchResponse response = { .asset_id = ctx->asset_id, .success = false };
    on_completed(&response);
    free(ctx->asset_id);
    free(ctx);
}
