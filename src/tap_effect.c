/**
 * @file tap_effect.c
 * @brief Premium pixel-art tap feedback rendered in screen space.
 *
 * The effect is built from three lightweight layers that can be combined via a
 * style bitmask:
 *   - Pixel burst      : chunky particles with a slight ballistic falloff.
 *   - Ring pulse       : thick expanding ring with crisp contrast.
 *   - Marker flare     : short heavy brackets around the tap center.
 *
 * Design goals:
 *   - crisp retro aesthetic (snapped coordinates, blocky particles)
 *   - browser-friendly (fixed pool, no heap allocation, simple primitives)
 *   - reusable from any system that already knows the target screen position
 */

#include "tap_effect.h"
#include <math.h>
#include <string.h>

#define TAP_EFFECT_PARTICLE_COUNT 10
#define TAP_EFFECT_TAU 6.28318530718f

typedef struct {
    Vector2 position;
    Vector2 particle_dir[TAP_EFFECT_PARTICLE_COUNT];
    float particle_travel[TAP_EFFECT_PARTICLE_COUNT];
    float particle_size[TAP_EFFECT_PARTICLE_COUNT];
    float particle_delay[TAP_EFFECT_PARTICLE_COUNT];
    float particle_twinkle[TAP_EFFECT_PARTICLE_COUNT];
    float age;
    float duration;
    float scale;
    float intensity;
    Color color;
    unsigned int style_mask;
    bool active;
} TapEffectEntry;

static TapEffectEntry s_entries[TAP_EFFECT_MAX_ENTRIES];
static bool s_tap_effect_ready = false;
static unsigned int s_rng_state = 0xC0FFEE11u;

static float tap_effect_clampf(float value, float min_value, float max_value) {
    if (value < min_value) return min_value;
    if (value > max_value) return max_value;
    return value;
}

static float tap_effect_snapf(float value) {
    return floorf(value + 0.5f);
}

static float tap_effect_rand01(void) {
    s_rng_state = s_rng_state * 1664525u + 1013904223u;
    return (float)(s_rng_state >> 8) / (float)(1u << 24);
}

static float tap_effect_rand_signed(void) {
    return tap_effect_rand01() * 2.0f - 1.0f;
}

static float tap_effect_ease_out_cubic(float t) {
    float inv = 1.0f - tap_effect_clampf(t, 0.0f, 1.0f);
    return 1.0f - inv * inv * inv;
}

static float tap_effect_ease_out_quart(float t) {
    float inv = 1.0f - tap_effect_clampf(t, 0.0f, 1.0f);
    return 1.0f - inv * inv * inv * inv;
}

static Color tap_effect_alpha(Color color, float alpha_scale) {
    Color out = color;
    out.a = (unsigned char)tap_effect_clampf((float)color.a * alpha_scale, 0.0f, 255.0f);
    return out;
}

static Color tap_effect_mix(Color a, Color b, float t) {
    t = tap_effect_clampf(t, 0.0f, 1.0f);
    return (Color){
        (unsigned char)(a.r + (b.r - a.r) * t),
        (unsigned char)(a.g + (b.g - a.g) * t),
        (unsigned char)(a.b + (b.b - a.b) * t),
        (unsigned char)(a.a + (b.a - a.a) * t),
    };
}

static Color tap_effect_glow(Color color) {
    return tap_effect_mix(color, WHITE, 0.35f);
}

static Color tap_effect_shadow_tint(Color color) {
    return tap_effect_mix(color, BLACK, 0.72f);
}

