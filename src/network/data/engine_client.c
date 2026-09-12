#include "engine_client.h"
#include "js/asset_bridge.h"
#include "config.h"

#include <raylib.h>
#include <rlgl.h>
#include <assert.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define FETCH_CONSUMERS 16
#define FETCH_COPY_BYTES 16384u
#define FETCH_UPLOAD_BYTES (256u * 1024u)
#define FETCH_BODY_LIMIT (8u * 1024u * 1024u)
#define FETCH_CACHE_BYTES (64u * 1024u * 1024u)
#define FETCH_LOADED_BYTES (16u * 1024u * 1024u)
#define FETCH_RETRY_MS 5000.0

typedef struct {
    int references;
    max_align_t alignment;
    unsigned char data[];
} SharedData;

typedef struct {
    char* id;
    FetchCompletedCb callback;
} Consumer;

typedef struct {
    char* url;
    AssetState state;
    FetchPriority priority;
    Consumer consumers[FETCH_CONSUMERS];
    int count;
    int handle;
    int row;
    int pixel_scale;
    size_t limit;
    unsigned timeout;
    size_t size;
    size_t copied;
    SharedData* shared;
    Texture2D gpu;
    unsigned long order;
    unsigned long touched;
    double completed;
    double fallback_start;
    bool visible;
} Request;

static Request s_requests[FETCH_QUEUE_CAP];
static int s_concurrency = FETCH_DEFAULT_MAX_CONCURRENT;
static int s_in_flight;
static unsigned long s_order;
static unsigned long s_frame;
static double s_frame_start;
static double s_frame_ms;
static double s_spent;
static double s_budget;
static unsigned s_flags;
static double s_upload_cost = 0.5 / FETCH_UPLOAD_BYTES;

void fetch_data_release(void* data) {
    if (NULL == data) return;
    SharedData* shared = (SharedData*)((unsigned char*)data - offsetof(SharedData, data));
    assert(0 < shared->references);
    if (0 == --shared->references) free(shared);
}

static size_t resident_bytes(const Request* request) {
    return (NULL != request->shared ? request->size : 0) + (size_t)request->gpu.width * request->gpu.height * 4;
}

static void discard(Request* request) {
    if (ASSET_FETCHING == request->state) s_in_flight--;
    asset_bridge_release(request->handle);
    if (NULL != request->shared) fetch_data_release(request->shared->data);
    if (0 != request->gpu.id) UnloadTexture(request->gpu);
    for (int i = 0; request->count > i; i++) free(request->consumers[i].id);
    free(request->url);
    *request = (Request){0};
}

static Request* find_request(const char* url) {
    for (int i = 0; FETCH_QUEUE_CAP > i; i++) {
        if (NULL != s_requests[i].url && 0 == strcmp(s_requests[i].url, url)) return &s_requests[i];
    }
    return NULL;
}

static void trace_request(const char* event, const Request* request) {
    const char* consumer = 0 < request->count ? request->consumers[0].id :
        FETCH_P0 == request->priority ? "player" : FETCH_P1 == request->priority ? "visible" : "nearby";
    asset_bridge_trace(event, request->url, request->size, resident_bytes(request), request->priority, consumer);
}

static void deliver(Request* request) {
    Consumer consumers[FETCH_CONSUMERS];
    const int count = request->count;
    memcpy(consumers, request->consumers, (size_t)count * sizeof(Consumer));
    request->count = 0;
    for (int i = 0; count > i; i++) {
        const bool success = ASSET_READY == request->state;
        if (success) request->shared->references++;
        FetchResponse response = {.asset_id = consumers[i].id, .success = success,
            .data = success ? request->shared->data : NULL, .size = success ? request->size : 0,
            .priority = request->priority};
        const double started = fetch_now();
        consumers[i].callback(&response);
        fetch_account("asset_callback", request->url, response.size, started);
        free(consumers[i].id);
    }
}

static void complete(Request* request, bool success) {
    request->state = success ? ASSET_READY : ASSET_FAILED;
    request->completed = fetch_now();
    trace_request(success ? "asset_ready" : "asset_failed", request);
    if (0.0 < request->fallback_start) {
        fetch_event("fallback_duration", request->url, 0, request->completed - request->fallback_start);
        request->fallback_start = 0.0;
    }
    asset_bridge_release(request->handle);
    request->handle = 0;
    if (!success) {
        if (NULL != request->shared) fetch_data_release(request->shared->data);
        request->shared = NULL;
        if (0 != request->gpu.id) UnloadTexture(request->gpu);
        request->gpu = (Texture2D){0};
    }
    deliver(request);
}

