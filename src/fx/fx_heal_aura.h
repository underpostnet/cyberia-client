// Heal aura: a ring of green sparks turns around the entity's feet while
// sparks and pixel plus marks rise through its body. It follows the entity,
// so a walking player keeps its aura.
//
// Integration pattern:
//   observe: fx_heal_aura_observe(behavior, id)  // item_consume event
//   update:  fx_heal_aura_update(delta_time);
//   draw:    fx_heal_aura_draw();   // inside BeginMode2D, above the entities

#ifndef CYBERIA_FX_FX_HEAL_AURA_H
#define CYBERIA_FX_FX_HEAL_AURA_H

#include "world/world_types.h"

#include <stdbool.h>

/* Spawns one aura on entity `id` when `behavior` is the loot behavior that heals.
 * Any other behavior, or no entity in view, spawns nothing. */
bool fx_heal_aura_observe(const char* behavior, EntityID id);
void fx_heal_aura_update(float dt);
void fx_heal_aura_draw(void);
void fx_heal_aura_reset(void);

#endif /* CYBERIA_FX_FX_HEAL_AURA_H */
