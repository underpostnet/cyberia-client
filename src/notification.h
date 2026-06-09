/**
 * @file notification.h
 * @brief Central notification dispatcher — counts only, pure C.
 *
 * Single authority for unread notification counts. Counts are tracked along
 * two axes:
 *   - register (category): chat today; extensible (trade, quest, ...).
 *   - target  (entity id): who the notification is about.
 *
 * Exposes a global total, a per-register total, a per-target total (all
 * registers), and the per-register-per-target count used by badges. Message
 * text itself lives in notify_store; this module only counts.
 */

#ifndef NOTIFICATION_H
#define NOTIFICATION_H

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
