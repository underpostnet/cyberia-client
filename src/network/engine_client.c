#include "engine_client.h"

#include "js/services.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

typedef struct FetchRequest {
    uint32_t         id;
    const char*      url;
    FetchState       state;
    FetchCompletedCb on_completed;
} FetchRequest;


#define RQ_QUEUE_CAPACITY 512
static FetchRequest rq_queue[RQ_QUEUE_CAPACITY];
static size_t       rq_count = 0;

static uint32_t next_request_id = 1; // Zero is No Request
uint32_t fetch_request_next_id(void) { return next_request_id++; }

uint32_t fetch_request_start(const char* url, FetchCompletedCb on_completed) {
    assert(url);
    assert(on_completed);

    if (rq_count >= RQ_QUEUE_CAPACITY) return 0;

    FetchRequest rq = (FetchRequest){
        .url          = strdup(url),
        .id           = fetch_request_next_id(),
        .state        = FETCH_STATE_PENDING,
        .on_completed = on_completed,
    };

    js_start_fetch_binary(rq.url, rq.id);
    rq_queue[rq_count++] = rq;
    return rq.id;
}

void fetch_request_update(void) {
    bool needs_compaction = false;
    for (size_t i = 0; i < rq_count; i++) {
        FetchRequest* rq = &rq_queue[i];
        if (rq->id > 0) {
            int sz = 0;
            uint8_t* buf = js_get_fetch_result(rq->id, &sz);

            if (NULL != buf && sz > 0) { rq->state = FETCH_STATE_READY; }
            if (sz < 0) { rq->state = FETCH_STATE_ERROR; sz = 0; }

            if(rq->state == FETCH_STATE_READY || rq->state == FETCH_STATE_ERROR)
            {
                assert(rq->on_completed);
                needs_compaction = true;
                rq->on_completed(rq->id, rq->state, buf, (size_t)sz);
                free((void*)rq->url);
                *rq = (FetchRequest){0};
            }
        }
    }

    if(needs_compaction)
    {
        size_t w = 0;
        for (size_t r = 0; r < rq_count; r++) {
            if (rq_queue[r].id > 0) {
                if (w != r) rq_queue[w] = rq_queue[r];
                w++;
            }
        }
        rq_count = w;
    }
}
