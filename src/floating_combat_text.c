/**
 * @file floating_combat_text.c
 * @brief Floating Combat Text — animated pop-up numbers for the Cyberia client.
 *
 * Follows the Fountain & Sink Economy model: every coin event (gain, loss, sink)
 * and every combat event (damage, regen) is surfaced to the player as a
 * short-lived animated number that rises, arcs, and fades from the event's
 * world position.
 *
 * Animation phases (total lifetime FCT_TOTAL_LIFETIME seconds):
 *   Phase 1 — Pop   [0, FCT_POP_DURATION):
 *       Font size scales from 40 % → 120 % with a brief overshoot, giving the
 *       number a punchy "pop" entrance.  Alpha ramps from 0.4 → 1.0.
 *
 *   Phase 2 — Rise  [FCT_POP_DURATION, FCT_FADE_START):
 *       Constant upward velocity (FCT_RISE_SPEED world-units/s) with a random
 *       horizontal drift (FCT_DRIFT_MIN … FCT_DRIFT_MAX).  Full alpha.
 *
 *   Phase 3 — Fade  [FCT_FADE_START, FCT_TOTAL_LIFETIME):
 *       Vertical velocity decelerates exponentially; horizontal drift reduces;
 *       alpha decays linearly to 0.
 *
 * Size:  font_px = clamp(FCT_FONT_SIZE_MIN + (FCT_FONT_SIZE_MAX - MIN) *
 *                        log2(value + 1) / 10.0, MIN, MAX)
 *        → small hits (1–10) get ~10–14 px; large hits (1000+) get ~24–26 px.
 *
 * The module has no external dependencies beyond game_state.h (for cell_size)
 * and Raylib (for DrawText/MeasureText).
 */

#include "floating_combat_text.h"
#include "game_state.h"
#include <raylib.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

/* ── Tuning constants ─────────────────────────────────────────────────── */

#define FCT_TOTAL_LIFETIME  1.8f   /* total seconds before entry is freed    */
#define FCT_POP_DURATION    0.12f  /* seconds for the pop-in scale animation  */
#define FCT_FADE_START      1.3f   /* second at which alpha decay begins      */
#define FCT_RISE_SPEED      1.6f   /* world units / second (upward)           */
#define FCT_DRIFT_MIN       0.20f  /* min absolute horizontal drift wu/s      */
#define FCT_DRIFT_MAX       0.65f  /* max absolute horizontal drift wu/s      */
#define FCT_FONT_SIZE_MIN   10     /* pixels for value = 1                    */
#define FCT_FONT_SIZE_MAX   26     /* pixels for value ≥ 1024                 */

/* ── Base colours (alpha is per-frame computed) ───────────────────────── */

static const Color s_color_damage    = {220,  50,  50, 255};  /* red    */
static const Color s_color_regen     = { 60, 210,  60, 255};  /* green  */
static const Color s_color_coin      = {255, 210,  40, 255};  /* yellow */
static const Color s_color_fallback  = {190, 190, 190, 255};  /* grey   */

/* ── Internal entry structure ─────────────────────────────────────────── */

typedef struct {
    float   x, y;         /* current world position (moves each frame)      */
    float   vx, vy;       /* velocity in world units / second               */
    float   age;          /* seconds elapsed since spawn                    */
    int     font_px;      /* font size in screen pixels (fixed at spawn)     */
    char    text[14];     /* formatted string: "+42", "-1337", etc.         */
    Color   base_color;   /* colour before alpha is applied                 */
    bool    active;
} FCTEntry;

static FCTEntry s_pool[FCT_MAX_ENTRIES];
static bool     s_init = false;

/* ── Deterministic LCG — avoids touching the global rand() state ──────── */

static uint32_t s_lcg = 0xBEEF1337u;

static float lcg_f01(void) {
    s_lcg = s_lcg * 1664525u + 1013904223u;
    return (float)(s_lcg >> 8) / (float)(1u << 24);
}

/* ── Public API ────────────────────────────────────────────────────────── */

void fct_init(void) {
    memset(s_pool, 0, sizeof(s_pool));
    s_init = true;
}

