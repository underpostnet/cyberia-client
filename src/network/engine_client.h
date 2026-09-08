#ifndef CYBERIA_NETWORK_ENGINE_CLIENT_H
#define CYBERIA_NETWORK_ENGINE_CLIENT_H

#include <stdbool.h>
#include <stddef.h>
#include <raylib.h>

typedef enum { FETCH_P0, FETCH_P1, FETCH_P2 } FetchPriority;
typedef enum {
    ASSET_UNREQUESTED, ASSET_QUEUED, ASSET_FETCHING, ASSET_LOADED,
    ASSET_PROCESSING, ASSET_READY, ASSET_FAILED
} AssetState;
typedef enum {
    STREAM_AUDIO_DISABLED = 1, STREAM_AUDIO_NETWORK_DISABLED = 2,
    STREAM_AUDIO_RUNTIME_DISABLED = 4, STREAM_ATLAS_DISABLED = 8,
    STREAM_DYNAMIC_DISABLED = 16
} StreamDiagnostic;

// Consumers release shared bytes with fetch_data_release().
typedef struct {
    const char* asset_id;
    bool success;
    void* data;
    size_t size;
    FetchPriority priority;
} FetchResponse;
typedef void (*FetchCompletedCb)(const FetchResponse* response);

#define FETCH_DEFAULT_MAX_CONCURRENT 8
#define FETCH_QUEUE_CAP 512

void fetch_init(void);
void fetch_shutdown(void);
void fetch_frame_begin(double frame_ms, bool critical);
void fetch_frame_end(void);
bool fetch_has_budget(void);
double fetch_now(void);
bool fetch_disabled(StreamDiagnostic flag);
void fetch_event(const char* event, const char* id, size_t bytes, double duration_ms);
void fetch_account(const char* operation, const char* id, size_t bytes, double start_ms);
void fetch_process_frame(void);
void fetch_data_release(void* data);
void fetch_visible(const char* url);
void fetch_promote(const char* url, FetchPriority priority);
void fetch_deprioritize(void);
AssetState fetch_state(const char* url);
void fetch_set_max_concurrent(int max_concurrent);
int fetch_max_concurrent(void);
int fetch_in_flight_count(void);
int fetch_queued_count(void);
int fetch_pending_count(void);
void fetch_request_start(const char* id, const char* url, FetchCompletedCb callback);
void fetch_request_start_at(const char* id, const char* url, FetchCompletedCb callback, FetchPriority priority);
void fetch_request_start_limited(const char* id, const char* url, FetchCompletedCb callback,
                                 size_t max_bytes, unsigned timeout_ms, FetchPriority priority);
// The request table owns textures; callers use them only in the current frame.
Texture2D fetch_texture(const char* url, int pixel_scale, FetchPriority priority, bool visible);

#endif
