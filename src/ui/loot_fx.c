#include "ui/loot_fx.h"

#include "game_state.h"
#include "world_types.h"

#include <math.h>
#include <string.h>

/* ── Tuning ─────────────────────────────────────────────────────────────── */

#define LOOT_FX_MAX          64      /* concurrent vacuum flights               */
#define LOOT_DROP_MAX        128     /* concurrent in-world drop animations     */
#define LOOT_PARTICLE_MAX    512     /* pooled burst particles                  */

/* Stage 3 — vacuum flight (ground → player). The token pops larger the moment
 * it is taken, then vacuums in. */
#define VAC_DURATION         0.55f   /* seconds                                 */
#define VAC_EASE_POW         3.0f    /* >1 accelerates into the player          */
#define VAC_ARC_CELLS        1.6f    /* parabola peak height, grid cells        */
#define VAC_BASE_SIZE        1.35f   /* launch square side, grid cells (enlarged)*/
#define VAC_END_SHRINK       0.62f   /* size shed by arrival                     */
#define VAC_FADE_FROM        0.85f   /* progress at which fade begins            */

/* Stage 1 — spawn launch (corpse → cell). */
#define LAUNCH_ARC_CELLS     1.4f    /* main toss height                        */
#define LAUNCH_BOUNCE_CELLS  0.28f   /* settle-bounce height                    */
#define LAUNCH_BOUNCE_FROM   0.80f   /* progress at which the bounce starts     */

/* Stage 2 — idle float. The token hovers slightly above its cell so the sparks,
 * which emit from below its base and rise, superimpose over it from underneath. */
#define IDLE_FREQ            3.2f    /* rad/s bounce rate                       */
#define IDLE_AMP_CELLS       0.16f   /* bounce amplitude, grid cells            */
#define IDLE_LIFT_CELLS      0.14f   /* constant hover lift, grid cells (up)    */
#define EMIT_DOWN_FRAC       0.40f   /* spark origin below token center, ×dims_h */

/* Particles — tap-effect styled yellow squares (black-bordered). Two tiers:
 * a continuous ambient sparkle around the idle drop, and a big treasure burst
 * on collection. */
#define PART_GRAVITY         8.0f    /* cells/s^2                               */

/* Collection burst (fires when the drop is taken — larger + faster). */
#define BURST_COUNT          12      /* particles per collection                */
#define BURST_TTL_MIN        0.45f
#define BURST_TTL_MAX        0.85f
#define BURST_UP_MIN         3.6f    /* initial upward speed, cells/s           */
#define BURST_UP_MAX         7.2f
#define BURST_SPREAD         3.2f    /* horizontal speed spread, cells/s        */
#define BURST_SIZE_MIN       0.18f   /* grid cells (subtle squares)             */
#define BURST_SIZE_MAX       0.34f
#define BURST_EMIT_DOWN      0.5f    /* burst origin below the token, grid cells */

/* Ambient idle sparkle (drifts gently off the floating token). */
#define AMBIENT_INTERVAL_MIN 0.13f   /* seconds between emits, per drop          */
#define AMBIENT_INTERVAL_MAX 0.30f
#define AMBIENT_TTL_MIN      0.40f
#define AMBIENT_TTL_MAX      0.70f
#define AMBIENT_UP_MIN       1.1f
#define AMBIENT_UP_MAX       2.4f
#define AMBIENT_SPREAD       1.1f
#define AMBIENT_OFFSET       0.45f   /* spawn jitter around the token, grid cells */
#define AMBIENT_SIZE_MIN     0.18f   /* grid cells                              */
#define AMBIENT_SIZE_MAX     0.32f

#define DROP_ANIM_EVICT_SEC  1.5f    /* forget a drop unseen this long          */

/* ── State ──────────────────────────────────────────────────────────────── */

typedef struct {
    char  item_id[MAX_ITEM_ID_LENGTH];
    char  collector_id[MAX_ID_LENGTH];
    float start_x, start_y;
    float last_target_x, last_target_y;
    bool  have_target;
    float age;
    float cur_x, cur_y, size, alpha;
    bool  active;
} VacFlight;

typedef struct {
    char   drop_id[MAX_ID_LENGTH];
    float  origin_x, origin_y;   /* launch start (corpse center)              */
    float  landing_x, landing_y; /* launch end / idle anchor                  */
    double launch_start;         /* s_clock at launch                          */
    float  launch_dur;           /* seconds; 0 = spawn-launch unknown, idle-only */
    float  phase;                /* idle sine phase offset                     */
    double last_seen;            /* s_clock at last render query               */
    double next_emit;            /* s_clock at which the next ambient spark fires */
    bool   collected;            /* in-world render yields to vacuum flight    */
    bool   active;
} DropAnim;

