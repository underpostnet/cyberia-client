/**
 * @file notify_store.h
 * @brief Per-entity message store and unread-badge counter — pure C, no JS round-trip.
 *
 * Replaces the JS-side notify_badge module so that interaction_bubble_draw()
 * can read unread counts without crossing the C↔JS boundary every frame.
 *
 * Drop-in replacement API (same semantics as js_notify_badge_*):
 *   notify_store_push()           ← was js_notify_badge_push()
 *   notify_store_unread_count()   ← was js_notify_badge_count()
 *   notify_store_mark_read()      ← was js_notify_badge_read()
 */

#ifndef NOTIFY_STORE_H
#define NOTIFY_STORE_H

#include <stddef.h>

#define NS_MAX_ENTITIES        64
#define NS_MAX_MESSAGES        100
#define NS_ENTITY_ID_LEN       64
#define NS_SENDER_LEN          64
#define NS_TEXT_LEN            256

typedef struct {
    char   sender[NS_SENDER_LEN];
    char   text[NS_TEXT_LEN];
    double ts_ms;   /* Unix timestamp in milliseconds */
} NotifyMessage;

typedef struct {
    char          entity_id[NS_ENTITY_ID_LEN];
    NotifyMessage messages[NS_MAX_MESSAGES];
    int           count;
    int           unread;
} NotifyEntry;

/** Push a chat message and increment unread counter for entity_id. */
void notify_store_push(const char* entity_id, const char* sender, const char* text);

/** Return unread message count for entity_id (0 if unknown). */
int  notify_store_unread_count(const char* entity_id);

/** Mark all messages as read for entity_id. */
void notify_store_mark_read(const char* entity_id);

/** Return the NotifyEntry for entity_id, or NULL if not found. */
const NotifyEntry* notify_store_get(const char* entity_id);

#endif /* NOTIFY_STORE_H */
