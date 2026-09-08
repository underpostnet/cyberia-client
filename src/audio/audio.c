#include "audio.h"
#include "audio_internal.h"
#include "util/utils.h"
#include "network/engine_client.h"

#include <raylib.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define AUDIO_CACHE_BYTES (32u * 1024u * 1024u)
#define AUDIO_PENDING_CAP 16

typedef struct {
    char code[AUDIO_CODE_CAP];
    bool resolving;
    void* data;
    uint64_t token;
    uint64_t touched;
    double retry_at;
    double last_started;
    bool absent;
    size_t bytes;
    int references;
    FetchPriority priority;
} Asset;

typedef struct {
    Asset* asset;
    Music sound;
    AudioParams params;
    uint64_t order;
    float gain;
    float fade_from;
    float fade_to;
    float fade_elapsed;
    float fade_duration;
} Voice;

typedef struct {
    char code[AUDIO_CODE_CAP];
    AudioParams params;
    double expires;
} Pending;

static Asset s_assets[AUDIO_ASSET_CAP];
static Voice s_sfx[AUDIO_SFX_CAP];
static Voice s_music[2];
static Pending s_pending[AUDIO_PENDING_CAP];
static char s_music_code[AUDIO_CODE_CAP];
static AudioParams s_music_params;
static float s_volumes[AUDIO_BUS_COUNT];
static uint64_t s_sequence;
static double s_time;
static size_t s_bytes;
static bool s_ready;
static bool s_muted;
static float s_unmuted_master = 1.0f;
static bool s_started;

static float bounded(float value, float low, float high) {
    return isfinite(value) ? fminf(high, fmaxf(low, value)) : low;
}

AudioParams audio_params(AudioBus bus) {
    return (AudioParams){ .bus = bus, .volume = 1.0f, .pitch = 1.0f, .pan = 0.5f,
                          .fade_seconds = AUDIO_MUSIC == bus ? 0.8f : 0.0f,
                          .loop = AUDIO_MUSIC == bus };
}

static Asset* find_asset(const char* code) {
    if (!code) return NULL;
    for (int i = 0; AUDIO_ASSET_CAP > i; i++) {
        if ('\0' != s_assets[i].code[0] && 0 == strcmp(code, s_assets[i].code)) return &s_assets[i];
    }
    return NULL;
}

static void release_voice(Voice* voice) {
    if (!voice->asset) return;
    StopMusicStream(voice->sound);
    UnloadMusicStream(voice->sound);
    voice->asset->references--;
    *voice = (Voice){0};
}

static void release_asset(Asset* asset) {
    fetch_data_release(asset->data);
    s_bytes -= asset->bytes;
    *asset = (Asset){0};
}

static Asset* oldest_asset(const Asset* except, bool resident_only) {
    Asset* oldest = NULL;
    for (int i = 0; AUDIO_ASSET_CAP > i; i++) {
        Asset* asset = &s_assets[i];
        /* A slot mid-fetch is owed to its callback, and one holding undecoded bytes is owed a
         * frame; evicting either throws away work already paid for over the network. */
        if (except == asset || 0 != asset->references || asset->resolving || (resident_only && 0 == asset->bytes)) continue;
        if (!oldest || oldest->touched > asset->touched) oldest = asset;
    }
    return oldest;
}

static Asset* reserve_asset(const char* code) {
    if (!code || '\0' == code[0] || AUDIO_CODE_CAP <= strlen(code)) return NULL;
    Asset* asset = find_asset(code);
    if (asset) return asset;
    asset = oldest_asset(NULL, false);
    if (!asset) return NULL;
    release_asset(asset);
    copy_str(asset->code, sizeof(asset->code), code);
    asset->touched = ++s_sequence;
    return asset;
}

static uint32_t u32(const unsigned char* p) {
    return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}

static unsigned u16(const unsigned char* p) { return (unsigned)p[0] | (unsigned)p[1] << 8; }

/* Validate chunk bounds before the decoder reads untrusted file bytes. */
static bool valid_wav(const void* bytes, size_t size) {
    if (!bytes || 44 > size || AUDIO_WAV_MAX < size) return false;
    const unsigned char* p = bytes;
    if (0 != memcmp(p, "RIFF", 4) || 0 != memcmp(p + 8, "WAVE", 4) || size - 8 != u32(p + 4)) return false;
    bool format = false;
    unsigned align = 0;
    unsigned rate = 0;
    unsigned chunks = 0;
    for (size_t offset = 12; size - 8 >= offset && 64 > chunks++;) {
        size_t length = u32(p + offset + 4);
        if (size - offset - 8 < length) return false;
        const unsigned char* data = p + offset + 8;
        if (0 == memcmp(p + offset, "fmt ", 4)) {
            if (16 > length || 1 != u16(data) || 16 != u16(data + 14)) return false;
            unsigned channels = u16(data + 2);
            rate = u32(data + 4);
            align = u16(data + 12);
            if (1 > channels || 2 < channels || channels * 2 != align ||
                22050 > rate || 48000 < rate || rate * align != u32(data + 8)) return false;
            format = true;
        } else if (0 == memcmp(p + offset, "data", 4)) {
            return format && 0 < length && 0 == length % align && (size_t)rate * align * 45 >= length;
        }
        offset += 8 + length + (length & 1);
    }
    return false;
}

