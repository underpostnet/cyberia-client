#include "view/overhead_occlusion.h"
#include "sim/game_state.h"

#include <assert.h>
#include <math.h>
#include <raylib.h>
#include <stdio.h>
#include <string.h>

GameState g_game_state;
static float hidden_opacity = 0.2f;
static int fade_ms = 200;

float presentation_runtime_overhead_occlusion_hidden_opacity(void) { return hidden_opacity; }
int presentation_runtime_overhead_occlusion_fade_ms(void) { return fade_ms; }
bool CheckCollisionPointRec(Vector2 point, Rectangle rec) {
    return rec.x <= point.x && point.x < rec.x + rec.width && rec.y <= point.y && point.y < rec.y + rec.height;
}

static bool near(float a, float b) { return 1e-4f > fabsf(a - b); }

/* Slot i holds id i + 1: the roof, the canopy, then the local player. */
int game_state_slot(EntityID id) { return 0 < id ? (int)id - 1 : -1; }
static RenderState own_render;
static const EntityTransform own_transform = { .size = { 2.0f, 2.0f } };
const RenderState* game_state_own_render(void) { return &own_render; }
const EntityTransform* game_state_own_transform(void) { return &own_transform; }

/* Puts the centre of the 2x2 local player on (x, y). */
static void stand_at(float x, float y) {
    own_render.interp_pos = (Vector2){ x - 1.0f, y - 1.0f };
}

int main(void) {
    g_game_state.scenery_count = 2;
    const EntityID roof = 1, canopy = 2;
    g_game_state.scenery[0] = roof;
    g_game_state.scenery[1] = canopy;
    g_game_state.data[0] = (EntityData){ .kind = ENTITY_KIND_SCENERY, .draw_order = DRAW_FOREGROUND,
                                         .flags = ENTITY_FLAG_OVERHEAD_OCCLUSION };
    g_game_state.data[1] = (EntityData){ .kind = ENTITY_KIND_SCENERY, .draw_order = DRAW_FOREGROUND };
    for (int i = 0; 2 > i; i++) {
        g_game_state.mask[i] = COMP_DATA | COMP_TRANSFORM;
        g_game_state.transform[i] = (EntityTransform){ .pos = { 10, 10 }, .size = { 6, 4 } };
    }

    /* Outside: shown. */
    stand_at(5.0f, 5.0f);
    overhead_occlusion_update(0.1f);
    assert(near(1.0f, overhead_occlusion_opacity(roof)));

    /* Enter: half the fade time gives half the eased way. */
    stand_at(12.0f, 12.0f);
    overhead_occlusion_update(0.1f);
    assert(near(0.6f, overhead_occlusion_opacity(roof)));
    /* A normal foreground never fades. */
    assert(near(1.0f, overhead_occlusion_opacity(canopy)));

    /* Inside: holds the hidden opacity. */
    overhead_occlusion_update(0.1f);
    assert(near(hidden_opacity, overhead_occlusion_opacity(roof)));
    overhead_occlusion_update(1.0f);
    assert(near(hidden_opacity, overhead_occlusion_opacity(roof)));

    /* Exit: fades back to shown. */
    stand_at(5.0f, 5.0f);
    overhead_occlusion_update(0.1f);
    assert(near(0.6f, overhead_occlusion_opacity(roof)));
    overhead_occlusion_update(0.1f);
    assert(near(1.0f, overhead_occlusion_opacity(roof)));

    /* A reversal starts from the current opacity. */
    stand_at(12.0f, 12.0f);
    overhead_occlusion_update(0.1f);
    stand_at(5.0f, 5.0f);
    overhead_occlusion_update(0.05f);
    assert(near(1.0f + (hidden_opacity - 1.0f) * 0.15625f, overhead_occlusion_opacity(roof)));

    /* A roof that leaves the snapshot loses its fade. */
    stand_at(12.0f, 12.0f);
    overhead_occlusion_update(0.2f);
    g_game_state.scenery_count = 0;
    overhead_occlusion_update(0.0f);
    g_game_state.scenery_count = 2;
    assert(near(1.0f, overhead_occlusion_opacity(roof)));

    /* No fade time: the roof hides at once. */
    fade_ms = 0;
    overhead_occlusion_update(0.0f);
    assert(near(hidden_opacity, overhead_occlusion_opacity(roof)));

    puts("overhead_occlusion_test OK");
    return 0;
}
