#include "skill_preview.h"
#include "ol_as_animated_ico.h"
#include "text.h"
#include "ui_button.h"
#include "ui_state.h"

#include "sim/local_player.h"
#include "view/viewport.h"
#include "world/object_layer.h"
#include "world/object_layers_management.h"

#include <assert.h>
#include <raylib.h>
#include <stdbool.h>
#include <stdio.h>
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

/* Pager state: arrows exist only while `s_total > 1`. */
static int       s_page  = 0;
static int       s_total = 0;
static Rectangle s_prev_rect;
static Rectangle s_next_rect;

void skill_preview_pager_reset(void) {
    s_page = 0;
    s_total = 0;
}

/* Layout:
 *   [separator]
 *   SkillName (1/2)                              < >
 *   [sprite] summonedItemId
 *            word-wrapped description... */
float skill_preview_pager_draw(ObjectLayersManager* mgr, const char* item_id, float x, float y, float w, int font) {
    s_total = 0;
    if (!item_id || item_id[0] == '\0') return y;

    int match_indices[UI_STATE_MAX_SKILL_ENTRIES];
    int match_count = 0;
    int total = ui_state_skill_count();
    for (int si = 0; si < total; si++) {
        const UiSkillEntry* se = ui_state_skill_at(si);
        if (!se || strcmp(se->trigger_item_id, item_id) != 0) continue;
        if (se->summoned_entity_item_id[0] == '\0') continue;
        if (match_count < UI_STATE_MAX_SKILL_ENTRIES)
            match_indices[match_count++] = si;
    }
    s_total = match_count;
    if (0 == match_count) return y;

    if (s_page >= match_count) s_page = match_count - 1;
    if (s_page < 0) s_page = 0;

    const UiSkillEntry* se = ui_state_skill_at(match_indices[s_page]);
    const char* summon_id = skill_preview_summon_id(se);
    const char* summon_label = summon_id ? summon_id : se->summoned_entity_item_id;
    bool compact = viewport_is_mobile();
    float right = x + w;

    DrawLine((int)x, (int)y, (int)right, (int)y, (Color){ 70, 70, 100, 140 });
    y += 6;

    /* Header row: skill name (left) + pagination arrows (right). */
    const char* skill_label = (se->name[0] != '\0') ? se->name : se->logic_event_id;
    char header_buf[96];
    if (match_count > 1)
        snprintf(header_buf, sizeof(header_buf), "%s (%d/%d)", skill_label, s_page + 1, match_count);
    else
        snprintf(header_buf, sizeof(header_buf), "%s", skill_label);
    DrawText(header_buf, (int)x, (int)y, font, (Color){ 180, 160, 255, 220 });

    if (match_count > 1) {
        float arrow_w = compact ? 24.0f : 28.0f;
        float arrow_h = compact ? 20.0f : 22.0f;
        float next_x = right - arrow_w;
        float prev_x = next_x - arrow_w - 4;
        s_prev_rect = (Rectangle){ prev_x, y, arrow_w, arrow_h };
        s_next_rect = (Rectangle){ next_x, y, arrow_w, arrow_h };

        int mx = GetMouseX(), my = GetMouseY();
        UIButtonStyle arrow = {
            .font_size     = font,
            .bg            = { 40, 42, 68, 220 },
            .bg_hover      = { 60, 60, 100, 240 },
            .bg_disabled   = { 22, 22, 30, 180 },
            .border        = { 60, 65, 90, 160 },
            .text_color    = { 200, 210, 230, 240 },
            .text_disabled = { 60, 65, 75, 140 },
        };
        arrow.text = "<";
        ui_button_draw(s_prev_rect, &arrow,
            ui_button_resolve_state(s_page > 0, false, ui_button_hit(s_prev_rect, mx, my)));
        arrow.text = ">";
        ui_button_draw(s_next_rect, &arrow,
            ui_button_resolve_state(s_page < match_count - 1, false, ui_button_hit(s_next_rect, mx, my)));
    }
    y += font + 6;

    /* Content row: [sprite] + summoned item name + description. */
    int ico_sz = compact ? 30 : 36;
    if (mgr && summon_id) {
        ol_as_ico_draw(mgr, summon_id, (int)x, (int)y, ico_sz, "down_idle", 0, WHITE);
    }
    DrawRectangleLinesEx((Rectangle){ x, y, (float)ico_sz, (float)ico_sz }, 1.0f, (Color){ 80, 80, 130, 160 });

    float text_x = x + ico_sz + 8;
    DrawText(summon_label, (int)text_x, (int)y, font, (Color){ 200, 220, 255, 240 });

    float bottom = y + ico_sz;
    if (se->description[0] != '\0') {
        float desc_y = y + font + 3;
        desc_y += (float)text_wrap(se->description, (int)text_x, (int)desc_y, (int)(right - text_x),
                                   font - 1, (Color){ 160, 165, 180, 200 }, false, true);
        if (desc_y > bottom) bottom = desc_y;
    }
    return bottom + 6;
}

bool skill_preview_pager_click(int mx, int my) {
    if (s_total <= 1) return false;
    if (s_page > 0 && ui_button_hit(s_prev_rect, mx, my)) {
        s_page--;
        return true;
    }
    if (s_page < s_total - 1 && ui_button_hit(s_next_rect, mx, my)) {
        s_page++;
        return true;
    }
    return false;
}
