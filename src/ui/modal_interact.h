/**
 * modal_interact — top-half entity interaction modal.
 *
 * A tab strip over a fixed bottom bar of integration buttons:
 *
 *   Tabs:   [quest]  mission interface — only for quest-provider entities
 *           [shop]   vendor catalog     — only for entities selling items
 *           [stack]  active item slots
 *           [stats]  six-stat stack totals
 *   Bottom: [ Chat ]        opens the JS overlay on the Chat tab
 *           [ Integration ] opens the JS overlay on the Integration tab
 *
 * The paired modal_dialogue (bottom half) carries the talk flow; server
 * validation drives quest grant/advance. Dismiss closes both.
 */

#ifndef MODAL_INTERACT_H
#define MODAL_INTERACT_H

#include "object_layer.h"
#include <raylib.h>
#include <stdbool.h>
#include <stdint.h>

void modal_interact_init(void);

/* dialogue_item_id is the entity's active skin; has_dialogue is true when that
 * skin has a default dialogue. interaction_flags is the server's per-player
 * capability bitmask (INTERACTION_FLAG_*): the action bit gates the Action tab,
 * the quest bit gates the Quest tab. border tints the header to match the
 * entity's status indicator. */
void modal_interact_open(const char* entity_id, const char* display_name,
                         const char* dialogue_item_id, bool has_dialogue,
                         uint8_t interaction_flags, Color border);

void modal_interact_close(void);
bool modal_interact_is_open(void);
float modal_interact_layout_bottom(void);

/* True while the JS interact overlay has taken over this modal's session. The
 * card and its paired dialogue stay out of the frame for the duration —
 * without this they show through the overlay's transparent backdrop. */
bool modal_interact_overlay_is_open(void);

/* Anchored card rect this modal is currently showing for `entity_id`, for a
 * panel taking its place (the DOM overlay). False when the modal is closed,
 * showing a different entity, or using the full-width layout — the caller then
 * resolves its own placement. */
bool modal_interact_card_rect(const char* entity_id, Rectangle* out);

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

/* Called when the JS overlay closes — reopens this modal. */
void modal_interact_overlay_closed(void);

/* Inventory-bar slot tap while this modal is open: stack the player-item
 * inventory modal on top of the session; closing it returns here. */
void modal_interact_stack_player_item(int inv_idx);

/* Discard any pending interact session on the ephemeral stack (so it is not
 * reopened). Called when the inventory modal switches straight to a bar slot. */
void modal_interact_discard_stack(void);

/* Returns the cached alive layer snapshot (persists across AOI changes). */
const ObjectLayerState* modal_interact_get_cached_layers(int* out_count);

/* Authoritative vault contents for the open storage session. `indices` carries
 * each slot's position in the vault, parallel to `slots`. */
void modal_interact_storage_state(const char* entity_id, int capacity,
                                  const ObjectLayerState* slots, const int* indices,
                                  int count);

/* True while the Storage tab is showing and can adopt a drag from the
 * inventory bar. */
bool modal_interact_storage_accepts_drag(void);

/* Draw the vault's dragged item. The frame orchestrator calls this after the
 * inventory bar so the item in hand rides above every surface. */
void modal_interact_draw_storage_drag(void);

/* Lift an inventory-bar stack into the vault grid's drag, giving Inventory Bar
 * → Storage Grid the same gesture as a move inside the grid. */
void modal_interact_storage_drag_in(int inv_idx);

#endif /* MODAL_INTERACT_H */
