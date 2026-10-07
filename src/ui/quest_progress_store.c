#include "quest_progress_store.h"
#include "quest_cache.h"

#include "audio/audio.h"
#include "audio/audio_events.h"
#include "util/utils.h"

#include <string.h>

static QuestProgressEntry s_entries[QUEST_PROGRESS_STORE_CAP];
static int        s_count = 0;

void quest_progress_store_reset(void) {
    s_count = 0;
}

static QuestStatus quest_progress_store_parse_status(const char* status_str) {
    if (status_str) {
        if (0 == strcmp(status_str, "completed")) return QUEST_COMPLETED;
        if (0 == strcmp(status_str, "failed"))    return QUEST_FAILED;
    }
    return QUEST_ACTIVE;
}

static QuestProgressEntry* find_by_code(const char* code) {
    for (int i = 0; i < s_count; ++i) {
        if (0 == strcmp(s_entries[i].code, code)) return &s_entries[i];
    }
    return NULL;
}

static QuestProgressEntry* upsert(const char* code, const char* status_str,
                                  const char* active_step, const char* objectives) {
    if (!code || '\0' == code[0]) return NULL;

    QuestProgressEntry* e = find_by_code(code);
    if (NULL == e) {
        if (s_count >= QUEST_PROGRESS_STORE_CAP) return NULL;
        e = &s_entries[s_count++];
        memset(e, 0, sizeof(*e));
        copy_str(e->code, META_CACHE_CODE_MAX, code);
        /* A miss fetches; quest_cache sets the title through set_meta. */
        const QuestMetadataEntry* qm = quest_cache_get(code);
        if (qm) {
            copy_str(e->title, QUEST_TITLE_MAX, qm->title);
            copy_str(e->description, QUEST_DESC_MAX, qm->description);
        }
    }
    copy_str(e->active_step, QUEST_STEP_MAX, active_step);
    copy_str(e->objectives,  QUEST_OBJECTIVES_MAX, objectives);
    e->status = quest_progress_store_parse_status(status_str);
    return e;
}

void quest_progress_store_upsert(const char* code, const char* status_str,
                                 const char* active_step, const char* objectives) {
    upsert(code, status_str, active_step, objectives);
}

void quest_progress_store_apply(const char* code, const char* status_str,
                                const char* active_step, const char* objectives,
                                bool granted) {
    const QuestProgressEntry* prev = code ? find_by_code(code) : NULL;
    QuestStatus status = quest_progress_store_parse_status(status_str);

    QuestChange change = QUEST_CHANGE_NONE;
    if (QUEST_COMPLETED == status && !(prev && QUEST_COMPLETED == prev->status)) {
        change = QUEST_CHANGE_COMPLETED;
    } else if (granted) {
        change = QUEST_CHANGE_ACCEPTED;
    } else if (QUEST_ACTIVE == status && prev && QUEST_ACTIVE == prev->status &&
               '\0' != prev->active_step[0] && 0 != strcmp(prev->active_step, active_step)) {
        /* The step text changes exactly when the previous step finished. */
        change = QUEST_CHANGE_STEP_DONE;
    }

    QuestProgressEntry* e = upsert(code, status_str, active_step, objectives);
    if (NULL == e || QUEST_CHANGE_NONE == change) return;
    e->change = change;
    if (QUEST_CHANGE_COMPLETED == change) audio_event(AUDIO_EVENT_VICTORY);
}

const QuestProgressEntry* quest_progress_store_take_change(QuestChange* change) {
    for (int i = 0; i < s_count; ++i) {
        if (QUEST_CHANGE_NONE == s_entries[i].change) continue;
        *change = s_entries[i].change;
        s_entries[i].change = QUEST_CHANGE_NONE;
        return &s_entries[i];
    }
    return NULL;
}

int quest_progress_store_count(QuestStatus status) {
    int n = 0;
    for (int i = 0; i < s_count; ++i) {
        if (s_entries[i].status == status) ++n;
    }
    return n;
}

const QuestProgressEntry* quest_progress_store_get(QuestStatus status, int index) {
    int seen = 0;
    for (int i = 0; i < s_count; ++i) {
        if (s_entries[i].status != status) continue;
        if (seen == index) return &s_entries[i];
        ++seen;
    }
    return NULL;
}

const QuestProgressEntry* quest_progress_store_find(const char* code) {
    if (!code) return NULL;
    const QuestProgressEntry* e = find_by_code(code);
    return e;
}

bool quest_progress_store_is_completed(const char* code) {
    const QuestProgressEntry* e = quest_progress_store_find(code);
    return e && QUEST_COMPLETED == e->status;
}

bool quest_progress_store_set_meta(const char* code, const char* title,
                          const char* description) {
    QuestProgressEntry* e = find_by_code(code);
    if (!e) return false;
    if (title) copy_str(e->title, QUEST_TITLE_MAX, title);
    if (description) copy_str(e->description, QUEST_DESC_MAX, description);
    return true;
}