static Request* enqueue(const char* id, const char* url, int pixel_scale, size_t limit, unsigned timeout, FetchPriority priority) {
    assert(NULL != url && FETCH_P0 <= priority && FETCH_P2 >= priority);
    Request* request = find_request(url);
    if (NULL != request && ASSET_FAILED == request->state && FETCH_RETRY_MS < fetch_now() - request->completed) {
        discard(request);
        request = NULL;
    }
    if (NULL != request) {
        assert(pixel_scale == request->pixel_scale);
        fetch_promote(url, priority);
        request->touched = s_frame;
        return request;
    }
    if (fetch_disabled(STREAM_DYNAMIC_DISABLED) || (0 < pixel_scale && fetch_disabled(STREAM_ATLAS_DISABLED))) return NULL;
    Request* oldest = NULL;
    Request* queued = NULL;
    for (int i = 0; FETCH_QUEUE_CAP > i; i++) {
        Request* slot = &s_requests[i];
        if (NULL == slot->url) { request = slot; break; }
        if (ASSET_QUEUED == slot->state && priority < slot->priority &&
            (NULL == queued || queued->priority < slot->priority)) queued = slot;
        if ((ASSET_READY == slot->state || ASSET_FAILED == slot->state) && s_frame > slot->touched &&
            (NULL == oldest || oldest->touched > slot->touched)) oldest = slot;
    }
    if (NULL == request) {
        if (NULL == oldest && NULL != queued) {
            complete(queued, false);
            oldest = queued;
        }
        if (NULL == oldest) return NULL;
        discard(oldest);
        request = oldest;
    }
    *request = (Request){.url = strdup(url), .state = ASSET_QUEUED, .priority = priority,
        .limit = 0 != limit ? limit : FETCH_BODY_LIMIT, .timeout = timeout, .pixel_scale = pixel_scale,
        .order = ++s_order, .touched = s_frame};
    assert(NULL != request->url);
    asset_bridge_trace("asset_queued", url, 0, 0, priority, id);
    return request;
}

void fetch_request_start_limited(const char* id, const char* url, FetchCompletedCb callback,
                                 size_t limit, unsigned timeout, FetchPriority priority) {
    assert(NULL != id && NULL != callback);
    Request* request = enqueue(id, url, 0, limit, timeout, priority);
    if (NULL != request) {
        for (int i = 0; request->count > i; i++) {
            if (request->consumers[i].callback == callback && 0 == strcmp(request->consumers[i].id, id)) return;
        }
        if (FETCH_CONSUMERS > request->count) {
            Consumer* consumer = &request->consumers[request->count++];
            *consumer = (Consumer){.id = strdup(id), .callback = callback};
            assert(NULL != consumer->id);
            return;
        }
    }
    FetchResponse response = {.asset_id = id, .priority = priority};
    callback(&response);
}

void fetch_request_start(const char* id, const char* url, FetchCompletedCb callback) {
    fetch_request_start_at(id, url, callback, FETCH_P1);
}

void fetch_request_start_at(const char* id, const char* url, FetchCompletedCb callback, FetchPriority priority) {
    fetch_request_start_limited(id, url, callback, FETCH_BODY_LIMIT, 15000, priority);
}

Texture2D fetch_texture(const char* url, int pixel_scale, FetchPriority priority, bool visible) {
    assert(0 < pixel_scale);
    Request* request = enqueue(FETCH_P0 == priority ? "player" : visible ? "visible" : "nearby", url, pixel_scale, FETCH_BODY_LIMIT, 15000, priority);
    if (NULL == request) return (Texture2D){0};
    if (visible) fetch_visible(url);
    if (ASSET_READY == request->state) return request->gpu;
    // First frame a consumer draws a fallback; complete() reports the span.
    if (0.0 == request->fallback_start) request->fallback_start = fetch_now();
    return (Texture2D){0};
}

void fetch_visible(const char* url) {
    Request* request = find_request(url);
    if (NULL != request && !request->visible) {
        request->visible = true;
        trace_request("asset_visible", request);
    }
}

void fetch_promote(const char* url, FetchPriority priority) {
    Request* request = find_request(url);
    if (NULL != request && request->priority > priority) {
        request->priority = priority;
        trace_request("asset_promoted", request);
    }
}

