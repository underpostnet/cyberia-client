/**
 * modal_interact — top-half entity interaction modal.
 *
 * A tab strip over a fixed bottom bar of integration buttons. The tabs follow
 * who the modal shows:
 *
 *   NPC:     [shop] [assembly] [storage] [quest] — the capabilities it offers
 *            [stack] active item slots, [stats] six-stat stack totals
 *   Own:     [profile] [storage] personal storage, [party], [friends], [stats]
 *   Player:  [profile] public profile and party actions, [stats], [stack]
 *   Bottom:  [ Chat ]        another player: opens its direct conversation in
 *                            the chat panel (ui/chat_panel.h)
 *            [ Integration ] stack composite + viewer links, dev UI only
 *
 * The paired modal_dialogue (bottom half) carries the talk flow; server
 * validation drives quest grant/advance. Dismiss closes both.
 */

#ifndef MODAL_INTERACT_H
#define MODAL_INTERACT_H

#include "player_panel.h"
#include "world/object_layer.h"
#include "world/world_types.h"
#include <raylib.h>
#include <stdbool.h>
#include <stdint.h>

void modal_interact_init(void);

/* dialogue_item_id is the entity's active skin; has_dialogue is true when that
 * skin has a default dialogue. border tints the header to match the entity's
 * status indicator. The own player opens its profile; another player its
 * public profile. */
void modal_interact_open(EntityID entity_id, const char* display_name,
                         const char* dialogue_item_id, bool has_dialogue,
                         Color border);

/* The own profile, on one of its player views. */
void modal_interact_open_own(PlayerView view);

void modal_interact_close(void);
bool modal_interact_is_open(void);
float modal_interact_layout_bottom(void);

/* Quest-talk switcher for the paired dialogue: one entry per active quest with
 * an incomplete talk objective this NPC's action maps to a dialogue. The
 * dialogue draws a button bar above its title; selecting an index swaps the
 * shown dialogue, -1 restores the default greeting. */
int  modal_interact_quest_talk_count(void);
const char* modal_interact_quest_talk_label(int index);
int  modal_interact_quest_talk_selected(void);
void modal_interact_set_quest_talk(int index);
void modal_interact_update(float dt);
void modal_interact_draw(void);
bool modal_interact_handle_click(int mx, int my);
bool modal_interact_handle_wheel(float wheel_delta);

/* Inventory-bar slot tap while this modal is open: stack the player-item
 * inventory modal on top of the session; closing it returns here. */
void modal_interact_stack_player_item(int inv_idx);

/* Discard any pending interact session on the ephemeral stack (so it is not
 * reopened). Called when the inventory modal switches straight to a bar slot. */
void modal_interact_discard_stack(void);

/* Returns the cached alive layer snapshot (persists across AOI changes). */
const ObjectLayerState* modal_interact_get_cached_layers(int* out_count);

#endif /* MODAL_INTERACT_H */
