/**
 * @file notify_store.c
 * @brief Pure-C per-entity message store and unread-badge counter.
 */

#include "notify_store.h"

#include "util/log.h"

#include <raylib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

/* ── Module state ─────────────────────────────────────────────────────── */

static NotifyEntry s_entries[NS_MAX_ENTITIES];
static int         s_count = 0;

/* ── Helpers ──────────────────────────────────────────────────────────── */

static NotifyEntry* find_or_create(const char* entity_id) {
    if (!entity_id || entity_id[0] == '\0') return NULL;

    /* Look for existing entry */
    for (int i = 0; i < s_count; i++) {
        if (strncmp(s_entries[i].entity_id, entity_id, NS_ENTITY_ID_LEN - 1) == 0) {
            return &s_entries[i];
        }
    }

    /* Create new entry if space available */
    if (s_count >= NS_MAX_ENTITIES) {
        LOG_ERROR("[NOTIFY_STORE] Max entity slots reached, dropping entry for %s", entity_id);
        return NULL;
    }

    NotifyEntry* e = &s_entries[s_count++];
    memset(e, 0, sizeof(*e));
    strncpy(e->entity_id, entity_id, NS_ENTITY_ID_LEN - 1);
    return e;
}

/* ── Public API ───────────────────────────────────────────────────────── */

void notify_store_push(const char* entity_id, const char* sender, const char* text) {
    NotifyEntry* e = find_or_create(entity_id);
    if (!e) return;

    /* Ring-buffer: drop oldest when full */
    if (e->count >= NS_MAX_MESSAGES) {
        memmove(e->messages, e->messages + 1,
                sizeof(NotifyMessage) * (NS_MAX_MESSAGES - 1));
        e->count = NS_MAX_MESSAGES - 1;
    }

    NotifyMessage* m = &e->messages[e->count++];
    strncpy(m->sender, sender ? sender : "",  NS_SENDER_LEN - 1);
    strncpy(m->text,   text   ? text   : "",  NS_TEXT_LEN   - 1);
    m->ts_ms = GetTime() * 1000.0;
}

const NotifyEntry* notify_store_get(const char* entity_id) {
    if (!entity_id) return NULL;
    for (int i = 0; i < s_count; i++) {
        if (strncmp(s_entries[i].entity_id, entity_id, NS_ENTITY_ID_LEN - 1) == 0) {
            return &s_entries[i];
        }
    }
    return NULL;
}
