/**
 * modal_interact — top-half entity interaction modal.
 *
 * A tab strip over a fixed bottom bar of integration buttons:
 *
 *   Tabs:   [stack]  active item slots
 *           [stats]  six-stat stack totals
 *           [action] mission interface — only for action-provider entities
 *   Bottom: [ Chat ]        opens the JS overlay on the Chat tab
 *           [ Integration ] opens the JS overlay on the Integration tab
 *
 * The action tab's "Talk" opens the paired modal_dialogue (bottom half);
 * server validation drives quest grant/advance. Dismiss closes both.
 */

#ifndef MODAL_INTERACT_H
#define MODAL_INTERACT_H

#include "object_layer.h"
#include <raylib.h>
#include <stdbool.h>

void modal_interact_init(void);

/* dialogue_item_id is the entity's active skin; has_dialogue is true when
 * that skin has a default dialogue. is_action_provider gates the action tab
 * (true for entities bound to a CyberiaAction). border tints the header to
 * match the entity's status indicator. */
void modal_interact_open(const char* entity_id, const char* display_name,
                         const char* dialogue_item_id, bool has_dialogue,
                         bool is_action_provider, Color border);

void modal_interact_close(void);
bool modal_interact_is_open(void);
void modal_interact_update(float dt);
void modal_interact_draw(void);
bool modal_interact_handle_click(int mx, int my);

/* Called when the JS overlay closes — reopens this modal. */
void modal_interact_overlay_closed(void);

/* Returns the cached alive layer snapshot (persists across AOI changes). */
const ObjectLayerState* modal_interact_get_cached_layers(int* out_count);

#endif /* MODAL_INTERACT_H */
