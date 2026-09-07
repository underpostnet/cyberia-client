#ifndef CYBERIA_AUDIO_H
#define CYBERIA_AUDIO_H

#include <stdbool.h>
#include <stddef.h>

typedef enum { AUDIO_MASTER, AUDIO_MUSIC, AUDIO_SFX, AUDIO_BUS_COUNT } AudioBus;
typedef enum { AUDIO_ABSENT, AUDIO_LOADING, AUDIO_READY, AUDIO_FAILED } AudioAssetState;

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
void audio_preload(const char* code);
AudioAssetState audio_asset_state(const char* code);
bool audio_unload(const char* code);
bool audio_load_wav(const char* code, const void* bytes, size_t size);
bool audio_play(const char* code, const AudioParams* params);
void audio_stop_music(float fade_seconds);
void audio_set_map(const char* map_code);
void audio_event(const char* logic_event_id);

#endif
