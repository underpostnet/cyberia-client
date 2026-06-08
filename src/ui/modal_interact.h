/**
 * modal_interact — top-half entity interaction modal.
 *
 * Tapping an interaction bubble opens this modal and auto-opens the paired
 * modal_dialogue (bottom half), which renders the entity and, when the skin
 * has a default dialogue, its text. This modal offers two actions:
 *
 *   [ Chat ]         opens the JS overlay directly on the Chat tab.
 *   [ Integration ]  opens the JS overlay directly on the Integration tab.
 *
 * Dismisses (close button or tap outside) close both modals.
 */

#ifndef MODAL_INTERACT_H
#define MODAL_INTERACT_H

#include "object_layer.h"
#include <raylib.h>
#include <stdbool.h>

void modal_interact_init(void);

/* dialogue_item_id is the entity's active skin; has_dialogue is true when
 * that skin has a default dialogue. border tints the header to match the
 * entity's status indicator. */
void modal_interact_open(const char* entity_id, const char* display_name,
                         const char* dialogue_item_id, bool has_dialogue,
                         Color border);

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