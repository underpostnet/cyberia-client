#ifndef NOTIFY_STORE_H
#define NOTIFY_STORE_H

/* Per-entity chat message store: lines received from an entity and lines sent
 * to it. The chat pane draws the history; the interaction bubble shows the last
 * received line. Unread counts live in notification.h. */

#include <stdbool.h>
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
    bool   mine;    /* sent by the local player */
} NotifyMessage;

typedef struct {
    char          entity_id[NS_ENTITY_ID_LEN];
    NotifyMessage messages[NS_MAX_MESSAGES];
    int           count;
} NotifyEntry;

void notify_store_push(const char* entity_id, const char* sender, const char* text, bool mine);

/* NULL when the entity has no messages. */
const NotifyEntry* notify_store_get(const char* entity_id);

#endif /* NOTIFY_STORE_H */
