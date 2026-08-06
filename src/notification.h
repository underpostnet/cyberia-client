#ifndef NOTIFICATION_H
#define NOTIFICATION_H

/* Sole authority for unread notification counts, along two axes: register
 * (the category — chat today) and target (the entity the notification is
 * about). Message text lives in notify_store; this module only counts. */

#define NOTIF_MAX_TARGETS    64
#define NOTIF_TARGET_ID_LEN  64

typedef enum {
    NOTIF_CHAT = 0,
    NOTIF_REGISTER_COUNT,
} NotifRegister;

/* Increment the count for (register, target). */
void notification_push(NotifRegister reg, const char* target_id);

/* Clear the count for (register, target) — e.g. the user viewed that chat. */
void notification_clear(NotifRegister reg, const char* target_id);

/* Per-register, per-target count (badge on the Chat button). */
int notification_count(NotifRegister reg, const char* target_id);

/* All registers for one target (the generic badge on a bubble). */
int notification_target_total(const char* target_id);

/* One register across all targets. */
int notification_register_total(NotifRegister reg);

/* Everything, all registers and targets. */
int notification_total(void);

#endif /* NOTIFICATION_H */