bool audio_unload(const char* code) {
    Asset* asset = find_asset(code);
    if (!asset) return true;
    if (0 != asset->references) return false;
    release_asset(asset);
    return true;
}

void audio_request_absent(uint64_t token) {
    for (int i = 0; s_ready && AUDIO_ASSET_CAP > i; i++) {
        Asset* asset = &s_assets[i];
        if (token != asset->token || !asset->resolving) continue;
        asset->resolving = false;
        asset->absent = true;
        return;
    }
}

void audio_preload(const char* code, FetchPriority priority) {
    // Loading is deliberately not gated on the tap: an asset fetched during the loading screen
    // plays the instant the world starts, where one fetched on first use would arrive late and
    // silently miss the moment it was meant to mark.
    if (!s_ready || fetch_disabled(STREAM_AUDIO_NETWORK_DISABLED)) return;
    Asset* asset = reserve_asset(code);
    if (NULL != asset && asset->priority > priority) asset->priority = priority;
    if (!asset || asset->absent || NULL != asset->data || asset->resolving || s_time < asset->retry_at) return;
    asset->resolving = true;
    asset->token = ++s_sequence;
    asset->retry_at = s_time + 25.0;
    asset->priority = priority;
    audio_content_fetch(code, asset->token, priority);
}

const char* audio_request_code(uint64_t token) {
    for (int i = 0; s_ready && AUDIO_ASSET_CAP > i; i++) {
        if (token == s_assets[i].token && s_assets[i].resolving) return s_assets[i].code;
    }
    return NULL;
}

FetchPriority audio_request_priority(uint64_t token) {
    for (int i = 0; s_ready && AUDIO_ASSET_CAP > i; i++) {
        if (token == s_assets[i].token && s_assets[i].resolving) return s_assets[i].priority;
    }
    return FETCH_P2;
}

void audio_request_complete(uint64_t token, void* bytes, size_t size) {
    for (int i = 0; s_ready && AUDIO_ASSET_CAP > i; i++) {
        Asset* asset = &s_assets[i];
        if (token != asset->token || !asset->resolving) continue;
        asset->resolving = false;
        const double started = fetch_now();
        fetch_event("audio_decode_start", asset->code, size, 0);
        bool valid = valid_wav(bytes, size);
        fetch_event("audio_decode_end", asset->code, size, fetch_now() - started);
        while (valid && AUDIO_CACHE_BYTES < s_bytes + size) {
            Asset* victim = oldest_asset(asset, true);
            if (NULL == victim) { valid = false; break; }
            release_asset(victim);
        }
        if (!valid) {
            fetch_data_release(bytes);
            asset->resolving = false;
            asset->retry_at = s_time + 30;
            return;
        }
        asset->data = bytes;
        asset->bytes = size;
        s_bytes += size;
        return;
    }
    fetch_data_release(bytes);
}

/* Every asset arrives from engine-cyberia, so a code is playable once its fetch has landed. */
static Asset* playable(const char* code) {
    Asset* asset = find_asset(code);
    return asset && NULL != asset->data ? asset : NULL;
}

static void apply_volume(Voice* voice) {
    SetMusicVolume(voice->sound, voice->gain * voice->params.volume *
                   s_volumes[voice->params.bus] * s_volumes[AUDIO_MASTER]);
}

static void fade(Voice* voice, float target, float seconds) {
    voice->fade_from = voice->gain;
    voice->fade_to = target;
    voice->fade_duration = seconds;
    voice->fade_elapsed = 0;
    if (0.0f >= seconds) voice->gain = target;
}

