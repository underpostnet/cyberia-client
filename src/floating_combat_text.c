/**
 * @file floating_combat_text.c
 * @brief Floating Combat Text — animated pop-up numbers for the Cyberia client.
 *
 * Surfaces every combat and economy event as a short-lived animated number
 * that pops, rises, and fades from the event's world position.
 *
 * Animation phases (FCT_TOTAL_LIFETIME seconds total):
 *   Phase 1 — Pop   [0, FCT_POP_DURATION):
 *       Font blasts from 40% → pop_overshoot (1.20–1.70×, scaled with value)
 *       then snaps back to 1.0. Alpha ramps 0.2 → 1.0.
 *
 *   Phase 2 — Rise  [FCT_POP_DURATION, FCT_FADE_START):
 *       Upward velocity (per-type FCT_RISE_* world-units/s) plus random
 *       horizontal drift.  Damage: fast and wide.  Regen: gentle.  Coins: medium.
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
 * Per-type visual tuning:
 *   FCT_TYPE_DAMAGE    red   {255,60,60}   rise 3.8  drift ±1.8  font 14–44  overshoot ≤1.70×
 *   FCT_TYPE_REGEN     green {80,240,80}   rise 2.2  drift ±0.5  font 14–32  overshoot ≤1.45×
 *   FCT_TYPE_COIN_GAIN gold  {255,215,0}   rise 2.6  drift ±1.2  font 14–36  overshoot ≤1.50×
 *   FCT_TYPE_COIN_LOSS amber {255,130,0}   rise 2.6  drift ±1.2  font 14–36  overshoot ≤1.50×
 */

#include "floating_combat_text.h"
#include "game_state.h"
#include <raylib.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

/* ── Global timing ─────────────────────────────────────────────────────── */

#define FCT_TOTAL_LIFETIME   2.2f   /* total seconds before entry is freed   */
#define FCT_POP_DURATION     0.09f  /* violent snap pop-in                   */
#define FCT_FADE_START       1.6f   /* second at which alpha decay begins    */

/* ── Per-type rise speeds (world units/second, upward) ────────────────── */

#define FCT_RISE_DAMAGE      3.8f
#define FCT_RISE_REGEN       2.2f
#define FCT_RISE_COIN        2.6f

/* ── Per-type max horizontal drift (world units/second) ───────────────── */

#define FCT_DRIFT_DAMAGE     1.8f
#define FCT_DRIFT_REGEN      0.5f
#define FCT_DRIFT_COIN       1.2f
#define FCT_DRIFT_MIN        0.2f   /* minimum absolute drift for all types  */

/* ── Per-type font sizing ──────────────────────────────────────────────── */

#define FCT_FONT_MIN         14     /* minimum pixels (all types)            */
#define FCT_FONT_MAX_DAMAGE  44     /* upper limit for damage hits           */
#define FCT_FONT_MAX_REGEN   32     /* upper limit for regen pulses          */
#define FCT_FONT_MAX_COIN    36     /* upper limit for coin events           */

/* Log-base divisor: log2(value+1)/div → 1.0 at the "saturating" value.
 * Smaller = font maxes out at lower hit values (more aggressive growth).  */
#define FCT_LOG_DIV_DAMAGE   5.0f   /* log2(32)  ≈ 5  → hit   33 = full sz */
#define FCT_LOG_DIV_REGEN    7.0f   /* log2(128) ≈ 7  → regen 129 = full   */
#define FCT_LOG_DIV_COIN     6.0f   /* log2(64)  ≈ 6  → coins  65 = full   */

/* ── Screen-overlay vignette ───────────────────────────────────────────── */

#define FCT_OVERLAY_DMG_ALPHA  0.28f  /* peak red-flash alpha               */
#define FCT_OVERLAY_DMG_DECAY  2.20f  /* alpha units lost per second        */
#define FCT_OVERLAY_RGN_ALPHA  0.14f  /* peak green-pulse alpha             */
#define FCT_OVERLAY_RGN_DECAY  1.80f  /* alpha units lost per second        */

/* ── Base colours ──────────────────────────────────────────────────────── */

static const Color s_color_damage    = {255,  60,  60, 255};  /* bright red   */
static const Color s_color_regen     = { 80, 240,  80, 255};  /* bright green */
static const Color s_color_coin_gain = {255, 215,   0, 255};  /* bright gold  */
static const Color s_color_coin_loss = {255, 130,   0, 255};  /* burnt orange */
static const Color s_color_item_gain = { 80, 220, 255, 255};  /* cyan         */
static const Color s_color_item_loss = {180,  80, 255, 255};  /* purple       */
static const Color s_color_fallback  = {190, 190, 190, 255};  /* grey         */

