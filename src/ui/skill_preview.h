#ifndef CYBERIA_UI_SKILL_PREVIEW_H
#define CYBERIA_UI_SKILL_PREVIEW_H

#include <raylib.h>
#include <stdbool.h>

#include "world/object_layer.h"
#include "world/object_layers_management.h"
#include "ui_state.h"

/* Summoned entity previews: the entities the configured skills of an item
 * summon, from the init_data skill map, and their place above a slot. */

/* Distinct summons drawn above one inventory slot. */
#define SKILL_PREVIEW_MAX 3

/* Item id of the entity `skill` summons; "$active_skin" gives the local
 * player's active skin. NULL when no entity resolves. */
const char* skill_preview_summon_id(const UiSkillEntry* skill);

/* One summoned entity, and the units of it the local player holds: those of
 * the slot itself when the slot summons its own item. */
typedef struct {
    const char* item_id;
    int         quantity;
} SkillPreview;

/* Fill `out` with the distinct entities the skills of `slot` summon, in skill
 * map order. An inactive slot shows only its held skills. `held` reports a
 * held skill in the row: the consume cooldown then times it. */
int skill_preview_summons(const ObjectLayerState* slot, SkillPreview out[SKILL_PREVIEW_MAX], bool* held);

/* One row of `count` previews centered over `slot`, with the cooldown bar
 * under the row. Both stay above `floor_y`, and above `keep_clear` where the
 * row crosses it. */
void skill_preview_layout(Rectangle slot, float floor_y, Rectangle keep_clear, int count,
                          Rectangle icons[SKILL_PREVIEW_MAX], Rectangle* bar);

/* Skill pager: the skills of one item that summon an entity, one per page,
 * with < > arrows when more than one matches. */
void  skill_preview_pager_reset(void);

/* Draw the current page of `item_id`'s skills at (x, y), `w` wide. Returns the
 * y below the page; `y` when no skill matches. */
float skill_preview_pager_draw(ObjectLayersManager* mgr, const char* item_id, float x, float y, float w, int font);

/* Turn the page on an arrow hit. True when the click hit an enabled arrow. */
bool  skill_preview_pager_click(int mx, int my);

#endif /* CYBERIA_UI_SKILL_PREVIEW_H */
