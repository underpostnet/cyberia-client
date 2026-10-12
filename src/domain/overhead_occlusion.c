#include "overhead_occlusion.h"

#include "game_state.h"
#include "presentation_runtime.h"
#include "ui/ease.h"
#include "world_types.h"

#include <math.h>
#include <raylib.h>
#include <stdbool.h>
#include <string.h>

/* Roofs that fade or stay hidden at one time. A roof with no entry is fully shown. */
#define MAX_FADING_ROOFS 32

typedef struct {
    EntityID id;
    float progress;   /* 0 = shown, 1 = hidden */
} FadingRoof;

static FadingRoof s_roofs[MAX_FADING_ROOFS];
static int        s_roof_count;

static const FadingRoof* find_roof(EntityID id) {
    for (int i = 0; i < s_roof_count; i++) {
        if (id == s_roofs[i].id) return &s_roofs[i];
    }
    return NULL;
}

void overhead_occlusion_update(float dt) {
    const int   fade_ms = presentation_runtime_overhead_occlusion_fade_ms();
    const float step    = 0 < fade_ms ? dt * 1000.0f / (float)fade_ms : 1.0f;
    const Vector2 at   = game_state_own_render()->interp_pos;
    const Vector2 size = game_state_own_transform()->size;
    const Vector2 centre = { at.x + size.x / 2.0f, at.y + size.y / 2.0f };

    /* Rebuilt from the snapshot each frame, so a roof that leaves the AOI loses its entry. */
    FadingRoof next[MAX_FADING_ROOFS];
    int next_count = 0;
    for (int i = 0; i < g_game_state.foreground_count && MAX_FADING_ROOFS > next_count; i++) {
        const WorldObject* fg = &g_game_state.foregrounds[i];
        if (!fg->overhead_occlusion) continue;

        const EntityTransform* tf = game_state_transform(fg->id);
        const bool under = CheckCollisionPointRec(centre, (Rectangle){ tf->pos.x, tf->pos.y, tf->size.x, tf->size.y });
        const FadingRoof* roof = find_roof(fg->id);
        const float current = NULL != roof ? roof->progress : 0.0f;
        const float progress = under ? fminf(1.0f, current + step) : fmaxf(0.0f, current - step);
        if (0.0f == progress) continue;

        next[next_count].id = fg->id;
        next[next_count].progress = progress;
        next_count++;
    }
    memcpy(s_roofs, next, sizeof(FadingRoof) * (size_t)next_count);
    s_roof_count = next_count;
}

float overhead_occlusion_opacity(const WorldObject* fg) {
    if (!fg->overhead_occlusion) return 1.0f;
    const FadingRoof* roof = find_roof(fg->id);
    if (NULL == roof) return 1.0f;
    const float hidden = presentation_runtime_overhead_occlusion_hidden_opacity();
    return 1.0f + (hidden - 1.0f) * ease_smoothstep(roof->progress);
}
