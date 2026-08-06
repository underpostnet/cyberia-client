#ifndef NOTIFY_STORE_H
#define NOTIFY_STORE_H

/* Per-entity chat message store. The interaction bubble reads the last
 * message for its chat bubble. Unread counts live in notification.h. */

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

void notify_store_push(const char* entity_id, const char* sender, const char* text);

/* NULL when the entity has no messages. */
const NotifyEntry* notify_store_get(const char* entity_id);

#endif /* NOTIFY_STORE_H */
