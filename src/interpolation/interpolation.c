#include "interpolation.h"
#include "game_state.h"

#include <raylib.h>

/* Render-time interpolation of remote entities only.
 *
 * The decoder updates entity.pos_prev / entity.pos_server on each snapshot.
 * We lerp from prev → server using (now - last_update) / interpolation_ms.
 * The local player is owned by prediction/ and is not touched here.
 */
void interpolation_compute_view(void) {
    double now = GetTime();

    float t = 1.0f;
    if (g_game_state.interpolation_ms > 0) {
        t = (float)((now - g_game_state.last_update_time) * 1000.0
                    / (double)g_game_state.interpolation_ms);
        if (t > 1.0f) t = 1.0f;
        if (t < 0.0f) t = 0.0f;
    }

    for (int i = 0; i < g_game_state.other_player_count; i++) {
        PlayerState* p = &g_game_state.other_players[i];
        p->base.interp_pos.x = p->base.pos_prev.x + (p->base.pos_server.x - p->base.pos_prev.x) * t;
        p->base.interp_pos.y = p->base.pos_prev.y + (p->base.pos_server.y - p->base.pos_prev.y) * t;
    }
    for (int i = 0; i < g_game_state.bot_count; i++) {
        BotState* b = &g_game_state.bots[i];
        b->base.interp_pos.x = b->base.pos_prev.x + (b->base.pos_server.x - b->base.pos_prev.x) * t;
        b->base.interp_pos.y = b->base.pos_prev.y + (b->base.pos_server.y - b->base.pos_prev.y) * t;
    }
}
