// Defeat cue: red sparks burst out of the entity and fall while a skull rises
// over it. One shot at the moment of death; it follows the body while it runs.
//
// Integration pattern:
//   observe: fx_death_observe(prior_alive, dead, entity)  // snapshot decode
//   update:  fx_death_update(delta_time);
//   draw:    fx_death_draw();   // inside BeginMode2D, above the entities

#ifndef CYBERIA_FX_FX_DEATH_H
#define CYBERIA_FX_FX_DEATH_H

#include "world_types.h"

#include <stdbool.h>

/* Spawns one effect on `entity` when it was alive in the prior snapshot and
 * is dead now. An entity first seen dead, or still dead, spawns nothing. */
bool fx_death_observe(bool prior_alive, bool dead, const EntityState* entity);
void fx_death_update(float dt);
void fx_death_draw(void);
void fx_death_reset(void);
/* Whether the entity's cue is running; the overhead HUD yields the space. */
bool fx_death_active(const char* entity_id);

#endif /* CYBERIA_FX_FX_DEATH_H */
