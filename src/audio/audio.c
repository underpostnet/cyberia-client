#include "audio.h"
#include "audio_internal.h"
#include "util/utils.h"

#include <raylib.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define AUDIO_CACHE_BYTES (32u * 1024u * 1024u)
#define AUDIO_PENDING_CAP 16

typedef struct {
    char code[AUDIO_CODE_CAP];
    AudioAssetState state;
    Sound sound;
    uint64_t token;
    uint64_t touched;
    double retry_at;
    double last_started;
    bool absent;
    size_t bytes;
    int references;
} Asset;

typedef struct {
    Asset* asset;
    Sound sound;
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
    StopSound(voice->sound);
    UnloadSoundAlias(voice->sound);
    voice->asset->references--;
    *voice = (Voice){0};
}

static void release_asset(Asset* asset) {
    if (AUDIO_READY == asset->state) UnloadSound(asset->sound);
    s_bytes -= asset->bytes;
    *asset = (Asset){0};
}

static Asset* oldest_asset(const Asset* except, bool resident_only) {
    Asset* oldest = NULL;
    for (int i = 0; AUDIO_ASSET_CAP > i; i++) {
        Asset* asset = &s_assets[i];
        if (except == asset || 0 != asset->references || AUDIO_LOADING == asset->state ||
            (resident_only && 0 == asset->bytes)) continue;
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
    for (size_t offset = 12; size - 8 >= offset;) {
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

bool audio_load_wav(const char* code, const void* bytes, size_t size) {
    if (!s_ready || !valid_wav(bytes, size)) return false;
    Asset* asset = reserve_asset(code);
    if (!asset || 0 != asset->references) return false;
    Wave wave = LoadWaveFromMemory(".wav", bytes, (int)size);
    if (!IsWaveValid(wave)) return false;
    /* Raylib stores device-rate stereo floats; reserve for up to 48 kHz. */
    size_t required = ((size_t)wave.frameCount * 48000 / wave.sampleRate + 1) * 8;
    while (AUDIO_CACHE_BYTES < s_bytes - asset->bytes + required) {
        Asset* victim = oldest_asset(asset, true);
        if (!victim || 0 == victim->bytes) { UnloadWave(wave); return false; }
        release_asset(victim);
    }
    Sound sound = LoadSoundFromWave(wave);
    UnloadWave(wave);
    if (!IsSoundValid(sound)) return false;
    required = (size_t)sound.frameCount * sound.stream.channels * (sound.stream.sampleSize / 8);
    while (AUDIO_CACHE_BYTES < s_bytes - asset->bytes + required) {
        Asset* victim = oldest_asset(asset, true);
        if (!victim) { UnloadSound(sound); return false; }
        release_asset(victim);
    }
    if (AUDIO_READY == asset->state) UnloadSound(asset->sound);
    s_bytes -= asset->bytes;
    asset->sound = sound;
    asset->bytes = required;
    asset->state = AUDIO_READY;
    asset->touched = ++s_sequence;
    s_bytes += required;
    return true;
}

AudioAssetState audio_asset_state(const char* code) {
    Asset* asset = find_asset(code);
    return asset ? asset->state : AUDIO_ABSENT;
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
        if (token != asset->token || AUDIO_LOADING != asset->state) continue;
        asset->state = AUDIO_FAILED;
        asset->absent = true;
        return;
    }
}

void audio_preload(const char* code) {
    // Loading is deliberately not gated on the tap: an asset fetched during the loading screen
    // plays the instant the world starts, where one fetched on first use would arrive late and
    // silently miss the moment it was meant to mark.
    if (!s_ready) return;
    Asset* asset = reserve_asset(code);
    if (!asset || asset->absent || AUDIO_READY == asset->state || AUDIO_LOADING == asset->state ||
        s_time < asset->retry_at) return;
    asset->state = AUDIO_LOADING;
    asset->token = ++s_sequence;
    asset->retry_at = s_time + 25.0;
    audio_content_fetch(code, asset->token);
}

const char* audio_request_code(uint64_t token) {
    for (int i = 0; s_ready && AUDIO_ASSET_CAP > i; i++) {
        if (token == s_assets[i].token && AUDIO_LOADING == s_assets[i].state) return s_assets[i].code;
    }
    return NULL;
}

void audio_request_complete(uint64_t token, const void* bytes, size_t size) {
    for (int i = 0; s_ready && AUDIO_ASSET_CAP > i; i++) {
        Asset* asset = &s_assets[i];
        if (token != asset->token || AUDIO_LOADING != asset->state) continue;
        if (!audio_load_wav(asset->code, bytes, size)) {
            asset->state = AUDIO_FAILED;
            asset->retry_at = s_time + 30.0;
        }
        return;
    }
}

/* Every asset arrives from engine-cyberia, so a code is playable once its fetch has landed. */
static Asset* playable(const char* code, AudioBus bus) {
    (void)bus;
    Asset* asset = find_asset(code);
    return asset && AUDIO_READY == asset->state ? asset : NULL;
}

static void apply_volume(Voice* voice) {
    SetSoundVolume(voice->sound, voice->gain * voice->params.volume *
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
    release_voice(voice);
    Sound alias = LoadSoundAlias(asset->sound);
    if (!IsSoundValid(alias)) return false;
    *voice = (Voice){ .asset = asset, .sound = alias, .params = params, .order = ++s_sequence,
                      .gain = 0.0f < params.fade_seconds ? 0.0f : 1.0f };
    asset->references++;
    asset->touched = s_sequence;
    fade(voice, 1.0f, params.fade_seconds);
    SetSoundPitch(alias, params.pitch);
    SetSoundPan(alias, params.pan);
    apply_volume(voice);
    PlaySound(alias);
    return true;
}

/* Two identical cues closer together than this read as one impact rather than a flam. Distinct
 * cues are never affected, and a repeating cue at any musical rate clears it easily. */
#define SFX_RETRIGGER_SECONDS 0.06

static bool play_sfx(Asset* asset, AudioParams params) {
    Voice* choice = NULL;
    for (int i = 0; AUDIO_SFX_CAP > i; i++) {
        Voice* voice = &s_sfx[i];
        if (!voice->asset || !IsSoundPlaying(voice->sound)) { choice = voice; break; }
        if (!choice || choice->params.priority > voice->params.priority ||
            (choice->params.priority == voice->params.priority && choice->order > voice->order)) choice = voice;
    }
    if (choice->asset && IsSoundPlaying(choice->sound) && params.priority < choice->params.priority) return false;
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
    audio_preload(code);
    if (AUDIO_MUSIC == params.bus) {
        copy_str(s_music_code, sizeof(s_music_code), code);
        s_music_params = params;
        return true;
    }
    Asset* asset = playable(code, params.bus);
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
    if (0.0f < voice->fade_duration) {
        voice->fade_elapsed += dt;
        float amount = fminf(1, voice->fade_elapsed / voice->fade_duration);
        voice->gain = voice->fade_from + (voice->fade_to - voice->fade_from) * amount;
    }
    if (0.0f == voice->fade_to && 0.0001f >= voice->gain) { release_voice(voice); return; }
    if (!IsSoundPlaying(voice->sound)) {
        if (voice->params.loop) PlaySound(voice->sound);
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
        if (AUDIO_LOADING == asset->state && s_time >= asset->retry_at) audio_request_complete(asset->token, NULL, 0);
    }
    if (!s_started) return;
    for (int i = 0; AUDIO_SFX_CAP > i; i++) update_voice(&s_sfx[i], dt);
    bool finished = false;
    for (int i = 0; 2 > i; i++) {
        if (s_music[i].asset && !s_music[i].params.loop && 1.0f == s_music[i].fade_to &&
            !IsSoundPlaying(s_music[i].sound)) finished = true;
        update_voice(&s_music[i], dt);
    }
    if (finished) {
        char completed[AUDIO_CODE_CAP];
        copy_str(completed, sizeof(completed), s_music_code);
        s_music_code[0] = '\0';
        audio_content_music_finished(completed);
    }
    if ('\0' != s_music_code[0]) {
        audio_preload(s_music_code);
        Asset* asset = playable(s_music_code, AUDIO_MUSIC);
        int current = -1;
        for (int i = 0; 2 > i; i++) {
            if (asset && asset == s_music[i].asset) current = i;
        }
        if (0 <= current) {
            if (s_music[current].params.pitch != s_music_params.pitch) SetSoundPitch(s_music[current].sound, s_music_params.pitch);
            if (s_music[current].params.pan != s_music_params.pan) SetSoundPan(s_music[current].sound, s_music_params.pan);
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
        Asset* asset = playable(pending->code, AUDIO_SFX);
        if (asset) { play_sfx(asset, pending->params); pending->code[0] = '\0'; }
    }
}

bool audio_init(void) {
    if (s_ready) return true;
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