static bool begin_voice(Voice* voice, Asset* asset, AudioParams params) {
    if (fetch_disabled(STREAM_AUDIO_RUNTIME_DISABLED) || !fetch_has_budget()) return false;
    const double started = fetch_now();
    fetch_event("audio_runtime_create_start", asset->code, asset->bytes, 0);
    Music alias = LoadMusicStreamFromMemory(".wav", asset->data, (int)asset->bytes);
    fetch_account("audio_runtime_create_end", asset->code, asset->bytes, started);
    if (!IsMusicValid(alias)) return false;
    release_voice(voice);
    alias.looping = params.loop;
    *voice = (Voice){ .asset = asset, .sound = alias, .params = params, .order = ++s_sequence,
                      .gain = 0.0f < params.fade_seconds ? 0.0f : 1.0f };
    asset->references++;
    asset->touched = s_sequence;
    fade(voice, 1.0f, params.fade_seconds);
    SetMusicPitch(alias, params.pitch);
    SetMusicPan(alias, params.pan);
    apply_volume(voice);
    PlayMusicStream(alias);
    fetch_event("audio_play", asset->code, asset->bytes, 0);
    return true;
}

/* Two identical cues closer together than this read as one impact rather than a flam. Distinct
 * cues are never affected, and a repeating cue at any musical rate clears it easily. */
#define SFX_RETRIGGER_SECONDS 0.06

static bool play_sfx(Asset* asset, AudioParams params) {
    Voice* choice = NULL;
    for (int i = 0; AUDIO_SFX_CAP > i; i++) {
        Voice* voice = &s_sfx[i];
        if (!voice->asset || !IsMusicStreamPlaying(voice->sound)) { choice = voice; break; }
        if (!choice || choice->params.priority > voice->params.priority ||
            (choice->params.priority == voice->params.priority && choice->order > voice->order)) choice = voice;
    }
    if (choice->asset && IsMusicStreamPlaying(choice->sound) && params.priority < choice->params.priority) return false;
    if (!begin_voice(choice, asset, params)) return false;
    asset->last_started = s_time;
    return true;
}

bool audio_play(const char* code, const AudioParams* input) {
    if (!s_started || !code || '\0' == code[0] || AUDIO_CODE_CAP <= strlen(code) || !input ||
        (AUDIO_MUSIC != input->bus && AUDIO_SFX != input->bus)) return false;
    AudioParams params = *input;
    params.volume = bounded(params.volume, 0, 1);
    params.pitch = bounded(params.pitch, 0.25f, 4);
    params.pan = bounded(params.pan, 0, 1);
    params.fade_seconds = bounded(params.fade_seconds, 0, 10);
    audio_preload(code, FETCH_P0);
    if (AUDIO_MUSIC == params.bus) {
        copy_str(s_music_code, sizeof(s_music_code), code);
        s_music_params = params;
        return true;
    }
    Asset* asset = playable(code);
    // Several entities hit in the same tick raise several damage events; the world should sound
    // like one impact, not like the same sample stacked on itself.
    if (asset && s_time < asset->last_started + SFX_RETRIGGER_SECONDS) return false;
    if (asset) return play_sfx(asset, params);
    for (int i = 0; AUDIO_PENDING_CAP > i; i++) {
        if ('\0' != s_pending[i].code[0]) continue;
        copy_str(s_pending[i].code, sizeof(s_pending[i].code), code);
        s_pending[i].params = params;
        s_pending[i].expires = s_time + 0.2;
        return true;
    }
    return false;
}

void audio_stop_music(float fade_seconds) {
    s_music_code[0] = '\0';
    for (int i = 0; 2 > i; i++) {
        fade(&s_music[i], 0, bounded(fade_seconds, 0, 10));
        if (0.0f == s_music[i].gain) release_voice(&s_music[i]);
    }
}

/* Mute is a latch over the master bus, not a level: it restores what the player had set. */
void audio_set_muted(bool muted) {
    if (muted == s_muted) return;
    if (muted) s_unmuted_master = s_volumes[AUDIO_MASTER];
    s_muted = muted;
    audio_set_volume(AUDIO_MASTER, muted ? 0.0f : s_unmuted_master);
}

bool audio_is_muted(void) { return s_muted; }

void audio_toggle_mute(void) { audio_set_muted(!s_muted); }

void audio_set_volume(AudioBus bus, float volume) {
    if (AUDIO_MASTER > bus || AUDIO_BUS_COUNT <= bus) return;
    s_volumes[bus] = bounded(volume, 0, 1);
    for (int i = 0; AUDIO_SFX_CAP > i; i++) if (s_sfx[i].asset) apply_volume(&s_sfx[i]);
    for (int i = 0; 2 > i; i++) if (s_music[i].asset) apply_volume(&s_music[i]);
}

float audio_get_volume(AudioBus bus) {
    return AUDIO_MASTER <= bus && AUDIO_BUS_COUNT > bus ? s_volumes[bus] : 0;
}

