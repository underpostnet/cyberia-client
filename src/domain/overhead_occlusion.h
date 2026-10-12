#ifndef CYBERIA_DOMAIN_OVERHEAD_OCCLUSION_H
#define CYBERIA_DOMAIN_OVERHEAD_OCCLUSION_H

#include "world_types.h"

/* Roof fade of `overhead-occlusion` foregrounds. Presentation only.
 *
 * While the centre of the local player is inside a roof, its opacity moves
 * toward the hinted hidden opacity. When the player leaves, it moves back to 1.
 * Each direction takes the hinted fade time, and a reversal starts from the
 * current opacity. */

/* Advance every fade by `dt` seconds. Call once per frame, after the render
 * position of the local player is set. */
void  overhead_occlusion_update(float dt);

/* Draw opacity of foreground `id`, in [hidden opacity, 1]. Only a roof with
 * ENTITY_FLAG_OVERHEAD_OCCLUSION ever fades. */
float overhead_occlusion_opacity(EntityID id);

#endif /* CYBERIA_DOMAIN_OVERHEAD_OCCLUSION_H */