typedef struct {
    float   x, y, vx, vy;
    float   age, ttl;
    float   size;
    uint8_t tint;
    bool    active;
} Particle;

static VacFlight s_flights[LOOT_FX_MAX];
static DropAnim  s_drops[LOOT_DROP_MAX];
static Particle  s_particles[LOOT_PARTICLE_MAX];

/* Presentation clock, advanced only by loot_fx_update — keeps launch/idle
 * timing independent of the raylib frame clock and of message arrival time. */
static double s_clock = 0.0;

/* Deterministic LCG — avoids perturbing the global rand() state. */
static uint32_t s_lcg = 0x51F00D5u;
static float lcg_f01(void) {
    s_lcg = s_lcg * 1664525u + 1013904223u;
    return (float)(s_lcg >> 8) / (float)(1u << 24);
}
static float lcg_range(float lo, float hi) { return lo + lcg_f01() * (hi - lo); }

/* Stable sine phase from a drop id so neighbouring drops bounce out of sync. */
static float id_phase(const char* id) {
    uint32_t h = 2166136261u;
    for (const char* p = id; *p; p++) { h ^= (uint8_t)*p; h *= 16777619u; }
    return (float)(h & 0xFFFFu) / 65535.0f * 6.2831853f;
}

static void spawn_ambient(float wx, float wy);

/* ── Lifecycle ──────────────────────────────────────────────────────────── */

void loot_fx_reset(void) {
    memset(s_flights, 0, sizeof(s_flights));
    memset(s_drops, 0, sizeof(s_drops));
    memset(s_particles, 0, sizeof(s_particles));
    s_clock = 0.0;
}

/* ── Drop animation registry (stages 1–2) ──────────────────────────────── */

static DropAnim* drop_find(const char* drop_id) {
    for (int i = 0; i < LOOT_DROP_MAX; i++) {
        if (s_drops[i].active && 0 == strcmp(s_drops[i].drop_id, drop_id))
            return &s_drops[i];
    }
    return NULL;
}

static DropAnim* drop_alloc(const char* drop_id) {
    DropAnim* slot = NULL;
    double oldest = 1e18;
    int    oldest_i = 0;
    for (int i = 0; i < LOOT_DROP_MAX; i++) {
        if (!s_drops[i].active) { slot = &s_drops[i]; break; }
        if (s_drops[i].last_seen < oldest) { oldest = s_drops[i].last_seen; oldest_i = i; }
    }
    if (!slot) slot = &s_drops[oldest_i];
    memset(slot, 0, sizeof(*slot));
    strncpy(slot->drop_id, drop_id, MAX_ID_LENGTH - 1);
    slot->phase     = id_phase(drop_id);
    slot->last_seen = s_clock;
    slot->active    = true;
    return slot;
}

void loot_fx_note_spawn(const char* drop_id, float origin_x, float origin_y,
                        float landing_x, float landing_y, const char* item_id,
                        uint16_t launch_ms) {
    if (!drop_id || drop_id[0] == '\0') return;

    DropAnim* a = drop_find(drop_id);
    if (!a) a = drop_alloc(drop_id);

    a->origin_x     = origin_x;
    a->origin_y     = origin_y;
    a->landing_x    = landing_x;
    a->landing_y    = landing_y;
    a->launch_start = s_clock;
    a->launch_dur   = (float)launch_ms / 1000.0f;
    a->collected    = false;
    a->last_seen    = s_clock;

    (void)item_id; /* item texture comes from the AOI drop bot itself */
}

