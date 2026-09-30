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
    char  id[MAX_ID_LENGTH];
    float progress;   /* 0 = shown, 1 = hidden */
} FadingRoof;

static FadingRoof s_roofs[MAX_FADING_ROOFS];
static int        s_roof_count;

static const FadingRoof* find_roof(const char* id) {
    for (int i = 0; i < s_roof_count; i++) {
        if (0 == strcmp(s_roofs[i].id, id)) return &s_roofs[i];
    }
    return NULL;
}

void overhead_occlusion_update(float dt) {
    const int   fade_ms = presentation_runtime_overhead_occlusion_fade_ms();
    const float step    = 0 < fade_ms ? dt * 1000.0f / (float)fade_ms : 1.0f;
    const EntityState* player = &g_game_state.player.base;
    const Vector2 centre = { player->interp_pos.x + player->dims.x / 2.0f,
                             player->interp_pos.y + player->dims.y / 2.0f };

    /* Rebuilt from the snapshot each frame, so a roof that leaves the AOI loses its entry. */
    FadingRoof next[MAX_FADING_ROOFS];
    int next_count = 0;
    for (int i = 0; i < g_game_state.foreground_count && MAX_FADING_ROOFS > next_count; i++) {
        const WorldObject* fg = &g_game_state.foregrounds[i];
        if (!fg->overhead_occlusion) continue;

        const bool under = CheckCollisionPointRec(centre, (Rectangle){ fg->pos.x, fg->pos.y, fg->dims.x, fg->dims.y });
        const FadingRoof* roof = find_roof(fg->id);
        const float current = NULL != roof ? roof->progress : 0.0f;
        const float progress = under ? fminf(1.0f, current + step) : fmaxf(0.0f, current - step);
        if (0.0f == progress) continue;

        memcpy(next[next_count].id, fg->id, sizeof(next[next_count].id));
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
