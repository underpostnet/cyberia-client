#ifndef ENTITY_RENDER_H
#define ENTITY_RENDER_H

#include "object_layers_management.h"
#include "object_layer.h"
#include <stdbool.h>

/* Entity renderer — animates and draws the object-layer stack of one entity.
 *
 * Holds one animation state per (entity_id, item_id) pair: current frame,
 * frame timing, and the last facing direction used for the idle fallback.
 * The state persists across frames and resets when the direction/mode string
 * changes. Layers draw in item-type priority order (floor first, coin last).
 * Not thread-safe — call from the render thread only. */

typedef struct EntityRender EntityRender;

/* The manager pointer is borrowed: free it separately, and keep it valid for
 * the lifetime of the EntityRender. NULL on allocation failure. */
EntityRender* create_entity_render(ObjectLayersManager* object_layers_manager);

/* Free the animation states and the structure. Does not touch the manager. */
void destroy_entity_render(EntityRender* render);

/* Evict animation states for entities that have left the AOI (not drawn
 * recently). Call periodically from the render loop to bound memory. */
void entity_render_gc(EntityRender* render);

/* Synchronously evict all animation states for a single entity (all its
 * item layers). Call when an entity is removed from the world snapshot. */
void entity_render_forget_entity(EntityRender* render, const char* entity_id);

/* Draw one entity's active layers. Position and size are grid units, scaled
 * internally by `cell_size`. `layers_state` may be NULL when layers_count is
 * 0; invalid inputs are skipped without an error. `entity_type` ("self",
 * "other", "bot", "floor") and `fallback_color` only tint the dev_ui boxes.
 * With `dev_ui` true the function draws the debug box and skips the layers;
 * with it false it draws the layers only. */
void draw_entity_layers(
    EntityRender* render,
    const char* entity_id,
    float pos_x,
    float pos_y,
    float width,
    float height,
    Direction direction,
    ObjectLayerMode mode,
    ObjectLayerState** layers_state,
    int layers_count,
    const char* entity_type,
    bool dev_ui,
    float cell_size,
    Color fallback_color
);

/* Draws a flat, squashed dark ellipse under an entity's feet — a ground
 * shadow shared by every living entity (players, other players, bots,
 * resources). `pos_x`/`pos_y`/`width`/`height` are the same grid-unit
 * footprint passed to draw_entity_layers; scaled internally by `cell_size`.
 * Stateless. Call once per frame, immediately before drawing the entity's
 * sprite layers, so the shadow sits beneath it. Not used for obstacles,
 * statics, or non-combat bots (skill/coin/drop projectiles). */
void draw_entity_shadow(float pos_x, float pos_y, float width, float height, float cell_size);

#endif // ENTITY_RENDER_H
