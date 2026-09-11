// Level-up celebration: an aura of gold sparks streams from the entity's feet
// up past its head while a star spins over it. It follows the entity, so a
// walking player keeps its aura.
//
// Integration pattern:
//   observe: fx_level_up_observe(previous, current, entity)  // snapshot decode
//   update:  fx_level_up_update(delta_time);
//   draw:    fx_level_up_draw();   // inside BeginMode2D, above the entities

#ifndef CYBERIA_FX_FX_LEVEL_UP_H
#define CYBERIA_FX_FX_LEVEL_UP_H

#include "world_types.h"

#include <stdbool.h>

/* Spawns one effect on `entity` when `current` exceeds a known `previous`
 * level. A first sight (previous 0), a repeat, or a lower level spawns nothing. */
bool fx_level_up_observe(int previous, int current, const EntityState* entity);
void fx_level_up_update(float dt);
void fx_level_up_draw(void);
void fx_level_up_reset(void);
/* Whether the entity's aura is running; the overhead HUD yields the space. */
bool fx_level_up_active(const char* entity_id);

#endif /* CYBERIA_FX_FX_LEVEL_UP_H */
