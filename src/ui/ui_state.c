#include "ui_state.h"

#include <string.h>

static struct {
    char         associated_items[UI_STATE_MAX_ITEM_IDS][MAX_ITEM_ID_LENGTH];
    int          associated_item_count;
    UiSkillEntry skills[UI_STATE_MAX_SKILL_ENTRIES];
    int          skill_count;
} g_ui = {0};

void ui_state_reset(void) {
    g_ui.associated_item_count = 0;
    g_ui.skill_count           = 0;
}

void ui_state_clear_associated_items(void) { g_ui.associated_item_count = 0; }
int  ui_state_associated_item_count(void)  { return g_ui.associated_item_count; }

const char* ui_state_associated_item_at(int idx) {
    if (idx < 0 || idx >= g_ui.associated_item_count) return NULL;
    return g_ui.associated_items[idx];
}

int ui_state_push_associated_item(const char* item_id) {
    if (!item_id || g_ui.associated_item_count >= UI_STATE_MAX_ITEM_IDS) return -1;
    strncpy(g_ui.associated_items[g_ui.associated_item_count],
            item_id, MAX_ITEM_ID_LENGTH - 1);
    g_ui.associated_items[g_ui.associated_item_count][MAX_ITEM_ID_LENGTH - 1] = '\0';
    return g_ui.associated_item_count++;
}

void ui_state_clear_skills(void)            { g_ui.skill_count = 0; }
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
