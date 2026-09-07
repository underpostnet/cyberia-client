#include "audio.h"
#include "audio_events.h"
#include "audio_internal.h"
#include "network/engine_client.h"
#include "util/serial.h"
#include "util/utils.h"

#include <cJSON.h>
#include <assert.h>
#include <ctype.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAP_CAP 8
#define EVENT_CAP 32

typedef struct {
    char event[AUDIO_CODE_CAP];
    char code[AUDIO_CODE_CAP];
    AudioParams params;
} Binding;

typedef struct {
    char code[AUDIO_CODE_CAP];
    char music[AUDIO_CODE_CAP];
    AudioParams params;
    Binding events[EVENT_CAP];
    int count;
    uint64_t token;
    uint64_t touched;
    double retry_at;
    bool loading;
    bool resolved;
} Map;

static Map s_maps[MAP_CAP];
static Map* s_current;
static uint64_t s_sequence;
static double s_time;
static bool s_started;
static char s_context[AUDIO_CODE_CAP];

static AudioParams settings(const cJSON* json, AudioParams params) {
    if (!cJSON_IsObject(json)) return params;
    char bus[16] = {0};
    serial_get_string(json, "bus", bus, sizeof(bus));
    if (0 == strcmp(bus, AUDIO_BUS_ID_MUSIC)) params.bus = AUDIO_MUSIC;
    if (0 == strcmp(bus, AUDIO_BUS_ID_SFX)) params.bus = AUDIO_SFX;
    params.volume = serial_get_float_default(json, "volume", params.volume);
    params.pitch = serial_get_float_default(json, "pitch", params.pitch);
    params.pan = serial_get_float_default(json, "pan", params.pan);
    params.fade_seconds = serial_get_float_default(json, "crossfadeMs", params.fade_seconds * 1000) / 1000;
    params.loop = serial_get_bool_default(json, "loop", params.loop);
    params.priority = serial_get_int_default(json, "priority", params.priority);
    return params;
}

static Binding* binding(Map* map, const char* event) {
    for (int i = 0; map->count > i; i++) {
        if (0 == strcmp(map->events[i].event, event)) return &map->events[i];
    }
    return NULL;
}

static void read_config(Map* map, const cJSON* doc) {
    if (!cJSON_IsObject(doc)) return;
    const cJSON* music = cJSON_GetObjectItemCaseSensitive(doc, "defaultMusic");
    if (cJSON_IsString(music)) copy_str(map->music, sizeof(map->music), music->valuestring);
    const cJSON* overrides = serial_get_object(doc, "settings");
    map->params = settings(overrides, map->params);
    map->params.bus = AUDIO_MUSIC;
    const cJSON* entry = NULL;
    cJSON_ArrayForEach(entry, serial_get_array(doc, "events")) {
        char event[AUDIO_CODE_CAP] = {0}, code[AUDIO_CODE_CAP] = {0};
        if (0 != serial_get_string(entry, "logicEventId", event, sizeof(event)) ||
            0 != serial_get_string(entry, "audioCode", code, sizeof(code)) || '\0' == event[0] || '\0' == code[0]) continue;
        Binding* target = binding(map, event);
        if (!target) {
            if (EVENT_CAP <= map->count) continue;
            target = &map->events[map->count++];
        }
        copy_str(target->event, sizeof(target->event), event);
        copy_str(target->code, sizeof(target->code), code);
        target->params = settings(serial_get_object(entry, "settings"), settings(overrides, audio_params(AUDIO_SFX)));
    }
}

static void apply_current(void) {
    if (!s_current) return;
    // Every asset this map can play is loaded as soon as the map is known — during the loading
    // screen, alongside the sprites — so nothing is fetched at the moment it is meant to sound.
    audio_preload(s_current->music);
    for (int i = 0; s_current->count > i; i++) audio_preload(s_current->events[i].code);
    if (!s_started) return;
    Binding* active = binding(s_current, s_context);
    if (active && AUDIO_MUSIC == active->params.bus) audio_play(active->code, &active->params);
    else audio_content_restore_music();
}