/* ── Internal entry ────────────────────────────────────────────────────── */

typedef struct {
    float   x, y;                    /* current world position (moves each frame)    */
    float   vx, vy;                  /* velocity in world units / second             */
    float   age;                     /* seconds elapsed since spawn                  */
    int     font_px;                 /* base font size in pixels (fixed at spawn)    */
    float   pop_overshoot;           /* peak scale during pop-in (type + value)      */
    char    text[32];                /* formatted string: "+42 wood", "-1337", etc.  */
    Color   base_color;              /* colour before alpha is applied               */
    uint8_t type;                    /* FCT_TYPE_* — for draw-time differentiation   */
    bool    active;
} FCTEntry;

static FCTEntry s_pool[FCT_MAX_ENTRIES];
static bool     s_init = false;

/* Screen-overlay alphas: updated by fct_spawn/fct_update, read by fct_draw_overlay. */
static float s_damage_overlay = 0.0f;
static float s_regen_overlay  = 0.0f;

/* ── Deterministic LCG — avoids touching the global rand() state ────────── */

static uint32_t s_lcg = 0xBEEF1337u;

static float lcg_f01(void) {
    s_lcg = s_lcg * 1664525u + 1013904223u;
    return (float)(s_lcg >> 8) / (float)(1u << 24);
}

/* ── Public API ─────────────────────────────────────────────────────────── */

void fct_init(void) {
    memset(s_pool, 0, sizeof(s_pool));
    s_damage_overlay = 0.0f;
    s_regen_overlay  = 0.0f;
    s_init = true;
}

void fct_spawn(float world_x, float world_y, uint32_t value, uint8_t type) {
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

    /* ── Format text ─────────────────────────────────────────────────── */
    if (type == FCT_TYPE_REGEN || type == FCT_TYPE_COIN_GAIN)
        snprintf(slot->text, sizeof(slot->text), "+%u", value);
    else
        snprintf(slot->text, sizeof(slot->text), "-%u", value);

    /* ── Per-type tuning ──────────────────────────────────────────────── */
    float rise_speed, drift_max, log_div, base_overshoot;
    int   font_max;
    Color base_color;

    switch (type) {
        case FCT_TYPE_DAMAGE:
            rise_speed     = FCT_RISE_DAMAGE;
            drift_max      = FCT_DRIFT_DAMAGE;
            log_div        = FCT_LOG_DIV_DAMAGE;
            font_max       = FCT_FONT_MAX_DAMAGE;
            base_color     = s_color_damage;
            base_overshoot = 1.45f;
            s_damage_overlay = FCT_OVERLAY_DMG_ALPHA;  /* trigger screen flash */
            break;
        case FCT_TYPE_REGEN:
            rise_speed     = FCT_RISE_REGEN;
            drift_max      = FCT_DRIFT_REGEN;
            log_div        = FCT_LOG_DIV_REGEN;
            font_max       = FCT_FONT_MAX_REGEN;
            base_color     = s_color_regen;
            base_overshoot = 1.20f;
            s_regen_overlay = FCT_OVERLAY_RGN_ALPHA;   /* trigger screen pulse */
            break;
        case FCT_TYPE_COIN_GAIN:
            rise_speed     = FCT_RISE_COIN;
            drift_max      = FCT_DRIFT_COIN;
            log_div        = FCT_LOG_DIV_COIN;
            font_max       = FCT_FONT_MAX_COIN;
            base_color     = s_color_coin_gain;
            base_overshoot = 1.25f;
            break;
        case FCT_TYPE_COIN_LOSS:
            rise_speed     = FCT_RISE_COIN;
            drift_max      = FCT_DRIFT_COIN;
            log_div        = FCT_LOG_DIV_COIN;
            font_max       = FCT_FONT_MAX_COIN;
            base_color     = s_color_coin_loss;
            base_overshoot = 1.25f;
            break;
        case FCT_TYPE_ITEM_GAIN:
            rise_speed     = FCT_RISE_COIN;
            drift_max      = FCT_DRIFT_COIN;
            log_div        = FCT_LOG_DIV_COIN;
            font_max       = FCT_FONT_MAX_COIN;
            base_color     = s_color_item_gain;
            base_overshoot = 1.20f;
            break;
        case FCT_TYPE_ITEM_LOSS:
            rise_speed     = FCT_RISE_COIN;
            drift_max      = FCT_DRIFT_COIN;
            log_div        = FCT_LOG_DIV_COIN;
            font_max       = FCT_FONT_MAX_COIN;
            base_color     = s_color_item_loss;
            base_overshoot = 1.20f;
            break;
        default:
            rise_speed     = FCT_RISE_COIN;
            drift_max      = FCT_DRIFT_COIN;
            log_div        = FCT_LOG_DIV_COIN;
            font_max       = FCT_FONT_MAX_COIN;
            base_color     = s_color_fallback;
            base_overshoot = 1.20f;
            break;
    }

    /* ── Font size — log₂ scale, per-type grow rate ─────────────────── */
    float log_v  = (value > 0) ? (float)log2((double)value + 1.0) : 1.0f;
    float size_f = (float)FCT_FONT_MIN
                 + (float)(font_max - FCT_FONT_MIN) * (log_v / log_div);
    if (size_f < (float)FCT_FONT_MIN) size_f = (float)FCT_FONT_MIN;
    if (size_f > (float)font_max)     size_f = (float)font_max;
    slot->font_px = (int)(size_f + 0.5f);

    /* ── Pop overshoot — scales with hit magnitude ────────────────────── */
    float t_norm = log_v / log_div;
    if (t_norm > 1.0f) t_norm = 1.0f;
    slot->pop_overshoot = base_overshoot + 0.25f * t_norm;

    /* ── Colour / type ────────────────────────────────────────────────── */
    slot->base_color = base_color;
    slot->type       = type;

    /* ── Velocity — random drift direction ────────────────────────────── */
    float drift = FCT_DRIFT_MIN + lcg_f01() * (drift_max - FCT_DRIFT_MIN);
    if (lcg_f01() < 0.5f) drift = -drift;

    slot->x      = world_x;
    slot->y      = world_y;
    slot->vx     = drift;
    slot->vy     = -rise_speed;
    slot->age    = 0.0f;
    slot->active = true;
}

