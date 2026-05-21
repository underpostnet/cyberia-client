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

typedef void (*FetchCompletedCb)(uint32_t request_id, FetchState state, void* data, size_t size);

// callback on success returns malloc, must free
uint32_t fetch_request_start(const char* url, FetchCompletedCb on_completed);
void fetch_request_update(void);

#endif