void fetch_deprioritize(void) {
    for (int i = 0; FETCH_QUEUE_CAP > i; i++) s_requests[i].priority = FETCH_P2;
}

AssetState fetch_state(const char* url) {
    const Request* request = find_request(url);
    return NULL != request ? request->state : ASSET_UNREQUESTED;
}

void fetch_process_frame(void) {
    const double pump_start = fetch_now();
    const double spent_before = s_spent;
    const double deadline = pump_start + s_budget - s_spent < s_frame_start + 16.0 ?
        pump_start + s_budget - s_spent : s_frame_start + 16.0;
    size_t loaded = 0;
    for (int i = 0; FETCH_QUEUE_CAP > i; i++) {
        Request* request = &s_requests[i];
        if (ASSET_FETCHING == request->state) {
            int status = asset_bridge_status(request->handle);
            if (0 != status) {
                s_in_flight--;
                request->size = asset_bridge_size(request->handle);
                request->state = 0 > status ? ASSET_FAILED : ASSET_LOADED;
                trace_request("asset_loaded", request);
                if (0 > status) complete(request, false);
            }
        }
        if (ASSET_LOADED == request->state || ASSET_PROCESSING == request->state) loaded += request->size;
    }
    while (s_concurrency > s_in_flight && fetch_now() < deadline) {
        Request* request = NULL;
        for (int i = 0; FETCH_QUEUE_CAP > i; i++) {
            Request* candidate = &s_requests[i];
            if (ASSET_QUEUED != candidate->state || (FETCH_P0 != candidate->priority && FETCH_LOADED_BYTES <= loaded)) continue;
            if (NULL == request || request->priority > candidate->priority ||
                (request->priority == candidate->priority && request->order > candidate->order)) request = candidate;
        }
        if (NULL == request) break;
        char target[4096];
        int length = snprintf(target, sizeof(target), "%s%s", config_data_server_url(), request->url);
        if (0 > length || sizeof(target) <= (size_t)length) { complete(request, false); continue; }
        request->handle = asset_bridge_fetch(target, request->timeout, request->limit);
        request->state = ASSET_FETCHING;
        s_in_flight++;
        trace_request("asset_started", request);
    }
    bool waiting[FETCH_QUEUE_CAP] = {0};
    while (fetch_now() < deadline) {
        Request* request = NULL;
        for (int i = 0; FETCH_QUEUE_CAP > i; i++) {
            Request* candidate = &s_requests[i];
            if (NULL == candidate->url || waiting[i]) continue;
            bool callback = (ASSET_READY == candidate->state || ASSET_FAILED == candidate->state) &&
                0 < candidate->count;
            if (!callback && ASSET_LOADED != candidate->state && ASSET_PROCESSING != candidate->state) continue;
            if (NULL == request || request->priority > candidate->priority ||
                (request->priority == candidate->priority && request->order > candidate->order)) request = candidate;
        }
        if (NULL == request) break;
        request->order = ++s_order;
        if (ASSET_READY == request->state || ASSET_FAILED == request->state) { deliver(request); continue; }
        if (ASSET_LOADED == request->state) {
            int decoding = 0;
            for (int i = 0; FETCH_QUEUE_CAP > i; i++) {
                if (0 < s_requests[i].pixel_scale && ASSET_PROCESSING == s_requests[i].state) decoding++;
            }
            // One background decode leaves one slot for player dependencies.
            if (0 < request->pixel_scale && (2 <= decoding || (1 <= decoding && FETCH_P0 != request->priority))) {
                waiting[request - s_requests] = true;
                continue;
            }
            request->state = ASSET_PROCESSING;
            trace_request("asset_processing", request);
            if (0 < request->pixel_scale) asset_bridge_decode_image(request->handle, request->pixel_scale);
            else {
                request->shared = malloc(sizeof(SharedData) + request->size);
                assert(NULL != request->shared);
                request->shared->references = 1;
            }
            continue;
        }
        if (0 < request->pixel_scale) {
            int status = asset_bridge_status(request->handle);
            if (0 > status) { complete(request, false); continue; }
            if (3 != status) { waiting[request - s_requests] = true; continue; }
            if (0 == request->gpu.id) {
                request->gpu = (Texture2D){.width = asset_bridge_image_width(request->handle),
                    .height = asset_bridge_image_height(request->handle), .mipmaps = 1, .format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8};
                const double started = fetch_now();
                request->gpu.id = rlLoadTexture(NULL, request->gpu.width, request->gpu.height, request->gpu.format, 1);
                fetch_event("texture_storage", request->url, resident_bytes(request), fetch_now() - started);
                if (0 == request->gpu.id) complete(request, false);
                continue;
            }
            const size_t row_bytes = (size_t)request->gpu.width * 4;
            const double available_bytes = (deadline - fetch_now()) / s_upload_cost;
            if ((double)row_bytes > available_bytes) break;
            const size_t upload_bytes = FETCH_UPLOAD_BYTES < available_bytes ? FETCH_UPLOAD_BYTES : (size_t)available_bytes;
            int rows = (int)(upload_bytes / row_bytes);
            if (1 > rows) rows = 1;
            if (request->gpu.height - request->row < rows) rows = request->gpu.height - request->row;
            const double started = fetch_now();
            asset_bridge_upload(request->handle, request->gpu.id, request->row, rows);
            const double elapsed = fetch_now() - started;
            if (0 < elapsed) s_upload_cost = 0.75 * s_upload_cost + 0.25 * elapsed / ((size_t)rows * row_bytes);
            fetch_event("texture_upload", request->url, (size_t)rows * row_bytes, elapsed);
            request->row += rows;
            if (request->gpu.height == request->row) complete(request, true);
        } else {
            size_t chunk = request->size - request->copied;
            if (FETCH_COPY_BYTES < chunk) chunk = FETCH_COPY_BYTES;
            asset_bridge_copy(request->handle, request->shared->data + request->copied, request->copied, chunk);
            request->copied += chunk;
            if (request->size == request->copied) complete(request, true);
        }
    }
    size_t resident = 0;
    for (int i = 0; FETCH_QUEUE_CAP > i; i++) resident += resident_bytes(&s_requests[i]);
    while (FETCH_CACHE_BYTES < resident && fetch_now() < deadline) {
        Request* oldest = NULL;
        for (int i = 0; FETCH_QUEUE_CAP > i; i++) {
            Request* request = &s_requests[i];
            if (ASSET_READY == request->state && s_frame > request->touched &&
                (NULL == oldest || oldest->touched > request->touched)) oldest = request;
        }
        if (NULL == oldest) break;
        resident -= resident_bytes(oldest);
        discard(oldest);
    }
    s_spent = spent_before + fetch_now() - pump_start;
}

