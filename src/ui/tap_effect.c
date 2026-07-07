// Screen-space tap feedback: a single electric-yellow cross with a wide black
// border that forms at the tap center. Fixed pool, no heap allocation.

#include "tap_effect.h"

#include <math.h>
#include <string.h>

#define TAP_EFFECT_TAU 6.28318530718f
#define TAP_EFFECT_PULSE_CYCLES 2.0f

typedef struct {
    Vector2 position;
    float age;
    float duration;
    float scale;
    float intensity;
    Color color;
    bool active;
} TapEffectEntry;

static TapEffectEntry s_entries[TAP_EFFECT_MAX_ENTRIES];
static bool s_tap_effect_ready = false;

static float tap_effect_clampf(float value, float min_value, float max_value) {
    if (value < min_value) return min_value;
    if (value > max_value) return max_value;
    return value;
}

static float tap_effect_snapf(float value) {
    return floorf(value + 0.5f);
}

static float tap_effect_ease_out_quart(float t) {
    float inv = 1.0f - tap_effect_clampf(t, 0.0f, 1.0f);
    return 1.0f - inv * inv * inv * inv;
}

static TapEffectEntry* tap_effect_alloc_entry(void) {
    TapEffectEntry* slot = NULL;
    float oldest_age = -1.0f;
    int oldest_index = 0;

    for (int i = 0; i < TAP_EFFECT_MAX_ENTRIES; i++) {
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

static void tap_effect_draw_bar(float x, float y, float w, float h, float border, Color body) {
    DrawRectangle((int)(x - border), (int)(y - border), (int)(w + border * 2.0f), (int)(h + border * 2.0f), BLACK);
    DrawRectangle((int)x, (int)y, (int)w, (int)h, body);
}

static void tap_effect_draw_cross(const TapEffectEntry* entry, float t) {
    float grow = tap_effect_ease_out_quart(tap_effect_clampf(t / 0.22f, 0.0f, 1.0f));
    float pulse = 1.0f + 0.20f * sinf(t * TAP_EFFECT_TAU * TAP_EFFECT_PULSE_CYCLES);
    float size = tap_effect_snapf((14.0f + 14.0f * grow) * entry->scale * (0.90f + 0.22f * entry->intensity) * pulse);
    float length = tap_effect_snapf(fmaxf(8.0f, size * 0.40f));
    float thickness = tap_effect_snapf(fmaxf(5.0f, size * 0.16f));
    float gap = tap_effect_snapf(fmaxf(6.0f, size * 0.30f));
    float border = tap_effect_snapf(fmaxf(3.0f, thickness * 0.60f));
    float x = tap_effect_snapf(entry->position.x);
    float y = tap_effect_snapf(entry->position.y);
    float half = tap_effect_snapf(thickness * 0.5f);
    Color body = entry->color;

    tap_effect_draw_bar(x - gap - length, y - half, length, thickness, border, body);
    tap_effect_draw_bar(x + gap, y - half, length, thickness, border, body);
    tap_effect_draw_bar(x - half, y - gap - length, thickness, length, border, body);
    tap_effect_draw_bar(x - half, y + gap, thickness, length, border, body);
}

void tap_effect_init(void) {
    memset(s_entries, 0, sizeof(s_entries));
    s_tap_effect_ready = true;
}

void tap_effect_reset(void) {
    memset(s_entries, 0, sizeof(s_entries));
    s_tap_effect_ready = false;
}

TapEffectParams tap_effect_default_params(void) {
    TapEffectParams params = {
        .color = (Color){ 255, 234, 0, 255 },
        .scale = 1.05f,
        .duration = 0.70f,
        .intensity = 1.15f,
    };
    return params;
}

void tap_effect_spawn(Vector2 screen_position, const TapEffectParams* params) {
    if (!s_tap_effect_ready) tap_effect_init();

    TapEffectParams cfg = params ? *params : tap_effect_default_params();
    cfg.scale = tap_effect_clampf(cfg.scale <= 0.0f ? 1.0f : cfg.scale, 0.35f, 3.5f);
    cfg.duration = tap_effect_clampf(cfg.duration <= 0.0f ? 0.34f : cfg.duration, 0.12f, 1.20f);
    cfg.intensity = tap_effect_clampf(cfg.intensity <= 0.0f ? 1.0f : cfg.intensity, 0.20f, 2.50f);
    if (0 == cfg.color.a) cfg.color = tap_effect_default_params().color;

    TapEffectEntry* entry = tap_effect_alloc_entry();
    entry->position.x = tap_effect_snapf(screen_position.x);
    entry->position.y = tap_effect_snapf(screen_position.y);
    entry->duration = cfg.duration;
    entry->scale = cfg.scale;
    entry->intensity = cfg.intensity;
    entry->color = cfg.color;
}

void tap_effect_update(float dt) {
    if (!s_tap_effect_ready) return;

    for (int i = 0; i < TAP_EFFECT_MAX_ENTRIES; i++) {
        if (!s_entries[i].active) continue;
        s_entries[i].age += dt;
        if (s_entries[i].age >= s_entries[i].duration) {
            s_entries[i].active = false;
        }
    }
}

void tap_effect_draw(void) {
    if (!s_tap_effect_ready) return;

    for (int i = 0; i < TAP_EFFECT_MAX_ENTRIES; i++) {
        const TapEffectEntry* entry = &s_entries[i];
        if (!entry->active || entry->duration <= 0.0f) continue;

        float t = tap_effect_clampf(entry->age / entry->duration, 0.0f, 1.0f);
        tap_effect_draw_cross(entry, t);
    }
}
