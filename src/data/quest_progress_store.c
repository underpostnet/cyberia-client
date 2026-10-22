#include "quest_progress_store.h"
#include "quest_cache.h"

#include "audio/audio.h"
#include "audio/audio_events.h"
#include "utils.h"

#include <stdio.h>
#include <string.h>

static QuestProgressEntry s_entries[QUEST_PROGRESS_STORE_CAP];
static int        s_count = 0;
static int        s_revision = 0;

void quest_progress_store_reset(void) {
    s_count = 0;
    s_revision++;
}

int quest_progress_store_revision(void) { return s_revision; }

static QuestStatus quest_progress_store_parse_status(const char* status_str) {
    if (status_str) {
        if (0 == strcmp(status_str, "completed")) return QUEST_COMPLETED;
        if (0 == strcmp(status_str, "failed"))    return QUEST_FAILED;
    }
    return QUEST_ACTIVE;
}

static const char* const OBJECTIVE_TYPE_NAMES[QUEST_OBJECTIVE_TYPE_COUNT] = {
    [QUEST_OBJECTIVE_KILL] = "kill", [QUEST_OBJECTIVE_TALK] = "talk", [QUEST_OBJECTIVE_COLLECT] = "collect",
};

QuestObjectiveType quest_objective_type(const char* name) {
    for (int t = 0; NULL != name && QUEST_OBJECTIVE_TYPE_COUNT > t; t++)
        if (0 == strcmp(name, OBJECTIVE_TYPE_NAMES[t])) return (QuestObjectiveType)t;
    return QUEST_OBJECTIVE_TYPE_COUNT;
}

const char* quest_objective_type_name(QuestObjectiveType type) {
    return QUEST_OBJECTIVE_TYPE_COUNT > type ? OBJECTIVE_TYPE_NAMES[type] : "";
}

const char* quest_objective_verb(QuestObjectiveType type) {
    static const char* const VERBS[QUEST_OBJECTIVE_TYPE_COUNT] = {
        [QUEST_OBJECTIVE_KILL] = "Defeat", [QUEST_OBJECTIVE_TALK] = "Talk to", [QUEST_OBJECTIVE_COLLECT] = "Collect",
    };
    return QUEST_OBJECTIVE_TYPE_COUNT > type ? VERBS[type] : "Complete";
}

static QuestProgressEntry* find_by_code(const char* code) {
    for (int i = 0; i < s_count; ++i) {
        if (0 == strcmp(s_entries[i].code, code)) return &s_entries[i];
    }
    return NULL;
}

static QuestProgressEntry* upsert(const QuestSnapshot* snapshot) {
    const char* code = snapshot->code;
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
    copy_str(e->active_step, QUEST_STEP_MAX, snapshot->active_step);
    copy_str(e->step_id, META_CACHE_CODE_MAX, snapshot->step_id);
    e->objective_count = snapshot->objective_count;
    e->objectives_text[0] = '\0';
    for (int i = 0; snapshot->objective_count > i; i++) {
        const QuestObjectiveProgress* o = &snapshot->objectives[i];
        e->objectives[i] = *o;
        size_t length = strlen(e->objectives_text);
        snprintf(e->objectives_text + length, sizeof e->objectives_text - length, "%s%d/%d %s %s",
                 0 == i ? "" : ", ", o->current, o->required, quest_objective_type_name(o->type), o->item_id);
    }
    e->status = quest_progress_store_parse_status(snapshot->status);
    e->rewards_held = snapshot->rewards_held;
    s_revision++;
    return e;
}

void quest_progress_store_upsert(const QuestSnapshot* snapshot) {
    upsert(snapshot);
}

void quest_progress_store_apply(const QuestSnapshot* snapshot, bool granted) {
    const QuestProgressEntry* prev = snapshot->code ? find_by_code(snapshot->code) : NULL;
    QuestStatus status = quest_progress_store_parse_status(snapshot->status);
    bool rewards_held = snapshot->rewards_held;

    QuestChange change = QUEST_CHANGE_NONE;
    if (QUEST_COMPLETED == status && !(prev && QUEST_COMPLETED == prev->status)) {
        change = QUEST_CHANGE_COMPLETED;
    } else if (granted) {
        change = QUEST_CHANGE_ACCEPTED;
    } else if (rewards_held && !(prev && prev->rewards_held)) {
        change = QUEST_CHANGE_REWARDS_HELD;
    } else if (QUEST_ACTIVE == status && prev && QUEST_ACTIVE == prev->status && '\0' != prev->step_id[0] &&
               0 != strcmp(prev->step_id, snapshot->step_id)) {
        /* The step changes exactly when the previous one finished. */
        change = QUEST_CHANGE_STEP_DONE;
    }

    QuestProgressEntry* e = upsert(snapshot);
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
