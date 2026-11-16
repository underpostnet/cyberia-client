#include "auto_target.h"

#include "content/stat_contract_generated.h"
#include "sim/game_state.h"

#include <assert.h>
#include <math.h>
#include <raylib.h>
#include <raymath.h>
#include <stdbool.h>
#include <string.h>

#define HOSTILE "hostile"

Vector2 auto_target_center(EntityID id) {
    Vector2 at   = game_state_render(id)->interp_pos;
    Vector2 size = game_state_transform(id)->size;
    return (Vector2){ at.x + size.x * 0.5f, at.y + size.y * 0.5f };
}

/* A living entity of the view, with what the view needs to place it. */
static bool alive(EntityID id) {
    return game_state_has(id, COMP_TRANSFORM | COMP_VITALS) && 0 == game_state_vitals(id)->respawn_in;
}

static bool hostile(EntityID id) {
    return alive(id) && game_state_has(id, COMP_AGENT) && 0 == strcmp(HOSTILE, game_state_agent(id)->behavior);
}

EntityID auto_target_predict(Vector2 from, Direction facing, float reach, EntityID preferred) {
    assert(DIRECTION_NONE != facing);
    if (0 != preferred && alive(preferred) && reach >= Vector2Distance(from, auto_target_center(preferred))) return preferred;
    Vector2  ahead = { CYBERIA_DIRECTION_VECTORS[facing][0], CYBERIA_DIRECTION_VECTORS[facing][1] };
    EntityID best = 0;
    bool     best_ahead = false;
    float    best_dist  = 0.0f;
    for (int i = 0; g_game_state.bot_count > i; i++) {
        EntityID id = g_game_state.bots[i];
        if (!hostile(id)) continue;
        Vector2 to   = Vector2Subtract(auto_target_center(id), from);
        float   dist = Vector2Length(to);
        if (0.0f == dist || reach < dist) continue;
        bool in_front = AUTO_TARGET_FACING_CONE <= Vector2DotProduct(to, ahead) / dist;
        bool better = 0 == best || (in_front && !best_ahead) ||
                      (in_front == best_ahead && (best_dist > dist || (best_dist == dist && best > id)));
        if (!better) continue;
        best       = id;
        best_ahead = in_front;
        best_dist  = dist;
    }
    return best;
}

EntityID auto_target_at(Vector2 at) {
    for (int i = 0; g_game_state.bot_count > i; i++) {
        EntityID id = g_game_state.bots[i];
        if (!hostile(id)) continue;
        Vector2 pos  = game_state_render(id)->interp_pos;
        Vector2 size = game_state_transform(id)->size;
        if (CheckCollisionPointRec(at, (Rectangle){ pos.x, pos.y, size.x, size.y })) return id;
    }
    return 0;
}
