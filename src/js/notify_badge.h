/**
 * @file notify_badge.h
 * @brief C declarations for the JS notification badge module.
 *
 * The badge system stores per-entity messages and unread counts entirely
 * in JS.  These extern functions are implemented in js/notify_badge.js.
 */

#ifndef NOTIFY_BADGE_H
#define NOTIFY_BADGE_H

/* Push a chat message — increments unread counter for entity_id. */
extern void js_notify_badge_push(const char* entity_id,
                                 const char* sender,
                                 const char* text);

/* Return unread message count for an entity (0 if none). */
extern int  js_notify_badge_count(const char* entity_id);

/* Mark all messages as read for an entity. */
extern void js_notify_badge_read(const char* entity_id);

#endif /* NOTIFY_BADGE_H */