void fetch_init(void) { s_flags = asset_bridge_diagnostics(); }
void fetch_shutdown(void) { for (int i = 0; FETCH_QUEUE_CAP > i; i++) discard(&s_requests[i]); }
double fetch_now(void) { return asset_bridge_now(); }
bool fetch_disabled(StreamDiagnostic flag) { return 0 != (s_flags & flag); }
void fetch_frame_begin(double frame_ms, bool critical) {
    s_frame++;
    s_frame_start = fetch_now();
    s_frame_ms = frame_ms;
    s_spent = 0;
    s_budget = critical ? 8.0 : 2.0;
}
bool fetch_has_budget(void) { return s_budget > s_spent && s_frame_start + 16.0 > fetch_now(); }
void fetch_event(const char* event, const char* id, size_t bytes, double duration_ms) {
    asset_bridge_trace(event, NULL != id ? id : "", bytes, duration_ms, -1, "");
}
void fetch_account(const char* operation, const char* id, size_t bytes, double start_ms) {
    const double elapsed = fetch_now() - start_ms;
    s_spent += elapsed;
    fetch_event(operation, id, bytes, elapsed);
}
void fetch_frame_end(void) {
    fetch_event("frame_time", "", 0, s_frame_ms);
    fetch_event("asset_processing_time", "", 0, s_spent);
}
int fetch_in_flight_count(void) { return s_in_flight; }
int fetch_max_concurrent(void) { return s_concurrency; }
void fetch_set_max_concurrent(int count) { s_concurrency = 1 > count ? 1 : 64 < count ? 64 : count; }
int fetch_queued_count(void) {
    int count = 0;
    for (int i = 0; FETCH_QUEUE_CAP > i; i++) if (ASSET_QUEUED == s_requests[i].state) count++;
    return count;
}
int fetch_pending_count(void) {
    int count = 0;
    for (int i = 0; FETCH_QUEUE_CAP > i; i++) {
        const Request* request = &s_requests[i];
        if ((ASSET_QUEUED <= request->state && ASSET_PROCESSING >= request->state) ||
            0 < request->count) count++;
    }
    return count;
}
