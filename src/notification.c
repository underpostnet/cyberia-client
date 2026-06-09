#include "notification.h"

#include <stdbool.h>
#include <string.h>

typedef struct {
    char id[NOTIF_TARGET_ID_LEN];
    int  counts[NOTIF_REGISTER_COUNT];
} NotifTarget;

static NotifTarget s_targets[NOTIF_MAX_TARGETS];
static int         s_count = 0;

static NotifTarget* find(const char* target_id) {
    for (int i = 0; i < s_count; i++) {
        if (0 == strncmp(s_targets[i].id, target_id, NOTIF_TARGET_ID_LEN - 1)) {
            return &s_targets[i];
        }
    }
    return NULL;
}

static NotifTarget* find_or_create(const char* target_id) {
    NotifTarget* t = find(target_id);
    if (t) return t;
    if (s_count >= NOTIF_MAX_TARGETS) return NULL;
    t = &s_targets[s_count++];
    memset(t, 0, sizeof(*t));
    strncpy(t->id, target_id, NOTIF_TARGET_ID_LEN - 1);
    return t;
}

static bool valid(NotifRegister reg, const char* target_id) {
    return target_id && '\0' != target_id[0] && reg >= 0 && reg < NOTIF_REGISTER_COUNT;
}

void notification_push(NotifRegister reg, const char* target_id) {
    if (!valid(reg, target_id)) return;
    NotifTarget* t = find_or_create(target_id);
    if (t) t->counts[reg]++;
}

void notification_clear(NotifRegister reg, const char* target_id) {
    if (!valid(reg, target_id)) return;
    NotifTarget* t = find(target_id);
    if (t) t->counts[reg] = 0;
}

int notification_count(NotifRegister reg, const char* target_id) {
    if (!valid(reg, target_id)) return 0;
    NotifTarget* t = find(target_id);
    return t ? t->counts[reg] : 0;
}

int notification_target_total(const char* target_id) {
    if (!target_id || '\0' == target_id[0]) return 0;
    NotifTarget* t = find(target_id);
    if (!t) return 0;
    int sum = 0;
    for (int r = 0; r < NOTIF_REGISTER_COUNT; r++) sum += t->counts[r];
    return sum;
}

int notification_register_total(NotifRegister reg) {
    if (reg < 0 || reg >= NOTIF_REGISTER_COUNT) return 0;
    int sum = 0;
    for (int i = 0; i < s_count; i++) sum += s_targets[i].counts[reg];
    return sum;
}

int notification_total(void) {
    int sum = 0;
    for (int i = 0; i < s_count; i++)
        for (int r = 0; r < NOTIF_REGISTER_COUNT; r++) sum += s_targets[i].counts[r];
    return sum;
}
