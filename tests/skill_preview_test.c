#include "ui/skill_preview.h"
#include "ui/ui_state.h"

#include "domain/local_player.h"
#include "network/game/client_event.h"
#include "object_layer.h"
#include "util/utils.h"

#include <assert.h>
#include <raylib.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

/* The skin the local player wears, as the ObjectLayer cache would type it. */
static const char* s_skin;

const char* active_layer_item_id(const ObjectLayerState* layers, int count, const char* type) {
    assert(layers == g_local_player.inventory && 0 == strcmp("skin", type));
    return s_skin;
}
client_event_t client_event_push(client_event_kind_t kind, client_event_payload_t p) { return (client_event_t){ 0 }; }
void vault_ops_clear(void) {}

static void push_skill_held(const char* trigger, const char* logic, const char* summon, bool held) {
    UiSkillEntry skill = { .held = held };
    copy_str(skill.trigger_item_id, sizeof skill.trigger_item_id, trigger);
    copy_str(skill.logic_event_id, sizeof skill.logic_event_id, logic);
    copy_str(skill.summoned_entity_item_id, sizeof skill.summoned_entity_item_id, summon);
    assert(0 <= ui_state_push_skill(&skill));
}

static void push_skill(const char* trigger, const char* logic, const char* summon) {
    push_skill_held(trigger, logic, summon, false);
}

static ObjectLayerState slot(const char* item_id, bool active) {
    ObjectLayerState s = { .active = active, .quantity = 1 };
    copy_str(s.item_id, sizeof s.item_id, item_id);
    return s;
}

static bool near(float a, float b) { return a - b < 0.01f && b - a < 0.01f; }

