#ifndef CYBERIA_UI_STATE_H
#define CYBERIA_UI_STATE_H

#include <stdbool.h>
#include <stddef.h>

#include "world/object_layer.h"

/* UI-only data fed by engine metadata.
 *
 *   - skill_map: trigger-item-id → skill-event mapping, pushed by the
 *     server's init_data and replaced per trigger by each metadata delta, so
 *     the client can render the right skill labels.
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
    bool held;  /* runs by itself on held units; a tap never fires it */
} UiSkillEntry;

void ui_state_reset(void);

int    ui_state_skill_count(void);
const UiSkillEntry* ui_state_skill_at(int idx);
int    ui_state_push_skill(const UiSkillEntry* entry);
/* Removes every skill entry of a trigger item. */
void   ui_state_drop_skills(const char* trigger_item_id);

#endif /* CYBERIA_UI_STATE_H */
