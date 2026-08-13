/* Dust puffs kicked up under a moving entity's feet. Fixed pool, no heap.
 *
 * Each puff is thrown backward from the heading and sideways along an
 * alternating fan, and that throw decelerates against a drag: the kick is spent
 * early and the puff coasts to a stop behind the character, with its neighbours
 * leaving on opposing paths. A separate lift carries it up the screen for the
 * whole life, so the motion reads as dust rising diagonally up and back rather
 * than sliding along the floor.
 *
 * Roughly half of each burst hops: the larger motes, thrown harder both back
 * and up, bleeding that off as they climb so the jump arcs. The rest stay low.
 * Sizes vary widely, and each puff swells as it disperses before shrinking and
 * fading out, so a burst reads as a ragged cloud thinning rather than a row of
 * matching dots. */

#include "fx_dust.h"

#include "fx_shapes.h"

#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

/* Puff opacity holds for this fraction of the life, then fades out — dust is
 * fully formed the moment it is kicked, and only thins as it settles. */
#define FX_DUST_HOLD_FRACTION  0.30f

/* Size curve: swell to PEAK by PEAK_AT, then shrink toward END. */
#define FX_DUST_SIZE_PEAK      1.30f
#define FX_DUST_SIZE_PEAK_AT   0.25f
#define FX_DUST_SIZE_END       0.45f

/* Velocity decay per second — the settle. Low enough that the backward throw
 * plays out across the life instead of snapping to a stop in a few frames. */
#define FX_DUST_DRAG           3.5f

/* Backward throw spread, radians each side of the reverse heading. */
#define FX_DUST_ARC            0.95f

/* Per-puff variation, so no two motes match. */
#define FX_DUST_SIZE_JITTER_LO 0.55f
#define FX_DUST_SIZE_JITTER_HI 1.55f
#define FX_DUST_RISE_JITTER_LO 0.65f
#define FX_DUST_RISE_JITTER_HI 1.40f

/* A share of every burst hops instead of drifting: the biggest motes, kicked
 * up hard off the foot and thrown back at the same time so the jump runs
 * diagonally, slowing as it climbs. The rest stay low and drift. */
#define FX_DUST_HOP_CHANCE     0.45f
#define FX_DUST_HOP_SPEED_LO   3.0f
#define FX_DUST_HOP_SPEED_HI   5.0f
#define FX_DUST_HOP_DECAY      4.5f
#define FX_DUST_HOP_BACK_SCALE 1.70f
#define FX_DUST_HOP_SIZE_SCALE 1.35f

/* Sideways throw, alternating per puff. Every burst therefore has motes leaving
 * in opposing directions instead of one shared path. */
#define FX_DUST_FAN_JITTER_LO  0.45f
#define FX_DUST_FAN_JITTER_HI  1.50f

#define FX_DUST_MIN_SIZE_PX    1.5f

typedef struct {
    Vector2 position;   /* world px */
    Vector2 velocity;   /* world px/s — the backward kick, spent against drag */
    float   rise_speed; /* world px/s upward; steady, unless this one hops */
    float   age;
    float   duration;
    float   size;       /* world px at birth */
    Color   color;
    bool    hop;        /* launched, so the climb slows instead of holding */
    bool    active;
} FxDustPuff;

static FxDustPuff s_puffs[FX_DUST_MAX_PUFFS];
static bool       s_ready = false;

/* Deterministic LCG — avoids perturbing the global rand() state. */
static uint32_t s_lcg = 0x2B7E1518u;
static float lcg_f01(void) {
    s_lcg = s_lcg * 1664525u + 1013904223u;
    return (float)(s_lcg >> 8) / (float)(1u << 24);
}
static float lcg_range(float lo, float hi) { return lo + lcg_f01() * (hi - lo); }