static void update_voice(Voice* voice, float dt) {
    if (!voice->asset) return;
    const double started = fetch_now();
    fetch_event("audio_decode_start", voice->asset->code, 0, 0);
    UpdateMusicStream(voice->sound);
    fetch_account("audio_decode_end", voice->asset->code, 0, started);
    if (0.0f < voice->fade_duration) {
        voice->fade_elapsed += dt;
        float amount = fminf(1, voice->fade_elapsed / voice->fade_duration);
        voice->gain = voice->fade_from + (voice->fade_to - voice->fade_from) * amount;
    }
    if (0.0f == voice->fade_to && 0.0001f >= voice->gain) { release_voice(voice); return; }
    if (!IsMusicStreamPlaying(voice->sound)) {
        if (voice->params.loop) PlayMusicStream(voice->sound);
        else { release_voice(voice); return; }
    }
    apply_volume(voice);
}

void audio_update(float dt) {
    if (!s_ready) return;
    dt = bounded(dt, 0, 1);
    s_time += dt;
    // Content resolution and asset loading advance from the first frame, so the map's audio is
    // resident by the time the player taps to start. Voices below only run once it has started.
    audio_content_update(dt);
    for (int i = 0; AUDIO_ASSET_CAP > i; i++) {
        Asset* asset = &s_assets[i];
        if (asset->resolving && s_time >= asset->retry_at) audio_request_complete(asset->token, NULL, 0);
    }
    if (!s_started) return;
    for (int i = 0; AUDIO_SFX_CAP > i; i++) update_voice(&s_sfx[i], dt);
    bool finished = false;
    for (int i = 0; 2 > i; i++) {
        if (s_music[i].asset && !s_music[i].params.loop && 1.0f == s_music[i].fade_to &&
            !IsMusicStreamPlaying(s_music[i].sound)) finished = true;
        update_voice(&s_music[i], dt);
    }
    if (finished) {
        char completed[AUDIO_CODE_CAP];
        copy_str(completed, sizeof(completed), s_music_code);
        s_music_code[0] = '\0';
        audio_content_music_finished(completed);
    }
    if ('\0' != s_music_code[0]) {
        audio_preload(s_music_code, FETCH_P0);
        Asset* asset = playable(s_music_code);
        int current = -1;
        for (int i = 0; 2 > i; i++) {
            if (asset && asset == s_music[i].asset) current = i;
        }
        if (0 <= current) {
            if (s_music[current].params.pitch != s_music_params.pitch) SetMusicPitch(s_music[current].sound, s_music_params.pitch);
            if (s_music[current].params.pan != s_music_params.pan) SetMusicPan(s_music[current].sound, s_music_params.pan);
            s_music[current].params = s_music_params;
            if (1.0f != s_music[current].fade_to) {
                fade(&s_music[current], 1, s_music_params.fade_seconds);
                fade(&s_music[1 - current], 0, s_music_params.fade_seconds);
            }
        } else if (asset) {
            int slot = !s_music[0].asset ? 0 : !s_music[1].asset ? 1 :
                       s_music[0].gain <= s_music[1].gain ? 0 : 1;
            if (begin_voice(&s_music[slot], asset, s_music_params)) fade(&s_music[1 - slot], 0, s_music_params.fade_seconds);
        }
    }
    for (int i = 0; AUDIO_PENDING_CAP > i; i++) {
        Pending* pending = &s_pending[i];
        if ('\0' == pending->code[0]) continue;
        if (s_time > pending->expires) { pending->code[0] = '\0'; continue; }
        Asset* asset = playable(pending->code);
        if (asset) { play_sfx(asset, pending->params); pending->code[0] = '\0'; }
    }
}

bool audio_init(void) {
    if (s_ready) return true;
    if (fetch_disabled(STREAM_AUDIO_DISABLED)) return false;
    SetAudioStreamBufferSizeDefault(1024);
    InitAudioDevice();
    s_ready = IsAudioDeviceReady();
    if (!s_ready) return false;
    for (int i = 0; AUDIO_BUS_COUNT > i; i++) s_volumes[i] = 1;
    s_volumes[AUDIO_MUSIC] = 0.45f;
    return true;
}

void audio_start(void) {
    if (!s_ready || s_started) return;
    s_started = true;
    audio_content_start();
}

void audio_shutdown(void) {
    audio_content_shutdown();
    s_started = false;
    for (int i = 0; AUDIO_SFX_CAP > i; i++) release_voice(&s_sfx[i]);
    for (int i = 0; 2 > i; i++) release_voice(&s_music[i]);
    for (int i = 0; AUDIO_ASSET_CAP > i; i++) release_asset(&s_assets[i]);
    memset(s_pending, 0, sizeof(s_pending));
    s_music_code[0] = '\0';
    s_time = 0;
    if (s_ready) CloseAudioDevice();
    s_ready = false;
}
