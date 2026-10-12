#ifndef CYBERIA_UI_SKILL_PREVIEW_H
#define CYBERIA_UI_SKILL_PREVIEW_H

#include <raylib.h>

#include "object_layer.h"
#include "ui_state.h"

/* Summoned entity previews: the entities the configured skills of an item
 * summon, from the init_data skill map, and their place above a slot. */

/* Distinct summons drawn above one inventory slot. */
#define SKILL_PREVIEW_MAX 3

/* Item id of the entity `skill` summons; "$active_skin" gives the local
 * player's active skin. NULL when no entity resolves. */
const char* skill_preview_summon_id(const UiSkillEntry* skill);

/* Fill `out` with the distinct entities the skills of `slot` summon, in skill
 * map order. An inactive slot fires no skill and gives 0. */
int skill_preview_summons(const ObjectLayerState* slot, const char* out[SKILL_PREVIEW_MAX]);

/* One row of `count` previews centered over `slot`, with the cooldown bar
 * under the row. Both stay above `floor_y`, and above `keep_clear` where the
 * row crosses it. */
void skill_preview_layout(Rectangle slot, float floor_y, Rectangle keep_clear, int count,
                          Rectangle icons[SKILL_PREVIEW_MAX], Rectangle* bar);

#endif /* CYBERIA_UI_SKILL_PREVIEW_H */
