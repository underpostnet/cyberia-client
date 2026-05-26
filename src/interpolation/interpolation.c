#include "interpolation.h"
#include "game_state.h"

#include <raylib.h>

/* Render-time interpolation of remote entities (Gambetta Part III).
 *
 * Each entity carries `pos_prev`, `pos_server`, and `snapshot_time` —
 * the wall-clock instant the snapshot that produced pos_server arrived.
 * Interpolating per-entity instead of per-snapshot tolerates batched or
 * jittered AOI delivery without leaving stale positions in place.
 *
 *   t = (now - entity.snapshot_time) * 1000 / interpolation_ms
 *   clamped to [0, 1]
 *
 * For entities that were teleported (mode == TELEPORTING in the decoder)
 * pos_prev was already snapped to the new server position, so the lerp
 * produces an immediate jump.
 */

static inline float compute_alpha_for(double now, double snapshot_time, int window_ms) {
    if (window_ms <= 0) return 1.0f;
    double t = (now - snapshot_time) * 1000.0 / (double)window_ms;
    if (t < 0.0) t = 0.0;
    if (t > 1.0) t = 1.0;
    return (float)t;
}

void interpolation_compute_view(void) {
    const double now = GetTime();
    const int window_ms = g_game_state.interpolation_ms;

    for (int i = 0; i < g_game_state.other_player_count; i++) {
        PlayerState* p = &g_game_state.other_players[i];
        float t = compute_alpha_for(now, p->base.snapshot_time, window_ms);
        p->base.interp_pos.x = p->base.pos_prev.x + (p->base.pos_server.x - p->base.pos_prev.x) * t;
        p->base.interp_pos.y = p->base.pos_prev.y + (p->base.pos_server.y - p->base.pos_prev.y) * t;
    }
    for (int i = 0; i < g_game_state.bot_count; i++) {
        BotState* b = &g_game_state.bots[i];
        float t = compute_alpha_for(now, b->base.snapshot_time, window_ms);
        b->base.interp_pos.x = b->base.pos_prev.x + (b->base.pos_server.x - b->base.pos_prev.x) * t;
        b->base.interp_pos.y = b->base.pos_prev.y + (b->base.pos_server.y - b->base.pos_prev.y) * t;
    }
}