int main(void) {
    SkillPreview summons[SKILL_PREVIEW_MAX];
    bool held;

    /* The reference skill: the pistol slot previews its bullet, not itself. */
    push_skill("atlas_pistol_mk2", "projectile", "atlas_pistol_mk2_bullet");
    push_skill("coin", "coin_drop_or_transaction", "coin");
    ObjectLayerState pistol = slot("atlas_pistol_mk2", true);
    assert(1 == skill_preview_summons(&pistol, summons, &held) && !held);
    assert(0 == strcmp("atlas_pistol_mk2_bullet", summons[0].item_id) && 0 == summons[0].quantity);

    /* Only an active trigger fires a tap skill; other items preview nothing. */
    pistol.active = false;
    assert(0 == skill_preview_summons(&pistol, summons, &held));
    ObjectLayerState coin = slot("coin", false);
    assert(0 == skill_preview_summons(&coin, summons, &held));
    ObjectLayerState sword = slot("sword", true);
    assert(0 == skill_preview_summons(&sword, summons, &held));

    /* A held skill previews an inactive stack: the item it consumes, with the units left. */
    push_skill_held("med-gel", "zero-waste-auto-consume-heal", "med-gel", true);
    g_local_player.inventory[0] = slot("med-gel", false);
    g_local_player.inventory[0].quantity = 3;
    g_local_player.inventory_count = 1;
    assert(1 == skill_preview_summons(&g_local_player.inventory[0], summons, &held) && held);
    assert(0 == strcmp("med-gel", summons[0].item_id) && 3 == summons[0].quantity);
    g_local_player.inventory_count = 0;

    /* One preview per distinct entity, in skill map order, up to the row size. */
    ui_state_reset();
    push_skill("staff", "projectile", "orb");
    push_skill("staff", "projectile", "orb");
    push_skill("staff", "doppelganger", "$active_skin");
    push_skill("staff", "projectile", "");
    push_skill("staff", "projectile", "$unknown");
    ObjectLayerState staff = slot("staff", true);
    s_skin = "skin-a";
    assert(2 == skill_preview_summons(&staff, summons, &held));
    assert(0 == strcmp("orb", summons[0].item_id) && 0 == strcmp("skin-a", summons[1].item_id));
    s_skin = NULL;
    assert(1 == skill_preview_summons(&staff, summons, &held));
    push_skill("staff", "projectile", "spark");
    push_skill("staff", "projectile", "ember");
    push_skill("staff", "projectile", "flare");
    assert(SKILL_PREVIEW_MAX == skill_preview_summons(&staff, summons, &held));
    assert(0 == strcmp("ember", summons[2].item_id));

    /* A reconnect clears the skill map, and with it every preview. */
    ui_state_reset();
    assert(0 == skill_preview_summons(&staff, summons, &held));

    /* The row sits above the bar top, centered on its slot, never on the slot. */
    Rectangle icons[SKILL_PREVIEW_MAX];
    Rectangle bar;
    Rectangle r = { 100.0f, 506.0f, 60.0f, 60.0f };
    Rectangle toggle = { 6.0f, 462.0f, 32.0f, 32.0f };
    skill_preview_layout(r, 500.0f, toggle, 1, icons, &bar);
    assert(near(42.0f, icons[0].width) && near(icons[0].width, icons[0].height));
    assert(near(r.x + r.width * 0.5f, icons[0].x + icons[0].width * 0.5f));
    assert(icons[0].y + icons[0].height <= bar.y && bar.y + bar.height <= 500.0f);
    assert(near(icons[0].x, bar.x) && near(icons[0].width, bar.width));

    /* The row follows its slot when the strip scrolls or reorders. */
    Rectangle moved = r;
    moved.x += 66.0f;
    Rectangle moved_icons[SKILL_PREVIEW_MAX];
    Rectangle moved_bar;
    skill_preview_layout(moved, 500.0f, toggle, 1, moved_icons, &moved_bar);
    assert(near(icons[0].x + 66.0f, moved_icons[0].x) && near(bar.x + 66.0f, moved_bar.x));

    /* Three previews share the slot width; one bar spans them. */
    skill_preview_layout(r, 500.0f, toggle, 3, icons, &bar);
    assert(icons[0].x >= r.x && icons[2].x + icons[2].width <= r.x + r.width + 0.01f);
    assert(icons[0].x + icons[0].width < icons[1].x);
    assert(near(icons[0].x, bar.x) && near(icons[2].x + icons[2].width, bar.x + bar.width));

    /* A smaller mobile slot gives a smaller preview. */
    skill_preview_layout((Rectangle){ 100.0f, 505.0f, 50.0f, 50.0f }, 500.0f, toggle, 1, icons, &bar);
    assert(near(35.0f, icons[0].width));

    /* Over the first slot the row rises above the bar toggle. */
    skill_preview_layout((Rectangle){ 6.0f, 506.0f, 60.0f, 60.0f }, 500.0f, toggle, 1, icons, &bar);
    assert(bar.y + bar.height <= toggle.y && near(15.0f, icons[0].x));

    /* The hidden bar keeps previews above the screen edge at the same X. */
    Rectangle hidden_slot = { 100.0f, 578.0f, 60.0f, 60.0f };
    Rectangle hidden_toggle = { 6.0f, 534.0f, 32.0f, 32.0f };
    skill_preview_layout(r, 500.0f, toggle, 1, icons, &bar);
    skill_preview_layout(hidden_slot, 572.0f, hidden_toggle, 1, moved_icons, &moved_bar);
    assert(near(icons[0].x, moved_icons[0].x));
    assert(near(icons[0].y + 72.0f, moved_icons[0].y));
    assert(near(bar.y + 72.0f, moved_bar.y));
    assert(moved_icons[0].y + moved_icons[0].height <= moved_bar.y);
    assert(moved_bar.y + moved_bar.height <= 572.0f);

    /* A preview over the toggle stays clear when the bar is hidden. */
    skill_preview_layout((Rectangle){ 6.0f, 578.0f, 60.0f, 60.0f },
                         572.0f, hidden_toggle, 1, icons, &bar);
    assert(bar.y + bar.height <= hidden_toggle.y);

    /* The cooldown bar reads the one authoritative ready time. */
    local_player_reset();
    assert(1.0f == local_player_action_charge(10.0));
    local_player_set_action_cooldown(0.5f);
    local_player_set_action_ready_at(10.5);
    assert(near(0.0f, local_player_action_charge(10.0)));
    assert(near(0.5f, local_player_action_charge(10.25)));
    assert(1.0f == local_player_action_charge(10.5));
    assert(1.0f == local_player_action_charge(60.0));
    /* A ready time past the cooldown (a slower Utility) shows empty, never negative. */
    local_player_set_action_ready_at(12.0);
    assert(0.0f == local_player_action_charge(10.0));

    /* The consume cooldown has its own ready time, on the same action cooldown. */
    local_player_set_action_cooldown(0.5f);
    local_player_set_consume_ready_at(10.5);
    assert(near(0.5f, local_player_consume_charge(10.25)) && 0.0f == local_player_action_charge(10.0));

    /* A reset never leaves a bar blocked. */
    local_player_reset();
    assert(1.0f == local_player_action_charge(0.0) && 1.0f == local_player_consume_charge(0.0));

    puts("skill_preview_test OK");
    return 0;
}