// fct_spawn_item spawns a labeled FCT for item quantity changes.
// The label is formatted as "+N itemId" or "-N itemId".
void fct_spawn_item(float world_x, float world_y, uint32_t quantity,
                    uint8_t type, const char* item_id) {
    if (!s_init) fct_init();
    if (quantity == 0) return;

    /* Reuse fct_spawn's slot-finding logic by calling it directly, then
     * patch the text with the item label. Use ITEM_GAIN/LOSS types so the
     * colour switch in fct_spawn picks the right palette. */
    fct_spawn(world_x, world_y, quantity, type);

    /* fct_spawn wrote to the most recently activated slot — find it. */
    FCTEntry* slot = NULL;
    float latest = -1.0f;
    for (int i = 0; i < FCT_MAX_ENTRIES; i++) {
        if (s_pool[i].active && s_pool[i].age <= 0.001f) {
            if (s_pool[i].age >= latest) {
                latest = s_pool[i].age;
                slot = &s_pool[i];
            }
        }
    }
    if (!slot) return;

    /* Overwrite text with labeled form: "+45 wood" or "-30 stone". */
    if (item_id && item_id[0] != '\0') {
        const char* sign = (type == FCT_TYPE_ITEM_GAIN) ? "+" : "-";
        snprintf(slot->text, sizeof(slot->text), "%s%u %s", sign, quantity, item_id);
    }
}

void fct_update(float dt) {
    /* Decay screen-space overlays. */
    if (s_damage_overlay > 0.0f) {
        s_damage_overlay -= dt * FCT_OVERLAY_DMG_DECAY;
        if (s_damage_overlay < 0.0f) s_damage_overlay = 0.0f;
    }
    if (s_regen_overlay > 0.0f) {
        s_regen_overlay -= dt * FCT_OVERLAY_RGN_DECAY;
        if (s_regen_overlay < 0.0f) s_regen_overlay = 0.0f;
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
    float cell_size = (g_game_state.cell_size > 0.0f) ? g_game_state.cell_size : 12.0f;

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
    /* Called outside BeginMode2D — draws full-screen tints in screen space.
     * Damage: brief red vignette.  Regen: brief green pulse.            */
    if (s_damage_overlay > 0.0f) {
        unsigned char a = (unsigned char)(s_damage_overlay * 255.0f + 0.5f);
        DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(),
                      (Color){180, 0, 0, a});
    }
    if (s_regen_overlay > 0.0f) {
        unsigned char a = (unsigned char)(s_regen_overlay * 255.0f + 0.5f);
        DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(),
                      (Color){0, 180, 0, a});
    }
}
