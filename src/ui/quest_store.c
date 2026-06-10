#include "quest_store.h"

#include <string.h>

static QuestEntry s_entries[QUEST_STORE_CAP];
static int        s_count = 0;

void quest_store_reset(void) {
    s_count = 0;
}

QuestStatus quest_store_parse_status(const char* status_str) {
    if (status_str) {
        if (0 == strcmp(status_str, "completed")) return QUEST_COMPLETED;
        if (0 == strcmp(status_str, "failed"))    return QUEST_FAILED;
    }
    return QUEST_ACTIVE;
}

static void copy_field(char* dst, size_t cap, const char* src) {
    if (!src) { dst[0] = '\0'; return; }
    strncpy(dst, src, cap - 1);
    dst[cap - 1] = '\0';
}

static QuestEntry* find_by_code(const char* code) {
    for (int i = 0; i < s_count; ++i) {
        if (0 == strcmp(s_entries[i].code, code)) return &s_entries[i];
    }
    return NULL;
}

bool quest_store_upsert(const char* code, const char* title, const char* description,
                        const char* status_str, const char* active_step,
                        const char* objectives) {
    if (!code || '\0' == code[0]) return false;

    bool        added = false;
    QuestEntry* e     = find_by_code(code);
    if (NULL == e) {
        if (s_count >= QUEST_STORE_CAP) return false;
        e = &s_entries[s_count++];
        copy_field(e->code, QUEST_CODE_MAX, code);
        added = true;
    }
    if (title) copy_field(e->title, QUEST_TITLE_MAX, title);
    if (description) copy_field(e->description, QUEST_DESC_MAX, description);
    copy_field(e->active_step, QUEST_STEP_MAX, active_step);
    copy_field(e->objectives,  QUEST_OBJECTIVES_MAX, objectives);
    e->status = quest_store_parse_status(status_str);
    return added;
}

int quest_store_count(QuestStatus status) {
    int n = 0;
    for (int i = 0; i < s_count; ++i) {
        if (s_entries[i].status == status) ++n;
    }
    return n;
}

const QuestEntry* quest_store_get(QuestStatus status, int index) {
    int seen = 0;
    for (int i = 0; i < s_count; ++i) {
        if (s_entries[i].status != status) continue;
        if (seen == index) return &s_entries[i];
        ++seen;
    }
    return NULL;
}

const QuestEntry* quest_store_find(const char* code) {
    if (!code) return NULL;
    const QuestEntry* e = find_by_code(code);
    return e;
}

bool quest_store_is_completed(const char* code) {
    const QuestEntry* e = quest_store_find(code);
    return e && QUEST_COMPLETED == e->status;
}

bool quest_store_set_meta(const char* code, const char* title,
                          const char* description) {
    QuestEntry* e = find_by_code(code);
    if (!e) return false;
    if (title) copy_field(e->title, QUEST_TITLE_MAX, title);
    if (description) copy_field(e->description, QUEST_DESC_MAX, description);
    return true;
}
