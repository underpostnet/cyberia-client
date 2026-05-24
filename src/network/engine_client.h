#ifndef CYBERIA_NETWORK_ENGINE_CLIENT_H
#define CYBERIA_NETWORK_ENGINE_CLIENT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef enum {
    FETCH_STATE_NONE = 0,
    FETCH_STATE_PENDING,
    FETCH_STATE_READY,
    FETCH_STATE_ERROR
} FetchState;

/*
 * Ownership:
 *   data     — callback OWNS, must free()
 *   asset_id — engine_client owns; valid only for callback duration.
 *              Do not free. Do not stash the pointer past callback return.
 */
typedef struct {
    uint32_t    request_id;
    const char* asset_id;
    FetchState  state;
    void*       data;
    size_t      size;
} FetchResponse;

typedef void (*FetchCompletedCb)(const FetchResponse* response);

uint32_t fetch_request_start(const char* asset_id, const char* url, FetchCompletedCb on_completed);
void     fetch_request_update(void);

#endif
