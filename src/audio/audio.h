#ifndef CYBERIA_AUDIO_H
#define CYBERIA_AUDIO_H

#include <stdbool.h>

#include "network/data/engine_client.h"
#include <stddef.h>

typedef enum { AUDIO_MASTER, AUDIO_MUSIC, AUDIO_SFX, AUDIO_BUS_COUNT } AudioBus;
typedef struct {
    AudioBus bus;
    float volume;
    float pitch;
    float pan;
    float fade_seconds;
    bool loop;
    int priority;
} AudioParams;

AudioParams audio_params(AudioBus bus);
bool audio_init(void);
void audio_start(void);
void audio_update(float dt);
void audio_shutdown(void);
void audio_set_volume(AudioBus bus, float volume);
float audio_get_volume(AudioBus bus);
/* Master-bus mute latch: silences every bus and restores the previous level on unmute. */
void audio_set_muted(bool muted);
bool audio_is_muted(void);
void audio_toggle_mute(void);
/* Bring an asset toward residency at the given priority. Idempotent: an asset already resident,
 * already fetching or already decoding is left alone. */
void audio_preload(const char* code, FetchPriority priority);
bool audio_unload(const char* code);
bool audio_play(const char* code, const AudioParams* params);
void audio_stop_music(float fade_seconds);
void audio_prefetch_map(const char* code);
void audio_set_map(const char* map_code);
void audio_event(const char* logic_event_id);

#endif
