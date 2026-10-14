#include "skill_preview.h"
#include "ui_state.h"

#include "sim/local_player.h"
#include "world/object_layer.h"
#include "world/object_layers_management.h"

#include <assert.h>
#include <raylib.h>
#include <stdbool.h>
#include <string.h>

#define SKILL_PREVIEW_CASTER_SKIN "$active_skin"
#define SKILL_PREVIEW_SIDE  0.7f  /* largest preview side per slot side */
#define SKILL_PREVIEW_GAP   2.0f
#define SKILL_PREVIEW_BAR_H 4.0f

const char* skill_preview_summon_id(const UiSkillEntry* skill) {
    assert(skill);
    const char* id = skill->summoned_entity_item_id;
    if (0 == strcmp(SKILL_PREVIEW_CASTER_SKIN, id)) {
        return active_layer_item_id(g_local_player.inventory, g_local_player.inventory_count, "skin");
    }
    /* A leading '$' marks a placeholder that only the server resolves. */
    if ('\0' == id[0] || '$' == id[0]) return NULL;
    return id;
}

int skill_preview_summons(const ObjectLayerState* slot, SkillPreview out[SKILL_PREVIEW_MAX], bool* held) {
    assert(slot && out && held);
    *held = false;
    int n = 0;
    for (int i = 0; ui_state_skill_count() > i && SKILL_PREVIEW_MAX > n; i++) {
        const UiSkillEntry* skill = ui_state_skill_at(i);
        if (0 != strcmp(skill->trigger_item_id, slot->item_id)) continue;
        if (!slot->active && !skill->held) continue;
        const char* id = skill_preview_summon_id(skill);
        if (NULL == id) continue;
        bool seen = false;
        for (int j = 0; n > j; j++) seen = seen || 0 == strcmp(out[j].item_id, id);
        if (seen) continue;
        /* A stack that summons its own item counts its own units: a held
         * consumable spends them one stack at a time. */
        int quantity = 0 == strcmp(id, slot->item_id) ? slot->quantity : local_player_item_quantity(id);
        out[n++] = (SkillPreview){ .item_id = id, .quantity = quantity };
        *held = *held || skill->held;
    }
    return n;
}

void skill_preview_layout(Rectangle slot, float floor_y, Rectangle keep_clear, int count,
                          Rectangle icons[SKILL_PREVIEW_MAX], Rectangle* bar) {
    assert(0 < count && SKILL_PREVIEW_MAX >= count && icons && bar);
    float side = (slot.width - (float)(count - 1) * SKILL_PREVIEW_GAP) / (float)count;
    if (side > slot.width * SKILL_PREVIEW_SIDE) side = slot.width * SKILL_PREVIEW_SIDE;
    float row_w = (float)count * side + (float)(count - 1) * SKILL_PREVIEW_GAP;
    float x = slot.x + (slot.width - row_w) * 0.5f;
    if (x < keep_clear.x + keep_clear.width && keep_clear.x < x + row_w && keep_clear.y < floor_y) {
        floor_y = keep_clear.y;
    }
    *bar = (Rectangle){ x, floor_y - SKILL_PREVIEW_GAP - SKILL_PREVIEW_BAR_H, row_w, SKILL_PREVIEW_BAR_H };
    float top = bar->y - SKILL_PREVIEW_GAP - side;
    for (int i = 0; count > i; i++) {
        icons[i] = (Rectangle){ x + (float)i * (side + SKILL_PREVIEW_GAP), top, side, side };
    }
}
