// Tap feedback: a single electric-yellow cross with a wide black border that
// forms at the tap's WORLD position — it stays pinned to the grid cell while
// the camera moves. Fixed pool, no heap allocation.

#include "fx_tap.h"

#include "fx_shapes.h"

#include "domain/camera.h"

#include <math.h>
#include <string.h>

#define FX_TAP_TAU 6.28318530718f
#define FX_TAP_PULSE_CYCLES 2.0f

typedef struct {
    Vector2 position;
    float age;
    float duration;
    float scale;
    float intensity;
    Color color;
    bool active;
} FxTapEntry;

static FxTapEntry s_entries[FX_TAP_MAX_ENTRIES];
static bool s_fx_tap_ready = false;

static float fx_tap_clampf(float value, float min_value, float max_value) {
    if (value < min_value) return min_value;
    if (value > max_value) return max_value;
    return value;
}

static float fx_tap_snapf(float value) {
    return floorf(value + 0.5f);
}

static float fx_tap_ease_out_quart(float t) {
    float inv = 1.0f - fx_tap_clampf(t, 0.0f, 1.0f);
    return 1.0f - inv * inv * inv * inv;
}

static FxTapEntry* fx_tap_alloc_entry(void) {
    FxTapEntry* slot = NULL;
    float oldest_age = -1.0f;
    int oldest_index = 0;

    for (int i = 0; i < FX_TAP_MAX_ENTRIES; i++) {
        if (!s_entries[i].active) {
            slot = &s_entries[i];
            break;
        }
        if (s_entries[i].age > oldest_age) {
            oldest_age = s_entries[i].age;
            oldest_index = i;
        }
    }

    if (!slot) slot = &s_entries[oldest_index];
    memset(slot, 0, sizeof(*slot));
    slot->active = true;
    return slot;
}

static void fx_tap_draw_cross(const FxTapEntry* entry, Vector2 screen_pos, float t) {
    float grow = fx_tap_ease_out_quart(fx_tap_clampf(t / 0.22f, 0.0f, 1.0f));
    float pulse = 1.0f + 0.20f * sinf(t * FX_TAP_TAU * FX_TAP_PULSE_CYCLES);
    float size = fx_tap_snapf((14.0f + 14.0f * grow) * entry->scale * (0.90f + 0.22f * entry->intensity) * pulse);
    float length = fx_tap_snapf(fmaxf(8.0f, size * 0.40f));
    float thickness = fx_tap_snapf(fmaxf(5.0f, size * 0.16f));
    float gap = fx_tap_snapf(fmaxf(6.0f, size * 0.30f));
    float border = fx_tap_snapf(fmaxf(3.0f, thickness * 0.60f));
    float x = fx_tap_snapf(screen_pos.x);
    float y = fx_tap_snapf(screen_pos.y);
    float half = fx_tap_snapf(thickness * 0.5f);
    Color body = entry->color;

    fx_shape_bar(x - gap - length, y - half, length, thickness, border, body);
    fx_shape_bar(x + gap, y - half, length, thickness, border, body);
    fx_shape_bar(x - half, y - gap - length, thickness, length, border, body);
    fx_shape_bar(x - half, y + gap, thickness, length, border, body);
}

void fx_tap_init(void) {
    memset(s_entries, 0, sizeof(s_entries));
    s_fx_tap_ready = true;
}

void fx_tap_reset(void) {
    memset(s_entries, 0, sizeof(s_entries));
    s_fx_tap_ready = false;
}

FxTapParams fx_tap_default_params(void) {
    FxTapParams params = {
        .color = (Color){ 255, 234, 0, 255 },
        .scale = 1.05f,
        .duration = 0.70f,
        .intensity = 1.15f,
    };
    return params;
}

void fx_tap_spawn(Vector2 world_position, const FxTapParams* params) {
    if (!s_fx_tap_ready) fx_tap_init();

    FxTapParams cfg = params ? *params : fx_tap_default_params();
    cfg.scale = fx_tap_clampf(cfg.scale <= 0.0f ? 1.0f : cfg.scale, 0.35f, 3.5f);
    cfg.duration = fx_tap_clampf(cfg.duration <= 0.0f ? 0.34f : cfg.duration, 0.12f, 1.20f);
    cfg.intensity = fx_tap_clampf(cfg.intensity <= 0.0f ? 1.0f : cfg.intensity, 0.20f, 2.50f);
    if (0 == cfg.color.a) cfg.color = fx_tap_default_params().color;

    FxTapEntry* entry = fx_tap_alloc_entry();
    entry->position = world_position;
    entry->duration = cfg.duration;
    entry->scale = cfg.scale;
    entry->intensity = cfg.intensity;
    entry->color = cfg.color;
}

void fx_tap_update(float dt) {
    if (!s_fx_tap_ready) return;

    for (int i = 0; i < FX_TAP_MAX_ENTRIES; i++) {
        if (!s_entries[i].active) continue;
        s_entries[i].age += dt;
        if (s_entries[i].age >= s_entries[i].duration) {
            s_entries[i].active = false;
        }
    }
}

void fx_tap_draw(void) {
    if (!s_fx_tap_ready) return;

    Camera2D cam = camera_get();
    for (int i = 0; i < FX_TAP_MAX_ENTRIES; i++) {
        const FxTapEntry* entry = &s_entries[i];
        if (!entry->active || entry->duration <= 0.0f) continue;

        Vector2 screen_pos = GetWorldToScreen2D(entry->position, cam);
        float t = fx_tap_clampf(entry->age / entry->duration, 0.0f, 1.0f);
        fx_tap_draw_cross(entry, screen_pos, t);
    }
}
