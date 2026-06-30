#include "local_player_view.h"

#include <math.h>
#include <raylib.h>

/* ── Tuning ──────────────────────────────────────────────────────────────────
 * DAMP_LAMBDA — exponential convergence rate (1/seconds). Higher snaps faster,
 *               lower floats longer; ~18 gives a ≈55 ms time constant, absorbing
 *               corrections within a few frames while staying smooth.
 * SNAP_CELLS  — a jump larger than this (in world cells) is a teleport/respawn,
 *               not a reconciliation correction, so the view snaps rather than
 *               sliding across the map. */
#define LOCAL_PLAYER_VIEW_DAMP_LAMBDA 18.0f
#define LOCAL_PLAYER_VIEW_SNAP_CELLS 6.0f

static Vector2 s_pos = { 0.0f, 0.0f };
static bool s_initialized = false;

Vector2 local_player_view_update(Vector2 target, float dt) {
    float ex = target.x - s_pos.x;
    float ey = target.y - s_pos.y;

    bool snap = !s_initialized ||
                (ex * ex + ey * ey) > (LOCAL_PLAYER_VIEW_SNAP_CELLS * LOCAL_PLAYER_VIEW_SNAP_CELLS);
    if (snap) {
        s_pos = target;
        s_initialized = true;
        return s_pos;
    }

    /* Frame-rate independent exponential damping: the fraction of remaining
     * error removed scales with dt, so the slide is identical at any FPS. */
    float blend = 1.0f - expf(-LOCAL_PLAYER_VIEW_DAMP_LAMBDA * dt);
    s_pos.x += ex * blend;
    s_pos.y += ey * blend;
    return s_pos;
}
