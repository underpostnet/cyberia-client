/**
 * @file notify_store.h
 * @brief Per-entity chat message store + unread-badge counter — pure C.
 *
 * Single source of truth for chat unread state. The interaction bubble reads
 * the last message for its informational chat bubble; the interaction modal's
 * Chat button reads/clears the unread count.
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
} NotifyEntry;

/** Append a chat message for entity_id. Counts live in the notification
 *  dispatcher (notification.h), not here. */
void notify_store_push(const char* entity_id, const char* sender, const char* text);

/** Return the NotifyEntry for entity_id, or NULL if not found. */
const NotifyEntry* notify_store_get(const char* entity_id);

#endif /* NOTIFY_STORE_H */
