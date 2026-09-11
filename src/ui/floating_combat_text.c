/**
 * @file floating_combat_text.c
 * @brief Floating Combat Text — animated pop-up numbers for the Cyberia client.
 *
 * Surfaces every combat event as a short-lived animated number that pops,
 * rises, and fades from the event's world position.
 *
 * Animation phases (FCT_TOTAL_LIFETIME seconds total):
 *   Phase 1 — Pop   [0, FCT_POP_DURATION):
 *       Font blasts from 40% → pop_overshoot (1.20–1.70×, scaled with value)
 *       then snaps back to 1.0. Alpha ramps 0.2 → 1.0.
 *
 *   Phase 2 — Rise  [FCT_POP_DURATION, FCT_FADE_START):
 *       Upward velocity (per-type FCT_RISE_* world-units/s) plus random
 *       horizontal drift.  Damage: fast and wide.  Regen: gentle.
 *
 *   Phase 3 — Fade  [FCT_FADE_START, FCT_TOTAL_LIFETIME):
 *       Velocity decelerates (×(1−dt×5) each frame); alpha decays to 0.
 *
 * Rendering — 6 DrawText calls per active entry:
 *   1. Black drop-shadow at (+1, +2).
 *   2–5. Black outline at the 4 cardinal offsets (±1 px).
 *   6. Main colored text.
 *
 * Screen overlay (fct_draw_overlay(), called in screen space after EndMode2D):
 *   Damage → brief red   vignette (max alpha 0.28, decays in ~0.45 s).
 *   Regen  → brief green vignette (max alpha 0.14, decays in ~0.55 s).
 *
 * Per-type visual tuning lives in FCT_TUNING, indexed by FCTType.
 *
 * Damage/regen events are broadcast to every AOI viewer; the red/green screen
 * vignette is personal — it fires only when the event lands on the local
 * player's own footprint. XP events reach only their earner and flash nothing.
 */

#include "floating_combat_text.h"
#include "text.h"

#include "domain/local_player.h"
#include "domain/presentation_runtime.h"
#include "game_state.h"
#include "world_types.h"

#include <assert.h>
#include <math.h>
#include <raylib.h>
#include <stdio.h>
#include <string.h>

/* ── Global timing ─────────────────────────────────────────────────────── */

#define FCT_TOTAL_LIFETIME   2.2f   /* total seconds before entry is freed   */
#define FCT_POP_DURATION     0.09f  /* violent snap pop-in                   */
#define FCT_FADE_START       1.6f   /* second at which alpha decay begins    */

#define FCT_DRIFT_MIN        0.2f   /* minimum absolute drift for all types  */
#define FCT_FONT_MIN         14     /* minimum pixels (all types)            */

/* ── Per-type tuning ───────────────────────────────────────────────────── */

typedef struct {
    const char* format;    /* printf format for the value                     */
    Color       color;
    float       rise;      /* upward speed, world units/second                 */
    float       drift;     /* max horizontal drift, world units/second         */
    /* Font grows with log2(value+1)/log_div, saturating at font_max. A smaller
     * divisor reaches full size at a lower value. */
    float       log_div;
    int         font_max;
    float       overshoot; /* base pop-in scale                                */
    /* Screen vignette when the event lands on the local player: peak alpha
     * (0 for none), alpha lost per second, and tint. */
    float       flash;
    float       flash_decay;
    Color       flash_color;
} FCTTuning;

static const FCTTuning FCT_TUNING[FCT_TYPE_COUNT] = {
    [FCT_TYPE_DAMAGE] = { "-%u",    {255,  60,  60, 255}, 3.8f, 1.8f, 5.0f, 44, 1.45f, 0.28f, 2.20f, {180, 0, 0, 255} },
    [FCT_TYPE_REGEN]  = { "+%u",    { 80, 240,  80, 255}, 2.2f, 0.5f, 7.0f, 32, 1.20f, 0.14f, 1.80f, {0, 180, 0, 255} },
    [FCT_TYPE_XP]     = { "+%u XP", {255, 215,   0, 255}, 1.6f, 0.4f, 8.0f, 30, 1.30f, 0.00f, 0.00f, {0, 0, 0, 0} },
};

/* ── Internal entry ────────────────────────────────────────────────────── */