bool loot_fx_drop_render_pos(const char* drop_id,
                             float land_topx, float land_topy,
                             float dims_w, float dims_h,
                             float* out_topx, float* out_topy) {
    if (!out_topx || !out_topy) return false;

    float land_cx = land_topx + dims_w * 0.5f;
    float land_cy = land_topy + dims_h * 0.5f;

    DropAnim* a = drop_find(drop_id);
    if (!a) {
        /* Spawn event never arrived (or already forgotten): idle-float in place
         * anchored to the authoritative AOI cell. */
        a = drop_alloc(drop_id);
        a->origin_x = a->landing_x = land_cx;
        a->origin_y = a->landing_y = land_cy;
        a->launch_dur = 0.0f;
    }
    a->last_seen = s_clock;

    if (a->collected) return false;

    /* Keep the idle anchor pinned to the live authoritative cell. */
    a->landing_x = land_cx;
    a->landing_y = land_cy;

    float cx, cy;
    float elapsed = (float)(s_clock - a->launch_start);

    if (a->launch_dur > 0.0f && elapsed < a->launch_dur) {
        /* Stage 1 — swift parabolic launch with a settle bounce. */
        float t  = elapsed / a->launch_dur;
        if (t < 0.0f) t = 0.0f;
        float te = 1.0f - (1.0f - t) * (1.0f - t);   /* ease-out horizontal */
        cx = a->origin_x + (a->landing_x - a->origin_x) * te;
        cy = a->origin_y + (a->landing_y - a->origin_y) * te;

        float arc = 4.0f * LAUNCH_ARC_CELLS * t * (1.0f - t);
        float bounce = 0.0f;
        if (t > LAUNCH_BOUNCE_FROM) {
            float bt = (t - LAUNCH_BOUNCE_FROM) / (1.0f - LAUNCH_BOUNCE_FROM);
            bounce = sinf(bt * 3.1415927f) * LAUNCH_BOUNCE_CELLS * (1.0f - bt);
        }
        cy -= (arc + bounce);
    } else {
        /* Stage 2 — continuous idle float, hovering slightly above the cell. */
        cx = a->landing_x;
        cy = a->landing_y - IDLE_LIFT_CELLS
             - sinf((float)s_clock * IDLE_FREQ + a->phase) * IDLE_AMP_CELLS;

        /* Ambient sparks emit from below the token base and rise up over it. */
        if (s_clock >= a->next_emit) {
            spawn_ambient(cx, cy + dims_h * EMIT_DOWN_FRAC);
            a->next_emit = s_clock + lcg_range(AMBIENT_INTERVAL_MIN, AMBIENT_INTERVAL_MAX);
        }
    }

    *out_topx = cx - dims_w * 0.5f;
    *out_topy = cy - dims_h * 0.5f;
    return true;
}

/* ── Particle burst (stage 3) ──────────────────────────────────────────── */

static Particle* particle_alloc(void) {
    for (int i = 0; i < LOOT_PARTICLE_MAX; i++) {
        if (!s_particles[i].active) return &s_particles[i];
    }
    return NULL;
}

/* Big treasure burst fired the moment the drop is taken. */
static void spawn_burst(float wx, float wy) {
    for (int n = 0; n < BURST_COUNT; n++) {
        Particle* p = particle_alloc();
        if (!p) return;
        p->x    = wx;
        p->y    = wy;
        p->vx   = lcg_range(-BURST_SPREAD, BURST_SPREAD);
        p->vy   = -lcg_range(BURST_UP_MIN, BURST_UP_MAX);
        p->age  = 0.0f;
        p->ttl  = lcg_range(BURST_TTL_MIN, BURST_TTL_MAX);
        p->size = lcg_range(BURST_SIZE_MIN, BURST_SIZE_MAX);
        p->tint = (uint8_t)(n & 1);   /* two yellow shades */
        p->active = true;
    }
}

/* One gentle ambient spark drifting off the idle floating token. */
static void spawn_ambient(float wx, float wy) {
    Particle* p = particle_alloc();
    if (!p) return;
    p->x    = wx + lcg_range(-AMBIENT_OFFSET, AMBIENT_OFFSET);
    p->y    = wy + lcg_range(-AMBIENT_OFFSET, AMBIENT_OFFSET);
    p->vx   = lcg_range(-AMBIENT_SPREAD, AMBIENT_SPREAD);
    p->vy   = -lcg_range(AMBIENT_UP_MIN, AMBIENT_UP_MAX);
    p->age  = 0.0f;
    p->ttl  = lcg_range(AMBIENT_TTL_MIN, AMBIENT_TTL_MAX);
    p->size = lcg_range(AMBIENT_SIZE_MIN, AMBIENT_SIZE_MAX);
    p->tint = (uint8_t)(s_lcg & 1u);
    p->active = true;
}

/* ── Vacuum flight (stage 3) ───────────────────────────────────────────── */

void loot_fx_push(const char* drop_id, const char* collector_id,
                  const char* item_id, float world_x, float world_y) {
    if (!item_id || item_id[0] == '\0') return;

    /* Stop the in-world idle render for this token. */
    if (drop_id && drop_id[0] != '\0') {
        DropAnim* a = drop_find(drop_id);
        if (a) a->collected = true;
    }

    /* Treasure burst rising from below the token's ground location. */
    spawn_burst(world_x, world_y + BURST_EMIT_DOWN);

    /* Arm the detached vacuum flight. */
    VacFlight* slot = NULL;
    float oldest = -1.0f;
    int   oldest_i = 0;
    for (int i = 0; i < LOOT_FX_MAX; i++) {
        if (!s_flights[i].active) { slot = &s_flights[i]; break; }
        if (s_flights[i].age > oldest) { oldest = s_flights[i].age; oldest_i = i; }
    }
    if (!slot) slot = &s_flights[oldest_i];

    memset(slot, 0, sizeof(*slot));
    strncpy(slot->item_id, item_id, MAX_ITEM_ID_LENGTH - 1);
    if (collector_id) strncpy(slot->collector_id, collector_id, MAX_ID_LENGTH - 1);
    slot->start_x = world_x;
    slot->start_y = world_y;
    slot->cur_x   = world_x;
    slot->cur_y   = world_y;
    slot->size    = VAC_BASE_SIZE;
    slot->alpha   = 1.0f;
    slot->active  = true;
}