static void tap_effect_draw_rect_centered(float x, float y, float w, float h, float rotation, Color color) {
    Rectangle rect = {
        tap_effect_snapf(x - w * 0.5f),
        tap_effect_snapf(y - h * 0.5f),
        tap_effect_snapf(w),
        tap_effect_snapf(h),
    };
    Vector2 origin = { rect.width * 0.5f, rect.height * 0.5f };
    DrawRectanglePro(rect, origin, rotation, color);
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

static void tap_effect_draw_marker_flare(const TapEffectEntry* entry, float t) {
    float intro = tap_effect_ease_out_quart(tap_effect_clampf(t / 0.55f, 0.0f, 1.0f));
    float alpha = 0.92f - 0.20f * t;
    float size = tap_effect_snapf((8.0f + 12.0f * intro) * entry->scale * (0.90f + 0.22f * entry->intensity));
    float length = tap_effect_snapf(fmaxf(5.0f, size * 0.42f));
    float thickness = tap_effect_snapf(fmaxf(3.0f, 3.0f * entry->scale));
    float gap = tap_effect_snapf(fmaxf(3.0f, size * 0.22f));
    float x = tap_effect_snapf(entry->position.x);
    float y = tap_effect_snapf(entry->position.y);

    DrawRectangle((int)(x - gap - length + 1.0f), (int)(y - thickness * 0.5f + 1.0f), (int)length, (int)thickness,
                  tap_effect_alpha(tap_effect_shadow_tint(entry->color), alpha * 0.35f));
    DrawRectangle((int)(x + gap + 1.0f), (int)(y - thickness * 0.5f + 1.0f), (int)length, (int)thickness,
                  tap_effect_alpha(tap_effect_shadow_tint(entry->color), alpha * 0.35f));
    DrawRectangle((int)(x - thickness * 0.5f + 1.0f), (int)(y - gap - length + 1.0f), (int)thickness, (int)length,
                  tap_effect_alpha(tap_effect_shadow_tint(entry->color), alpha * 0.35f));
    DrawRectangle((int)(x - thickness * 0.5f + 1.0f), (int)(y + gap + 1.0f), (int)thickness, (int)length,
                  tap_effect_alpha(tap_effect_shadow_tint(entry->color), alpha * 0.35f));

    DrawRectangle((int)(x - gap - length), (int)(y - thickness * 0.5f), (int)length, (int)thickness,
                  tap_effect_alpha(entry->color, alpha * 0.92f));
    DrawRectangle((int)(x + gap), (int)(y - thickness * 0.5f), (int)length, (int)thickness,
                  tap_effect_alpha(entry->color, alpha * 0.92f));
    DrawRectangle((int)(x - thickness * 0.5f), (int)(y - gap - length), (int)thickness, (int)length,
                  tap_effect_alpha(entry->color, alpha * 0.92f));
    DrawRectangle((int)(x - thickness * 0.5f), (int)(y + gap), (int)thickness, (int)length,
                  tap_effect_alpha(entry->color, alpha * 0.92f));
}

static void tap_effect_draw_ring_pulse(const TapEffectEntry* entry, float t) {
    float eased = tap_effect_ease_out_cubic(t);
    float alpha = 0.96f - 0.24f * t;
    float radius = tap_effect_snapf((7.0f + 22.0f * eased) * entry->scale * (0.94f + 0.24f * entry->intensity));
    float thickness = tap_effect_snapf(fmaxf(4.0f, (6.0f - 2.0f * t) * entry->scale));
    float inner = fmaxf(0.0f, radius - thickness);
    Vector2 shadow_center = { entry->position.x + 1.0f, entry->position.y + 1.0f };

    DrawRing(shadow_center, fmaxf(0.0f, inner - 1.0f), radius + 1.0f, 0.0f, 360.0f, 28,
             tap_effect_alpha(tap_effect_shadow_tint(entry->color), alpha * 0.24f));
    DrawRing(entry->position, inner, radius, 0.0f, 360.0f, 28,
             tap_effect_alpha(entry->color, alpha * 0.90f));
    DrawRing(entry->position, fmaxf(0.0f, radius - thickness * 0.55f), fmaxf(1.0f, radius - 1.0f),
             0.0f, 360.0f, 28, tap_effect_alpha(tap_effect_glow(entry->color), alpha));
}

static void tap_effect_draw_pixel_burst(const TapEffectEntry* entry, float t) {
    float alpha = 0.94f - 0.34f * t;
    Color shadow = tap_effect_alpha(tap_effect_shadow_tint(entry->color), alpha * 0.22f);
    Color body = tap_effect_alpha(entry->color, alpha * 0.96f);
    Color highlight = tap_effect_alpha(tap_effect_glow(entry->color), alpha);

    for (int i = 0; i < TAP_EFFECT_PARTICLE_COUNT; i++) {
        float delay = entry->particle_delay[i];
        float particle_t = tap_effect_clampf((t - delay) / (1.0f - delay * 0.75f), 0.0f, 1.0f);
        if (particle_t <= 0.0f) continue;

        float travel = entry->particle_travel[i] * tap_effect_ease_out_cubic(particle_t);
        float fall = (6.0f + 6.0f * entry->intensity) * particle_t * particle_t * entry->scale;
        float x = entry->position.x + entry->particle_dir[i].x * travel;
        float y = entry->position.y + entry->particle_dir[i].y * travel + fall;

        float size = tap_effect_snapf(fmaxf(1.0f, entry->particle_size[i] * (1.0f - 0.68f * particle_t)));
        float stretch = 1.35f + 1.25f * (1.0f - particle_t);
        float thickness = tap_effect_snapf(fmaxf(2.0f, size * 1.25f));
        float rotation = entry->particle_twinkle[i] + sinf(entry->particle_twinkle[i] + particle_t * 9.0f) * 14.0f;

        tap_effect_draw_rect_centered(x + 1.0f, y + 1.0f, size * stretch, thickness, rotation, shadow);
        tap_effect_draw_rect_centered(x, y, size * stretch, thickness, rotation, body);
        tap_effect_draw_rect_centered(x, y, fmaxf(2.0f, size * stretch - 2.0f), fmaxf(1.0f, thickness - 2.0f), rotation, highlight);
    }
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
    TapEffectParams params;
    params.color = (Color){ 255, 212, 72, 255 };
    params.scale = 1.05f;
    params.duration = 0.38f;
    params.intensity = 1.15f;
    params.style_mask = TAP_EFFECT_STYLE_PREMIUM;
    return params;
}

void tap_effect_spawn(Vector2 screen_position, const TapEffectParams* params) {
    if (!s_tap_effect_ready) tap_effect_init();

    TapEffectParams cfg = params ? *params : tap_effect_default_params();
    cfg.scale = tap_effect_clampf(cfg.scale <= 0.0f ? 1.0f : cfg.scale, 0.35f, 3.5f);
    cfg.duration = tap_effect_clampf(cfg.duration <= 0.0f ? 0.34f : cfg.duration, 0.12f, 1.20f);
    cfg.intensity = tap_effect_clampf(cfg.intensity <= 0.0f ? 1.0f : cfg.intensity, 0.20f, 2.50f);
    if (cfg.style_mask == 0) cfg.style_mask = TAP_EFFECT_STYLE_PREMIUM;
    if (cfg.color.a == 0) cfg.color = tap_effect_default_params().color;

    TapEffectEntry* entry = tap_effect_alloc_entry();
    entry->position.x = tap_effect_snapf(screen_position.x);
    entry->position.y = tap_effect_snapf(screen_position.y);
    entry->duration = cfg.duration;
    entry->scale = cfg.scale;
    entry->intensity = cfg.intensity;
    entry->color = cfg.color;
    entry->style_mask = cfg.style_mask;

    float base_spin = tap_effect_rand01() * 360.0f;
    for (int i = 0; i < TAP_EFFECT_PARTICLE_COUNT; i++) {
        float angle = ((float)i / (float)TAP_EFFECT_PARTICLE_COUNT) * TAP_EFFECT_TAU;
        angle += tap_effect_rand_signed() * 0.34f;

        entry->particle_dir[i].x = cosf(angle);
        entry->particle_dir[i].y = sinf(angle);
        entry->particle_travel[i] =
            (10.0f + 18.0f * tap_effect_rand01()) * cfg.scale * (0.85f + cfg.intensity * 0.55f);
        entry->particle_size[i] =
            tap_effect_snapf((4.0f + floorf(tap_effect_rand01() * 3.0f)) * cfg.scale);
        entry->particle_delay[i] = tap_effect_rand01() * 0.12f;
        entry->particle_twinkle[i] = base_spin + tap_effect_rand_signed() * 90.0f;
    }
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

        if (entry->style_mask & TAP_EFFECT_STYLE_MARKER_FLARE) {
            tap_effect_draw_marker_flare(entry, t);
        }
        if (entry->style_mask & TAP_EFFECT_STYLE_RING_PULSE) {
            tap_effect_draw_ring_pulse(entry, t);
        }
        if (entry->style_mask & TAP_EFFECT_STYLE_PIXEL_BURST) {
            tap_effect_draw_pixel_burst(entry, t);
        }
    }
}