typedef struct {
    float   x, y;                    /* current world position (moves each frame)    */
    float   vx, vy;                  /* velocity in world units / second             */
    float   age;                     /* seconds elapsed since spawn                  */
    int     font_px;                 /* base font size in pixels (fixed at spawn)    */
    float   pop_overshoot;           /* peak scale during pop-in (type + value)      */
    char    text[32];                /* formatted string: "+42 wood", "-1337", etc.  */
    Color   base_color;              /* colour before alpha is applied               */
    FCTType type;                    /* FCT_TYPE_* — for draw-time differentiation   */
    bool    active;
} FCTEntry;

static FCTEntry s_pool[FCT_MAX_ENTRIES];
static bool     s_init = false;

/* Screen-overlay alphas per type: set by fct_spawn, decayed by fct_update,
 * read by fct_draw_overlay. */
static float s_overlay[FCT_TYPE_COUNT];

/* ── Deterministic LCG — avoids touching the global rand() state ────────── */

static uint32_t s_lcg = 0xBEEF1337u;

static float lcg_f01(void) {
    s_lcg = s_lcg * 1664525u + 1013904223u;
    return (float)(s_lcg >> 8) / (float)(1u << 24);
}

/* Whether a broadcast FCT event lies inside the local player's footprint —
 * gates the personal screen vignettes on viewer-shared (abstract) events. */
static bool fct_event_on_self(float wx, float wy) {
    const EntityState* self = &g_game_state.player.base;
    return wx >= self->interp_pos.x && wx <= self->interp_pos.x + self->dims.x
        && wy >= self->interp_pos.y && wy <= self->interp_pos.y + self->dims.y;
}

/* ── Public API ─────────────────────────────────────────────────────────── */

void fct_init(void) {
    memset(s_pool, 0, sizeof(s_pool));
    memset(s_overlay, 0, sizeof(s_overlay));
    s_init = true;
}

void fct_spawn(float world_x, float world_y, uint32_t value, FCTType type) {
    if (!s_init) fct_init();

    /* Find a free slot; evict the oldest active entry if the pool is full. */
    FCTEntry *slot      = NULL;
    float     oldest   = -1.0f;
    int       oldest_i = 0;
    for (int i = 0; i < FCT_MAX_ENTRIES; i++) {
        if (!s_pool[i].active) { slot = &s_pool[i]; break; }
        if (s_pool[i].age > oldest) { oldest = s_pool[i].age; oldest_i = i; }
    }
    if (!slot) slot = &s_pool[oldest_i];

    assert(type >= 0 && type < FCT_TYPE_COUNT);
    const FCTTuning* tune = &FCT_TUNING[type];
    snprintf(slot->text, sizeof(slot->text), tune->format, value);

    /* The screen vignette is personal — only when the event lands on the
     * local player. */
    if (tune->flash > 0.0f && fct_event_on_self(world_x, world_y)) s_overlay[type] = tune->flash;

    /* ── Font size — log₂ scale, per-type grow rate ─────────────────── */
    float log_v  = (value > 0) ? (float)log2((double)value + 1.0) : 1.0f;
    float size_f = (float)FCT_FONT_MIN
                 + (float)(tune->font_max - FCT_FONT_MIN) * (log_v / tune->log_div);
    if (size_f < (float)FCT_FONT_MIN)   size_f = (float)FCT_FONT_MIN;
    if (size_f > (float)tune->font_max) size_f = (float)tune->font_max;
    slot->font_px = (int)(size_f + 0.5f);

    /* ── Pop overshoot — scales with magnitude ────────────────────────── */
    float t_norm = log_v / tune->log_div;
    if (t_norm > 1.0f) t_norm = 1.0f;
    slot->pop_overshoot = tune->overshoot + 0.25f * t_norm;

    slot->base_color = tune->color;
    slot->type       = type;

    /* ── Velocity — random drift direction ────────────────────────────── */
    float drift = FCT_DRIFT_MIN + lcg_f01() * (tune->drift - FCT_DRIFT_MIN);
    if (lcg_f01() < 0.5f) drift = -drift;

    slot->x      = world_x;
    slot->y      = world_y;
    slot->vx     = drift;
    slot->vy     = -tune->rise;
    slot->age    = 0.0f;
    slot->active = true;
}