void fct_spawn(float world_x, float world_y, uint32_t value, uint8_t type) {
    if (!s_init) fct_init();

    /* Find a free slot; evict the oldest active entry if the pool is full. */
    FCTEntry *slot = NULL;
    float oldest_age = -1.0f;
    int   oldest_idx = 0;
    for (int i = 0; i < FCT_MAX_ENTRIES; i++) {
        if (!s_pool[i].active) { slot = &s_pool[i]; break; }
        if (s_pool[i].age > oldest_age) { oldest_age = s_pool[i].age; oldest_idx = i; }
    }
    if (!slot) slot = &s_pool[oldest_idx];

    /* ── Format text ───────────────────────────────────────────────── */
    if (type == FCT_TYPE_REGEN || type == FCT_TYPE_COIN_GAIN) {
        snprintf(slot->text, sizeof(slot->text), "+%u", value);
    } else {
        snprintf(slot->text, sizeof(slot->text), "-%u", value);
    }

    /* ── Font size: log₂ scale so 1 → ~10 px, 1024 → ~26 px ──────── */
    float log_v   = (value > 0) ? (float)log2((double)value + 1.0) : 1.0f;
    float size_f  = FCT_FONT_SIZE_MIN
                  + (float)(FCT_FONT_SIZE_MAX - FCT_FONT_SIZE_MIN)
                  * (log_v / 10.0f);   /* log2(1024) ≈ 10 → full range */
    if (size_f < (float)FCT_FONT_SIZE_MIN) size_f = (float)FCT_FONT_SIZE_MIN;
    if (size_f > (float)FCT_FONT_SIZE_MAX) size_f = (float)FCT_FONT_SIZE_MAX;
    slot->font_px = (int)(size_f + 0.5f);

    /* ── Colour ────────────────────────────────────────────────────── */
    switch (type) {
        case FCT_TYPE_DAMAGE:                   slot->base_color = s_color_damage;   break;
        case FCT_TYPE_REGEN:                    slot->base_color = s_color_regen;    break;
        case FCT_TYPE_COIN_GAIN:
        case FCT_TYPE_COIN_LOSS:                slot->base_color = s_color_coin;     break;
        default:                                slot->base_color = s_color_fallback; break;
    }

    /* ── Random horizontal drift (left or right) ───────────────────── */
    float drift = FCT_DRIFT_MIN + lcg_f01() * (FCT_DRIFT_MAX - FCT_DRIFT_MIN);
    if (lcg_f01() < 0.5f) drift = -drift;

    slot->x         = world_x;
    slot->y         = world_y;
    slot->vx        = drift;
    slot->vy        = -FCT_RISE_SPEED;
    slot->age       = 0.0f;
    slot->active    = true;
}

void fct_update(float dt) {
    for (int i = 0; i < FCT_MAX_ENTRIES; i++) {
        FCTEntry *e = &s_pool[i];
        if (!e->active) continue;

        e->age += dt;
        if (e->age >= FCT_TOTAL_LIFETIME) {
            e->active = false;
            continue;
        }

        /* During fade phase: decelerate velocity to create a floating stop. */
        if (e->age > FCT_FADE_START) {
            float decel = 1.0f - dt * 4.0f;
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

        /* ── Alpha ─────────────────────────────────────────────────── */
        float alpha;
        if (e->age < FCT_POP_DURATION) {
            /* Pop-in: ramp 0.4 → 1.0 */
            alpha = 0.4f + 0.6f * (e->age / FCT_POP_DURATION);
        } else if (e->age >= FCT_FADE_START) {
            /* Fade-out: 1.0 → 0.0 */
            float frac = (e->age - FCT_FADE_START) / (FCT_TOTAL_LIFETIME - FCT_FADE_START);
            alpha = 1.0f - frac;
        } else {
            alpha = 1.0f;
        }
        if (alpha < 0.0f) alpha = 0.0f;
        if (alpha > 1.0f) alpha = 1.0f;

        /* ── Pop scale: overshoot at 70 % of pop duration ─────────── */
        int font_px = e->font_px;
        if (e->age < FCT_POP_DURATION) {
            float t = e->age / FCT_POP_DURATION;
            /* ease-out-back: small overshoot then settle */
            float scale = (t < 0.7f)
                ? 0.4f + (t / 0.7f) * 0.8f          /* 0.4 → 1.2 */
                : 1.2f - ((t - 0.7f) / 0.3f) * 0.2f; /* 1.2 → 1.0 */
            font_px = (int)((float)e->font_px * scale + 0.5f);
            if (font_px < 4) font_px = 4;
        }

        /* ── Draw ──────────────────────────────────────────────────── */
        Color c   = e->base_color;
        c.a       = (unsigned char)(alpha * 255.0f + 0.5f);
        float px  = e->x * cell_size;
        float py  = e->y * cell_size;
        /* Centre the text horizontally over the origin position. */
        int   tw  = MeasureText(e->text, font_px);
        DrawText(e->text, (int)(px - tw * 0.5f), (int)py, font_px, c);
    }
}