static bool resolve_collector_center(const char* collector_id, float* cx, float* cy) {
    const GameState* gs = &g_game_state;

    if (collector_id[0] != '\0' && 0 == strcmp(collector_id, gs->player_id)) {
        *cx = gs->player.base.interp_pos.x + gs->player.base.dims.x * 0.5f;
        *cy = gs->player.base.interp_pos.y + gs->player.base.dims.y * 0.5f;
        return true;
    }
    for (int i = 0; i < gs->other_player_count; i++) {
        const PlayerState* p = &gs->other_players[i];
        if (0 == strcmp(p->base.id, collector_id)) {
            *cx = p->base.interp_pos.x + p->base.dims.x * 0.5f;
            *cy = p->base.interp_pos.y + p->base.dims.y * 0.5f;
            return true;
        }
    }
    return false;
}

/* ── Per-frame integration ─────────────────────────────────────────────── */

void loot_fx_update(float dt) {
    s_clock += dt;

    /* Vacuum flights. */
    for (int i = 0; i < LOOT_FX_MAX; i++) {
        VacFlight* f = &s_flights[i];
        if (!f->active) continue;

        f->age += dt;
        float t = f->age / VAC_DURATION;
        if (t >= 1.0f) { f->active = false; continue; }

        float eased = powf(t, VAC_EASE_POW);

        float tx, ty;
        if (resolve_collector_center(f->collector_id, &tx, &ty)) {
            f->last_target_x = tx;
            f->last_target_y = ty;
            f->have_target   = true;
        } else if (f->have_target) {
            tx = f->last_target_x;
            ty = f->last_target_y;
        } else {
            tx = f->start_x;
            ty = f->start_y;
        }

        f->cur_x = f->start_x + (tx - f->start_x) * eased;
        f->cur_y = f->start_y + (ty - f->start_y) * eased;
        f->cur_y -= 4.0f * VAC_ARC_CELLS * t * (1.0f - t);
        f->size  = VAC_BASE_SIZE * (1.0f - VAC_END_SHRINK * eased);
        f->alpha = (t < VAC_FADE_FROM)
                   ? 1.0f
                   : 1.0f - (t - VAC_FADE_FROM) / (1.0f - VAC_FADE_FROM);
        if (f->alpha < 0.0f) f->alpha = 0.0f;
    }

    /* Particles. */
    for (int i = 0; i < LOOT_PARTICLE_MAX; i++) {
        Particle* p = &s_particles[i];
        if (!p->active) continue;
        p->age += dt;
        if (p->age >= p->ttl) { p->active = false; continue; }
        p->vy += PART_GRAVITY * dt;
        p->x  += p->vx * dt;
        p->y  += p->vy * dt;
    }

    /* Evict drop animations for tokens no longer being rendered. */
    for (int i = 0; i < LOOT_DROP_MAX; i++) {
        DropAnim* a = &s_drops[i];
        if (!a->active) continue;
        if ((s_clock - a->last_seen) > DROP_ANIM_EVICT_SEC) a->active = false;
    }
}

/* ── Renderer bridges ──────────────────────────────────────────────────── */

int loot_fx_slot_count(void) { return LOOT_FX_MAX; }

bool loot_fx_render_at(int i, LootFxRender* out) {
    if (i < 0 || i >= LOOT_FX_MAX || !out) return false;
    const VacFlight* f = &s_flights[i];
    if (!f->active) return false;

    strncpy(out->item_id, f->item_id, MAX_ITEM_ID_LENGTH - 1);
    out->item_id[MAX_ITEM_ID_LENGTH - 1] = '\0';
    out->x     = f->cur_x;
    out->y     = f->cur_y;
    out->size  = f->size;
    out->alpha = f->alpha;
    return true;
}

int loot_fx_particle_slot_count(void) { return LOOT_PARTICLE_MAX; }

bool loot_fx_particle_at(int i, LootFxParticle* out) {
    if (i < 0 || i >= LOOT_PARTICLE_MAX || !out) return false;
    const Particle* p = &s_particles[i];
    if (!p->active) return false;

    out->x     = p->x;
    out->y     = p->y;
    out->size  = p->size;
    out->tint  = p->tint;
    out->alpha = 1.0f - p->age / p->ttl;
    if (out->alpha < 0.0f) out->alpha = 0.0f;
    return true;
}