static float dust_clampf(float v, float lo, float hi) {
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

/* Free slot, else the oldest puff — a burst never fails, it just recycles the
 * dust closest to expiring. */
static FxDustPuff* alloc_puff(void) {
    FxDustPuff* slot = NULL;
    float oldest = -1.0f;
    int   oldest_index = 0;

    for (int i = 0; i < FX_DUST_MAX_PUFFS; i++) {
        if (!s_puffs[i].active) {
            slot = &s_puffs[i];
            break;
        }
        float remaining = s_puffs[i].duration - s_puffs[i].age;
        float staleness = -remaining;
        if (staleness > oldest) {
            oldest = staleness;
            oldest_index = i;
        }
    }
    if (!slot) slot = &s_puffs[oldest_index];
    memset(slot, 0, sizeof(*slot));
    slot->active = true;
    return slot;
}

static float size_curve(float t) {
    if (t <= FX_DUST_SIZE_PEAK_AT) {
        float u = t / FX_DUST_SIZE_PEAK_AT;
        return 1.0f + (FX_DUST_SIZE_PEAK - 1.0f) * u;
    }
    float u = (t - FX_DUST_SIZE_PEAK_AT) / (1.0f - FX_DUST_SIZE_PEAK_AT);
    return FX_DUST_SIZE_PEAK + (FX_DUST_SIZE_END - FX_DUST_SIZE_PEAK) * u * u;
}

static float alpha_curve(float t) {
    if (t <= FX_DUST_HOLD_FRACTION) return 1.0f;
    float u = (t - FX_DUST_HOLD_FRACTION) / (1.0f - FX_DUST_HOLD_FRACTION);
    return (1.0f - u) * (1.0f - u);
}

void fx_dust_init(void) {
    memset(s_puffs, 0, sizeof(s_puffs));
    s_ready = true;
}

void fx_dust_reset(void) {
    memset(s_puffs, 0, sizeof(s_puffs));
    s_ready = false;
}

FxDustParams fx_dust_default_params(void) {
    FxDustParams params = {
        .color    = (Color){ 82, 80, 78, 255 },  /* sooty grit */
        .size     = 3.5f,
        .duration = 0.45f,
        .drift    = 7.0f,
        .rise     = 5.0f,
        .fan      = 4.0f,
        .spread   = 5.0f,
        .count    = 4,
    };
    return params;
}

void fx_dust_spawn(Vector2 world_pos, Vector2 heading, const FxDustParams* params) {
    if (!s_ready) fx_dust_init();

    FxDustParams cfg = params ? *params : fx_dust_default_params();
    FxDustParams def = fx_dust_default_params();
    cfg.size     = dust_clampf(cfg.size     <= 0.0f ? def.size     : cfg.size,     1.0f, 24.0f);
    cfg.duration = dust_clampf(cfg.duration <= 0.0f ? def.duration : cfg.duration, 0.10f, 1.20f);
    cfg.drift    = dust_clampf(cfg.drift    <  0.0f ? def.drift    : cfg.drift,    0.0f, 120.0f);
    cfg.rise     = dust_clampf(cfg.rise     <  0.0f ? def.rise     : cfg.rise,     0.0f, 120.0f);
    cfg.fan      = dust_clampf(cfg.fan      <  0.0f ? def.fan      : cfg.fan,      0.0f,  80.0f);
    cfg.spread   = dust_clampf(cfg.spread   <  0.0f ? def.spread   : cfg.spread,   0.0f,  40.0f);
    if (cfg.count <= 0) cfg.count = def.count;
    if (cfg.count > FX_DUST_MAX_PUFFS) cfg.count = FX_DUST_MAX_PUFFS;
    if (0 == cfg.color.a) cfg.color = def.color;

    /* Reverse of travel: the direction the dust is thrown. */
    float len = sqrtf(heading.x * heading.x + heading.y * heading.y);
    float hx  = (len > 0.0f) ? heading.x / len : 0.0f;
    float hy  = (len > 0.0f) ? heading.y / len : 0.0f;
    float base_angle = (len > 0.0f) ? atan2f(-hy, -hx) : lcg_range(0.0f, 6.2831853f);
    /* Travel is spent against the drag, so the launch speed is what decays to
     * that distance over the life: v0 = distance * drag / (1 − e^(−drag·T)). */
    float decay = 1.0f - expf(-FX_DUST_DRAG * cfg.duration);
    float back_speed = (decay > 0.0f) ? cfg.drift * FX_DUST_DRAG / decay : 0.0f;
    float fan_speed  = (decay > 0.0f) ? cfg.fan   * FX_DUST_DRAG / decay : 0.0f;

    for (int i = 0; i < cfg.count; i++) {
        FxDustPuff* puff = alloc_puff();
        puff->hop = lcg_f01() < FX_DUST_HOP_CHANCE;

        /* Scatter across the travel axis, not along it, so the burst reads as a
         * foot's width of dust rather than a trail. */
        float lateral = lcg_range(-cfg.spread, cfg.spread);
        puff->position = (Vector2){ world_pos.x - hy * lateral,
                                    world_pos.y + hx * lateral };

        /* Backward throw, plus a sideways one that alternates sides down the
         * burst so the motes leave along opposing paths. */
        float angle = base_angle + lcg_range(-FX_DUST_ARC, FX_DUST_ARC);
        float v = back_speed * lcg_range(0.65f, 1.15f);
        if (puff->hop) v *= FX_DUST_HOP_BACK_SCALE;
        float side = (0 == (i & 1)) ? 1.0f : -1.0f;
        float f = side * fan_speed * lcg_range(FX_DUST_FAN_JITTER_LO, FX_DUST_FAN_JITTER_HI);
        puff->velocity = (Vector2){ cosf(angle) * v - hy * f,
                                    sinf(angle) * v + hx * f };

        puff->duration = cfg.duration * lcg_range(0.85f, 1.15f);
        puff->size     = cfg.size * lcg_range(FX_DUST_SIZE_JITTER_LO, FX_DUST_SIZE_JITTER_HI);
        if (puff->hop) puff->size *= FX_DUST_HOP_SIZE_SCALE;
        puff->color    = cfg.color;

        /* Lift, so the puff is still climbing as it fades instead of stalling
         * with the throw. Screen up is −y. Most motes hold a steady climb; a
         * hopper leaves the foot far faster and bleeds that off as it goes,
         * which with the harder backward throw is what bends its jump into a
         * diagonal instead of a straight lift. */
        float rise = (puff->duration > 0.0f)
            ? cfg.rise / puff->duration *
              lcg_range(FX_DUST_RISE_JITTER_LO, FX_DUST_RISE_JITTER_HI)
            : 0.0f;
        if (puff->hop) rise *= lcg_range(FX_DUST_HOP_SPEED_LO, FX_DUST_HOP_SPEED_HI);
        puff->rise_speed = rise;
    }
}

void fx_dust_update(float dt) {
    if (!s_ready || dt <= 0.0f) return;

    float keep     = expf(-FX_DUST_DRAG * dt);
    float hop_keep = expf(-FX_DUST_HOP_DECAY * dt);
    for (int i = 0; i < FX_DUST_MAX_PUFFS; i++) {
        FxDustPuff* puff = &s_puffs[i];
        if (!puff->active) continue;

        puff->age += dt;
        if (puff->age >= puff->duration) {
            puff->active = false;
            continue;
        }
        puff->position.x += puff->velocity.x * dt;
        puff->position.y += (puff->velocity.y - puff->rise_speed) * dt;
        puff->velocity.x *= keep;
        puff->velocity.y *= keep;
        if (puff->hop) puff->rise_speed *= hop_keep;
    }
}

void fx_dust_draw(void) {
    if (!s_ready) return;

    for (int i = 0; i < FX_DUST_MAX_PUFFS; i++) {
        const FxDustPuff* puff = &s_puffs[i];
        if (!puff->active || puff->duration <= 0.0f) continue;

        float t = dust_clampf(puff->age / puff->duration, 0.0f, 1.0f);
        float size = puff->size * size_curve(t);
        if (size < FX_DUST_MIN_SIZE_PX) size = FX_DUST_MIN_SIZE_PX;
        fx_shape_spark(puff->position.x, puff->position.y, size,
                       puff->color, alpha_curve(t));
    }
}

int fx_dust_active_count(void) {
    int n = 0;
    for (int i = 0; i < FX_DUST_MAX_PUFFS; i++) {
        if (s_puffs[i].active) n++;
    }
    return n;
}
