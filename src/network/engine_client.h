#ifndef CYBERIA_NETWORK_ENGINE_CLIENT_H
#define CYBERIA_NETWORK_ENGINE_CLIENT_H

#include <stdbool.h>
#include <stddef.h>

/*
 * Ownership:
 *   data     — callback OWNS, must free()
 *   asset_id — engine_client owns; valid only for callback duration.
 *              Do not free. Do not stash the pointer past callback return.
 */
typedef struct {
    const char* asset_id;
    bool        success;
    void*       data;
    size_t      size;
} FetchResponse;

typedef void (*FetchCompletedCb)(const FetchResponse* response);

/* Requests dispatched at once. Beyond this, requests queue here in arrival order instead of in
 * the browser's connection pool, where their number and order would be neither visible nor
 * controllable. Override per build with -DFETCH_DEFAULT_MAX_CONCURRENT=<n>, or at runtime with
 * fetch_set_max_concurrent(). */
#ifndef FETCH_DEFAULT_MAX_CONCURRENT
#define FETCH_DEFAULT_MAX_CONCURRENT 8
#endif
#define FETCH_QUEUE_CAP 512

void fetch_set_max_concurrent(int max_concurrent);
int fetch_max_concurrent(void);
int fetch_in_flight_count(void);
int fetch_queued_count(void);

void fetch_request_start(const char* asset_id, const char* url, FetchCompletedCb on_completed);
void fetch_request_start_limited(const char* asset_id, const char* url, FetchCompletedCb on_completed,
                                 size_t max_bytes, unsigned timeout_ms);

/* Requests currently in flight — 0 means the engine fetch pipeline is idle.
 * The loading screen uses this as a REAL readiness signal. */
int fetch_pending_count(void);

/* Requests ever started this session (in flight + completed). */
int fetch_total_started(void);

/* Asset id of the most recently completed request ("" before the first) —
 * streamed by the loading screen as live "what is loading" feedback. */
const char* fetch_last_completed_id(void);

#endif
