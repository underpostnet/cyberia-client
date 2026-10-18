#include "ui_state.h"

#include <string.h>

static struct {
    UiSkillEntry skills[UI_STATE_MAX_SKILL_ENTRIES];
    int          skill_count;
} g_ui = {0};

void ui_state_reset(void) {
    g_ui.skill_count = 0;
}

int  ui_state_skill_count(void)             { return g_ui.skill_count; }

const UiSkillEntry* ui_state_skill_at(int idx) {
    if (idx < 0 || idx >= g_ui.skill_count) return NULL;
    return &g_ui.skills[idx];
}

int ui_state_push_skill(const UiSkillEntry* entry) {
    if (!entry || g_ui.skill_count >= UI_STATE_MAX_SKILL_ENTRIES) return -1;
    g_ui.skills[g_ui.skill_count] = *entry;
    return g_ui.skill_count++;
}

void ui_state_drop_skills(const char* trigger_item_id) {
    int kept = 0;
    for (int i = 0; i < g_ui.skill_count; i++)
        if (0 != strcmp(g_ui.skills[i].trigger_item_id, trigger_item_id)) g_ui.skills[kept++] = g_ui.skills[i];
    g_ui.skill_count = kept;
}