void audio_content_restore_music(void) {
    s_context[0] = '\0';
    if (!s_started || !s_current) return;
    if ('\0' != s_current->music[0]) audio_play(s_current->music, &s_current->params);
    else audio_stop_music(s_current->params.fade_seconds);
}

void audio_content_music_finished(const char* code) {
    if (s_current && '\0' == s_context[0] && 0 == strcmp(code, s_current->music)) return;
    audio_content_restore_music();
}

/* Use the existing list filter contract and encode the whole JSON query value. */
static void fetch_match(const char* api, const char* field, const char* value,
                        const char* token, FetchCompletedCb callback) {
    cJSON* filter = cJSON_CreateObject();
    assert(filter);
    cJSON* item = cJSON_AddObjectToObject(filter, field);
    assert(item);
    cJSON_AddStringToObject(item, "filterType", "text");
    cJSON_AddStringToObject(item, "type", "equals");
    cJSON_AddStringToObject(item, "filter", value);
    char* json = cJSON_PrintUnformatted(filter);
    assert(json);
    char url[2048];
    size_t used = (size_t)snprintf(url, sizeof(url), "/api/%s?limit=1&filterModel=", api);
    for (const unsigned char* p = (const unsigned char*)json; '\0' != *p && sizeof(url) - 4 > used; p++) {
        if (isalnum(*p) || '-' == *p || '_' == *p || '.' == *p || '~' == *p) url[used++] = (char)*p;
        else used += (size_t)snprintf(url + used, sizeof(url) - used, "%%%02X", *p);
    }
    url[used] = '\0';
    cJSON_free(json);
    cJSON_Delete(filter);
    fetch_request_start_limited(token, url, callback, 65536, 10000);
}

static const cJSON* first_document(const cJSON* root) {
    const cJSON* envelope = envelope_success_doc(root);
    return envelope ? cJSON_GetArrayItem(serial_get_array(envelope, "data"), 0) : NULL;
}

static void on_map(const FetchResponse* response) {
    uint64_t token = strtoull(response->asset_id, NULL, 10);
    for (int i = 0; MAP_CAP > i; i++) {
        Map* map = &s_maps[i];
        if (token != map->token || !map->loading) continue;
        cJSON* root = response->success && 65536 >= response->size ?
                      cJSON_ParseWithLength(response->data, response->size) : NULL;
        const cJSON* doc = first_document(root);
        char code[AUDIO_CODE_CAP] = {0};
        if (doc) serial_get_string(doc, "mapCode", code, sizeof(code));
        // An answer settles the map for the session, with or without a document: a map that
        // carries no configuration plays nothing, which is an answer and not a reason to keep
        // asking. Only a transport failure is retried, so one request per map is the norm.
        map->resolved = response->success;
        map->loading = false;
        map->retry_at = s_time + 30;
        if (doc && 0 == strcmp(code, map->code)) read_config(map, doc);
        cJSON_Delete(root);
        if (s_current == map) apply_current();
        break;
    }
    free(response->data);
}

static void request_map(Map* map) {
    if (map->resolved || map->loading || s_time < map->retry_at) return;
    map->token = ++s_sequence;
    map->loading = true;
    map->retry_at = s_time + 15;
    char token[32];
    snprintf(token, sizeof(token), "%llu", (unsigned long long)map->token);
    fetch_match("cyberia-map-audio-conf", "mapCode", map->code, token, on_map);
}

