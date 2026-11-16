#include "ui/modal_interact.c"

#include <assert.h>
#include <stdio.h>
#include <string.h>

/* Stubbed world: NPC 7 binds the action "vendor", whose catalog lands when the
 * test says so; any other entity offers nothing. */
#define NPC 7

static ActionMetadataEntry s_catalog;
static bool        s_catalog_landed;
static Interaction s_offer;
static bool        s_trading;

const ActionMetadataEntry* action_cache_get(const char* code) { return s_catalog_landed ? &s_catalog : NULL; }
const Interaction* local_player_interaction(EntityID id) { return NPC == id ? &s_offer : NULL; }
bool market_panel_visible(const ActionMetadataEntry* action) { return false; }
int merchant_panel_count(const ActionMetadataEntry* action, MerchantMode mode) { return 0; }
bool trade_panel_with(EntityID id) { return s_trading; }

static void npc(int quest_codes, bool actionable, int shop_items) {
    s_mode             = MI_MODE_NPC;
    s_entity_id        = NPC;
    s_quest_code_count = quest_codes;
    s_offer.flags      = actionable ? INTERACTION_FLAG_QUEST : 0;
    s_catalog          = (ActionMetadataEntry){ .shop_count = shop_items };
    s_catalog_landed   = true;
    copy_str(s_action_code, sizeof s_action_code, 0 < shop_items ? "vendor" : "");
}

int main(void) {
    /* An NPC with nothing to offer opens on its stack; one with past quests only, on Quest. */
    npc(0, false, 0);
    assert(MI_TAB_STACK == default_tab() && !action_pending());
    npc(2, false, 0);
    assert(MI_TAB_QUEST == default_tab());

    /* An action tab comes before quest feedback; an actionable quest comes before every action. */
    npc(2, false, 3);
    assert(MI_TAB_SHOP == default_tab());
    npc(2, true, 3);
    assert(MI_TAB_QUEST == default_tab());

    /* Until the catalog lands, the default stays open: the shop shows only after it. */
    npc(0, false, 3);
    s_catalog_landed = false;
    assert(action_pending() && MI_TAB_STACK == default_tab());
    s_catalog_landed = true;
    assert(!action_pending() && MI_TAB_SHOP == default_tab());

    /* A player opens on its profile, or on the trade that runs with it; the quest bit is for NPCs. */
    s_mode = MI_MODE_PLAYER;
    s_entity_id = NPC;
    s_offer.flags = INTERACTION_FLAG_QUEST;
    s_action_code[0] = '\0';
    assert(MI_TAB_PROFILE == default_tab() && !action_pending());
    s_trading = true;
    assert(MI_TAB_TRADE == default_tab());
    s_trading = false;
    s_mode = MI_MODE_SELF;
    assert(MI_TAB_PROFILE == default_tab());

    puts("modal_interact_test OK");
    return 0;
}
