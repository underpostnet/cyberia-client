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

void fetch_request_start(const char* asset_id, const char* url, FetchCompletedCb on_completed);

/* Requests currently in flight — 0 means the engine fetch pipeline is idle.
 * The loading screen uses this as a REAL readiness signal. */
int fetch_pending_count(void);

/* Requests ever started this session (in flight + completed). */
int fetch_total_started(void);

/* Asset id of the most recently completed request ("" before the first) —
 * streamed by the loading screen as live "what is loading" feedback. */
const char* fetch_last_completed_id(void);

#endif
