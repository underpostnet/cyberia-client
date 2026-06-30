#ifndef LOCAL_PLAYER_VIEW_H
#define LOCAL_PLAYER_VIEW_H

#include <raylib.h>

/* Presentation-only visual smoothing for the LOCAL player — the entity rendered
 * at screen centre. It absorbs the authoritative position corrections produced
 * by client prediction + server reconciliation as a smooth, frame-rate
 * independent slide instead of an instant snap.
 *
 * This layer is purely visual: it is never read by prediction, reconciliation,
 * collision, or any gameplay/simulation code — only by the renderer/camera. */

/* Advance the smoothed visual position toward the authoritative `target` over
 * `dt` seconds and return it. The first call and any jump beyond the snap
 * threshold (teleports / respawns) snap instead of sliding. */
Vector2 local_player_view_update(Vector2 target, float dt);

#endif /* LOCAL_PLAYER_VIEW_H */