void audio_set_map(const char* code) {
    if (!code || AUDIO_CODE_CAP <= strlen(code)) return;
    if ('\0' == code[0]) {
        s_current = NULL;
        s_context[0] = '\0';
        audio_stop_music(0.3f);
        return;
    }
    if (s_current && 0 == strcmp(s_current->code, code)) return;
    s_context[0] = '\0';
    Map* map = NULL;
    Map* oldest = &s_maps[0];
    for (int i = 0; MAP_CAP > i; i++) {
        if (0 == strcmp(s_maps[i].code, code)) map = &s_maps[i];
        if (oldest->touched > s_maps[i].touched) oldest = &s_maps[i];
    }
    if (!map) {
        // The map plays nothing until engine-cyberia answers with its configuration; the client
        // ships no audio of its own, so there is no local shape to start from.
        map = oldest;
        *map = (Map){ .params = audio_params(AUDIO_MUSIC) };
        copy_str(map->code, sizeof(map->code), code);
    }
    s_current = map;
    map->touched = ++s_sequence;
    apply_current();
    request_map(map);
}

void audio_event(const char* event) {
    if (!s_started || !s_current || !event) return;
    if (0 == strcmp(event, AUDIO_EVENT_IDLE)) { audio_content_restore_music(); return; }
    Binding* entry = binding(s_current, event);
    if (!entry) return;
    if (AUDIO_MUSIC == entry->params.bus) copy_str(s_context, sizeof(s_context), event);
    audio_play(entry->code, &entry->params);
}

static void on_wav(const FetchResponse* response) {
    uint64_t token = strtoull(response->asset_id, NULL, 10);
    audio_request_complete(token, response->success ? response->data : NULL, response->size);
    free(response->data);
}

static void on_asset(const FetchResponse* response) {
    uint64_t token = strtoull(response->asset_id, NULL, 10);
    const char* requested_code = audio_request_code(token);
    if (!requested_code) { free(response->data); return; }
    cJSON* root = response->success && 65536 >= response->size ?
                  cJSON_ParseWithLength(response->data, response->size) : NULL;
    char file_id[32] = {0}, code[AUDIO_CODE_CAP] = {0};
    const cJSON* doc = first_document(root);
    if (doc) {
        serial_get_string(doc, "fileId", file_id, sizeof(file_id));
        serial_get_string(doc, "code", code, sizeof(code));
    }
    bool valid = 24 == strlen(file_id) && 0 == strcmp(code, requested_code);
    for (int i = 0; valid && 24 > i; i++) valid = 0 != isxdigit((unsigned char)file_id[i]);
    // The engine answering "no such asset" is an answer. A binding can outlive the asset it
    // names — a renamed cue, a configuration seeded before its bank — and re-asking every thirty
    // seconds for something that does not exist is noise, not resilience. Only a transport
    // failure is worth another attempt.
    bool answered = response->success && NULL != root;
    cJSON_Delete(root);
    free(response->data);
    if (!valid) {
        if (answered) audio_request_absent(token);
        else audio_request_complete(token, NULL, 0);
        return;
    }
    char url[64];
    snprintf(url, sizeof(url), "/api/file/blob/%s", file_id);
    fetch_request_start_limited(response->asset_id, url, on_wav, AUDIO_WAV_MAX, 10000);
}

void audio_content_fetch(const char* code, uint64_t token) {
    char id[32];
    snprintf(id, sizeof(id), "%llu", (unsigned long long)token);
    fetch_match("cyberia-audio", "code", code, id, on_asset);
}

void audio_content_start(void) {
    // The map and its assets were resolved during loading; starting only releases playback.
    s_started = true;
    apply_current();
    if (s_current) request_map(s_current);
}

void audio_content_update(float dt) {
    s_time += dt;
    for (int i = 0; MAP_CAP > i; i++) {
        if (s_maps[i].loading && s_time >= s_maps[i].retry_at) {
            s_maps[i].loading = false;
            s_maps[i].retry_at = s_time + 30;
        }
    }
    if (s_current) request_map(s_current);
}

void audio_content_shutdown(void) {
    s_started = false;
    s_current = NULL;
    s_context[0] = '\0';
    s_time = 0;
    memset(s_maps, 0, sizeof(s_maps));
}
