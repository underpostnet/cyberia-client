#ifndef CYBERIA_SIM_AUTO_TARGET_H
#define CYBERIA_SIM_AUTO_TARGET_H

#include <raylib.h>

#include "world/object_layer.h"
#include "world/world_types.h"

/* The prediction of the server autoTarget from what the client holds. A
 * candidate is a living hostile bot within reach. The preferred target needs
 * only to live and stay in reach: the server named it. The server also takes
 * the enemies engaged with the player, which the client does not hold, so
 * skill_ack names the target the server took. */

/* Cosine of the widest angle off the facing that counts as ahead (67.5°), as the server facingCone. */
#define AUTO_TARGET_FACING_CONE 0.38268343f

/* The target an auto aim from `from` (cells) takes: the preferred target while
 * it stays one, else the candidate ahead of `facing`, then the nearest, then
 * the lowest id. 0 for none. */
EntityID auto_target_predict(Vector2 from, Direction facing, float reach, EntityID preferred);

/* The hostile bot under a world point (cells), or 0. */
EntityID auto_target_at(Vector2 at);

/* The centre of an entity as the view draws it, in cells. */
Vector2 auto_target_center(EntityID id);

#endif /* CYBERIA_SIM_AUTO_TARGET_H */
