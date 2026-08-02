#ifndef CYBERIA_UI_STATE_H
#define CYBERIA_UI_STATE_H

#include <stddef.h>

#include "object_layer.h"

/* UI-only data fed by engine metadata.
 *
 *   - skill_map: trigger-item-id → skill-event mapping, pushed by the
 *     server's init_data so the client can render the right skill labels.
 *
 * A pure presentation lookup table that never participates in gameplay
 * simulation, which is why it lives outside GameState.
 */

#define UI_STATE_MAX_SKILL_ENTRIES 64

typedef struct {
    char trigger_item_id[MAX_ITEM_ID_LENGTH];
    char logic_event_id[MAX_ITEM_ID_LENGTH];
    char name[MAX_ITEM_ID_LENGTH];
    char description[256];
    char summoned_entity_item_id[MAX_ITEM_ID_LENGTH];
} UiSkillEntry;

void ui_state_reset(void);

void   ui_state_clear_skills(void);
int    ui_state_skill_count(void);
const UiSkillEntry* ui_state_skill_at(int idx);
int    ui_state_push_skill(const UiSkillEntry* entry);

#endif /* CYBERIA_UI_STATE_H */
