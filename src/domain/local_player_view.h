#ifndef LOCAL_PLAYER_VIEW_H
#define LOCAL_PLAYER_VIEW_H

#include <raylib.h>

/* Presentation-only visual smoothing AND identity marking for the LOCAL
 * player — the entity rendered at screen centre. It absorbs the
 * authoritative position corrections produced by client prediction + server
 * reconciliation as a smooth, frame-rate independent slide instead of an
 * instant snap, and provides the local-player-exclusive render-scale bump
 * that lets the player pick themself out of a crowd.
 *
 * This layer is purely visual: it is never read by prediction, reconciliation,
 * collision, or any gameplay/simulation code — only by the renderer/camera. */

/* Advance the smoothed visual position toward the authoritative `target` over
 * `dt` seconds and return it. The first call and any jump beyond the snap
 * threshold (teleports / respawns) snap instead of sliding. */
Vector2 local_player_view_update(Vector2 target, float dt);

/* Local player's draw footprint (grid units) scaled up so the character
 * reads slightly larger than other entities. Anchored at the same feet
 * (bottom-centre) point as the unscaled footprint so growth never shifts
 * the character's ground contact or facing offset. */
Rectangle local_player_view_scaled_footprint(float pos_x, float pos_y, float width, float height);

#endif /* LOCAL_PLAYER_VIEW_H */