void fct_update(float dt) {
    /* Drain events queued by message.c via the local-player module. */
    int n = local_player_fct_count();
    for (int i = 0; i < n; i++) {
        const LocalFctEvent* ev = local_player_fct_at(i);
        if (!ev) continue;
        fct_spawn(ev->world_x, ev->world_y, ev->value, ev->type);
    }
    local_player_fct_clear();

    /* Decay screen-space overlays. */
    for (int t = 0; t < FCT_TYPE_COUNT; t++) {
        if (s_overlay[t] > 0.0f) {
            s_overlay[t] -= dt * FCT_TUNING[t].flash_decay;
            if (s_overlay[t] < 0.0f) s_overlay[t] = 0.0f;
        }
    }

    for (int i = 0; i < FCT_MAX_ENTRIES; i++) {
        FCTEntry *e = &s_pool[i];
        if (!e->active) continue;

        e->age += dt;
        if (e->age >= FCT_TOTAL_LIFETIME) { e->active = false; continue; }

        /* Fade phase: decelerate to a floating stop. */
        if (e->age > FCT_FADE_START) {
            float decel = 1.0f - dt * 5.0f;
            if (decel < 0.0f) decel = 0.0f;
            e->vy *= decel;
            e->vx *= decel;
        }

        e->x += e->vx * dt;
        e->y += e->vy * dt;
    }
}

void fct_draw(void) {
    float cell_size = world_cell_size();

    for (int i = 0; i < FCT_MAX_ENTRIES; i++) {
        FCTEntry *e = &s_pool[i];
        if (!e->active) continue;

        /* ── Alpha ───────────────────────────────────────────────────── */
        float alpha;
        if (e->age < FCT_POP_DURATION) {
            alpha = 0.2f + 0.8f * (e->age / FCT_POP_DURATION);
        } else if (e->age >= FCT_FADE_START) {
            float frac = (e->age - FCT_FADE_START) / (FCT_TOTAL_LIFETIME - FCT_FADE_START);
            alpha = 1.0f - frac;
        } else {
            alpha = 1.0f;
        }
        if (alpha < 0.0f) alpha = 0.0f;
        if (alpha > 1.0f) alpha = 1.0f;

        /* ── Pop scale: blast up to overshoot, then snap to 1.0 ──────── */
        int font_px = e->font_px;
        if (e->age < FCT_POP_DURATION) {
            float t = e->age / FCT_POP_DURATION;
            float scale;
            if (t < 0.6f) {
                scale = 0.4f + (t / 0.6f) * (e->pop_overshoot - 0.4f);
            } else {
                scale = e->pop_overshoot
                      - ((t - 0.6f) / 0.4f) * (e->pop_overshoot - 1.0f);
            }
            font_px = (int)((float)e->font_px * scale + 0.5f);
            if (font_px < 4) font_px = 4;
        }

        /* ── Screen position (text centered on world origin) ─────────── */
        int tw = MeasureText(e->text, font_px);
        int tx = (int)(e->x * cell_size - tw * 0.5f);
        int ty = (int)(e->y * cell_size);

        unsigned char a_main   = (unsigned char)(alpha * 255.0f + 0.5f);
        unsigned char a_shadow = (unsigned char)(alpha * 180.0f + 0.5f);
        unsigned char a_outln  = (unsigned char)(alpha * 220.0f + 0.5f);

        /* ── 1. Drop shadow ──────────────────────────────────────────── */
        DrawText(e->text, tx + 1, ty + 2, font_px, (Color){0, 0, 0, a_shadow});

        /* ── 2. Outline — 4 cardinal offsets, black ──────────────────── */
        Color outline = {0, 0, 0, a_outln};
        DrawText(e->text, tx - 1, ty,     font_px, outline);
        DrawText(e->text, tx + 1, ty,     font_px, outline);
        DrawText(e->text, tx,     ty - 1, font_px, outline);
        DrawText(e->text, tx,     ty + 1, font_px, outline);

        /* ── 3. Main colored text ────────────────────────────────────── */
        Color c = e->base_color;
        c.a = a_main;
        DrawText(e->text, tx, ty, font_px, c);
    }
}

void fct_draw_overlay(void) {
    /* Called outside BeginMode2D — draws full-screen tints in screen space. */
    for (int t = 0; t < FCT_TYPE_COUNT; t++) {
        if (s_overlay[t] <= 0.0f) continue;
        Color tint = FCT_TUNING[t].flash_color;
        tint.a = (unsigned char)(s_overlay[t] * 255.0f + 0.5f);
        DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), tint);
    }
}
