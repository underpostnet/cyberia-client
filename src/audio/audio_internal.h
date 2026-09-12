#ifndef CYBERIA_AUDIO_INTERNAL_H
#define CYBERIA_AUDIO_INTERNAL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "network/data/engine_client.h"

#define AUDIO_CODE_CAP 96
#define AUDIO_ASSET_CAP 32
#define AUDIO_SFX_CAP 16
#define AUDIO_WAV_MAX (8u * 1024u * 1024u)

void audio_content_start(void);
void audio_content_update(float dt);
void audio_content_shutdown(void);
void audio_content_fetch(const char* code, uint64_t token, FetchPriority priority);

/* Warmup class the asset behind this token was requested at, so the blob stage inherits it. */
FetchPriority audio_request_priority(uint64_t token);
void audio_content_restore_music(void);
void audio_content_music_finished(const char* code);
const char* audio_request_code(uint64_t token);
/* Takes ownership of `bytes` and frees it, whether or not an asset is still waiting on `token`. */
void audio_request_complete(uint64_t token, void* bytes, size_t size);
/* The engine has no asset under this code: stop asking for it for the rest of the session. */
void audio_request_absent(uint64_t token);

#endif
