#include "ui/loot_fx.h"

#include "game_render.h"
#include "game_state.h"
#include "ui/fx_inventory_bar_qty.h"
#include "ui/inventory_bar.h"
#include "world_types.h"

#include <math.h>
#include <raylib.h>
#include <string.h>

/* ── Tuning ─────────────────────────────────────────────────────────────── */

#define LOOT_FX_MAX          64      /* concurrent vacuum flights               */
#define LOOT_DROP_MAX        128     /* concurrent in-world drop animations     */
#define LOOT_PARTICLE_MAX    512     /* pooled world-space burst particles      */
#define LOOT_SCR_PARTICLE_MAX 256    /* pooled screen-space delivery particles  */
#define LOOT_PENDING_MAX     32      /* scheduled slot-arrival bursts           */

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

/* Screen-space slot delivery — after the world vacuum lands on the local
 * player, particles stream (homing) into the item's inventory slot, then burst
 * on arrival. All in screen pixels. */
#define DELIVER_COUNT        9
#define DELIVER_TTL          0.62f   /* seconds, avatar → slot (slow enough to read)*/
#define DELIVER_ARC_PX       130.0f  /* parabola peak height, screen px           */
#define DELIVER_START_SPREAD 24.0f   /* px jitter around the avatar origin        */
#define DELIVER_TARGET_JIT   7.0f    /* px jitter around the slot center          */
#define DELIVER_SIZE_MIN     6.0f    /* px squares                                */
#define DELIVER_SIZE_MAX     12.0f

/* Screen-space slot expend — the reverse of delivery. Large particles + the item
 * icon fly OUT of the slot to random on-screen points along the same parabola,
 * slow enough to read clearly. */
#define EXPEND_COUNT         11
#define EXPEND_TTL           1.15f   /* seconds, slot → outward (slow, legible)   */
#define EXPEND_DIST_MIN      120.0f  /* px travelled from the slot                */
#define EXPEND_DIST_MAX      280.0f
#define EXPEND_SIZE_MIN      12.0f   /* visible squares, a touch smaller          */
#define EXPEND_SIZE_MAX      22.0f
#define EXPEND_EDGE_PAD      10.0f   /* keep targets this far inside the screen   */

/* Delivery token — the item icon riding the stream, above the particles. */
#define TOKEN_MAX            16      /* concurrent delivery tokens               */
#define TOKEN_SIZE_START     36.0f   /* px at launch                             */
#define TOKEN_SIZE_END       22.0f   /* px on slot arrival                       */
#define TOKEN_LIFT_PX        16.0f   /* rides this far above the particle arc    */
#define TOKEN_FADE_FROM      0.85f   /* progress at which the token fades out    */
#define ARRIVAL_COUNT        11
#define ARRIVAL_TTL          0.44f
#define ARRIVAL_SPEED_MIN    55.0f   /* px/s                                      */
#define ARRIVAL_SPEED_MAX    150.0f
#define ARRIVAL_UP_BIAS      45.0f   /* px/s upward bias                          */
#define ARRIVAL_GRAVITY      240.0f  /* px/s^2                                    */
#define ARRIVAL_SIZE_MIN     8.0f
#define ARRIVAL_SIZE_MAX     16.0f

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
    bool   eligible;             /* local player may collect (AOI per-viewer flag) */
    bool   active;
} DropAnim;

typedef struct {
    float   x, y, vx, vy;
    float   age, ttl;
    float   size;
    uint8_t tint;
    bool    active;
} Particle;

/* Screen-space particle: homing (avatar → slot) or ballistic burst (at slot). */
typedef struct {
    uint8_t mode;          /* 0 = homing, 1 = burst                            */
    float   x, y;          /* current screen px                                */
    float   sx, sy, tx, ty;/* homing endpoints                                 */
    float   vx, vy;        /* burst velocity, px/s                             */
    float   age, ttl;
    float   size;          /* px                                              */
    uint8_t tint;
    bool    active;
} ScrParticle;

/* A slot-arrival burst scheduled to fire when a delivery stream lands. */
typedef struct {
    float  tx, ty;
    char   item_id[MAX_ITEM_ID_LENGTH];
    double fire_at;
    bool   active;
} PendingArrival;

/* The item icon flying with a delivery stream (screen space). */
typedef struct {
    char  item_id[MAX_ITEM_ID_LENGTH];
    float sx, sy, tx, ty;
    float age, ttl;
    bool  active;
} DeliveryToken;

static VacFlight      s_flights[LOOT_FX_MAX];
static DropAnim       s_drops[LOOT_DROP_MAX];
static Particle       s_particles[LOOT_PARTICLE_MAX];
static ScrParticle    s_scr[LOOT_SCR_PARTICLE_MAX];
static PendingArrival s_pending[LOOT_PENDING_MAX];
static DeliveryToken  s_tokens[TOKEN_MAX];

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

static void spawn_ambient(float wx, float wy, uint8_t tint);

/* ── Lifecycle ──────────────────────────────────────────────────────────── */

void loot_fx_reset(void) {
    memset(s_flights, 0, sizeof(s_flights));
    memset(s_drops, 0, sizeof(s_drops));
    memset(s_particles, 0, sizeof(s_particles));
    memset(s_scr, 0, sizeof(s_scr));
    memset(s_pending, 0, sizeof(s_pending));
    memset(s_tokens, 0, sizeof(s_tokens));
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
                             bool loot_eligible,
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
    /* Pinned to the live AOI value — eligibility is personal per viewer and
     * can arrive after the spawn event. */
    a->eligible = loot_eligible;

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
            spawn_ambient(cx, cy + dims_h * EMIT_DOWN_FRAC,
                          a->eligible ? LOOT_FX_TINT_GOLD : LOOT_FX_TINT_GRAY);
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
static void spawn_burst(float wx, float wy, uint8_t tint) {
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
        p->tint = tint;
        p->active = true;
    }
}

/* One gentle ambient spark drifting off the idle floating token. */
static void spawn_ambient(float wx, float wy, uint8_t tint) {
    Particle* p = particle_alloc();
    if (!p) return;
    p->x    = wx + lcg_range(-AMBIENT_OFFSET, AMBIENT_OFFSET);
    p->y    = wy + lcg_range(-AMBIENT_OFFSET, AMBIENT_OFFSET);
    p->vx   = lcg_range(-AMBIENT_SPREAD, AMBIENT_SPREAD);
    p->vy   = -lcg_range(AMBIENT_UP_MIN, AMBIENT_UP_MAX);
    p->age  = 0.0f;
    p->ttl  = lcg_range(AMBIENT_TTL_MIN, AMBIENT_TTL_MAX);
    p->size = lcg_range(AMBIENT_SIZE_MIN, AMBIENT_SIZE_MAX);
    p->tint = tint;
    p->active = true;
}

/* ── Screen-space slot delivery (stage 4) ──────────────────────────────── */

static ScrParticle* scr_alloc(void) {
    for (int i = 0; i < LOOT_SCR_PARTICLE_MAX; i++) {
        if (!s_scr[i].active) return &s_scr[i];
    }
    return NULL;
}

/* Burst of screen particles popping outward at the destination slot. */
static void spawn_slot_arrival(float sx, float sy) {
    for (int n = 0; n < ARRIVAL_COUNT; n++) {
        ScrParticle* p = scr_alloc();
        if (!p) return;
        float ang = lcg_range(0.0f, 6.2831853f);
        float spd = lcg_range(ARRIVAL_SPEED_MIN, ARRIVAL_SPEED_MAX);
        *p = (ScrParticle){
            .mode   = 1,
            .x      = sx,
            .y      = sy,
            .vx     = cosf(ang) * spd,
            .vy     = sinf(ang) * spd - ARRIVAL_UP_BIAS,
            .ttl    = ARRIVAL_TTL * lcg_range(0.8f, 1.2f),
            .size   = lcg_range(ARRIVAL_SIZE_MIN, ARRIVAL_SIZE_MAX),
            .tint   = LOOT_FX_TINT_GOLD,
            .active = true,
        };
    }
}

/* The item icon riding a stream, drawn above the particles. */
static void spawn_delivery_token(float from_x, float from_y, float to_x, float to_y,
                                 const char* item_id, float ttl) {
    DeliveryToken* t = NULL;
    for (int i = 0; i < TOKEN_MAX; i++) {
        if (!s_tokens[i].active) { t = &s_tokens[i]; break; }
    }
    if (!t) return;
    *t = (DeliveryToken){ .sx = from_x, .sy = from_y, .tx = to_x, .ty = to_y,
                          .ttl = ttl, .active = true };
    if (item_id) strncpy(t->item_id, item_id, MAX_ITEM_ID_LENGTH - 1);
}

/* Stream of homing screen particles from the avatar into the slot, plus a
 * scheduled arrival burst when they land. */
static void spawn_slot_delivery(float from_x, float from_y, float to_x, float to_y,
                                const char* item_id) {
    spawn_delivery_token(from_x, from_y, to_x, to_y, item_id, DELIVER_TTL);
    for (int n = 0; n < DELIVER_COUNT; n++) {
        ScrParticle* p = scr_alloc();
        if (!p) break;
        float sx = from_x + lcg_range(-DELIVER_START_SPREAD, DELIVER_START_SPREAD);
        float sy = from_y + lcg_range(-DELIVER_START_SPREAD, DELIVER_START_SPREAD);
        *p = (ScrParticle){
            .mode   = 0,
            .x      = sx,
            .y      = sy,
            .sx     = sx,
            .sy     = sy,
            .tx     = to_x + lcg_range(-DELIVER_TARGET_JIT, DELIVER_TARGET_JIT),
            .ty     = to_y + lcg_range(-DELIVER_TARGET_JIT, DELIVER_TARGET_JIT),
            .ttl    = DELIVER_TTL * lcg_range(0.85f, 1.15f),
            .size   = lcg_range(DELIVER_SIZE_MIN, DELIVER_SIZE_MAX),
            .tint   = LOOT_FX_TINT_GOLD,
            .active = true,
        };
    }

    for (int i = 0; i < LOOT_PENDING_MAX; i++) {
        if (!s_pending[i].active) {
            s_pending[i] = (PendingArrival){
                .tx = to_x, .ty = to_y,
                .fire_at = s_clock + DELIVER_TTL,
                .active = true,
            };
            if (item_id) strncpy(s_pending[i].item_id, item_id, MAX_ITEM_ID_LENGTH - 1);
            break;
        }
    }
}

/* On a local-player vacuum landing, hand off to the inventory slot: convert the
 * avatar's world position to screen, resolve the item's slot, and launch the
 * delivery stream. No-op for remote collectors (no local inventory shown). */
static void trigger_slot_delivery(const VacFlight* f) {
    if (0 != strcmp(f->collector_id, g_game_state.player_id)) return;

    Vector2 from = game_render_world_to_screen((Vector2){ f->cur_x, f->cur_y });
    Vector2 slot;
    if (!inventory_bar_item_slot_center(f->item_id, &slot)) return;
    spawn_slot_delivery(from.x, from.y, slot.x, slot.y, f->item_id);
}

void loot_fx_reward_delivery(const char* item_id, float from_x, float from_y) {
    if (!item_id || item_id[0] == '\0') return;
    Vector2 slot;
    if (!inventory_bar_item_slot_center(item_id, &slot)) return;
    spawn_slot_delivery(from_x, from_y, slot.x, slot.y, item_id);
}

/* A random on-screen point EXPEND_DIST away from the slot, clamped inside. */
static void expend_target(float from_x, float from_y, float* tx, float* ty) {
    float ang  = lcg_range(0.0f, 6.2831853f);
    float dist = lcg_range(EXPEND_DIST_MIN, EXPEND_DIST_MAX);
    float x = from_x + cosf(ang) * dist;
    float y = from_y + sinf(ang) * dist;
    float sw = (float)GetScreenWidth();
    float sh = (float)GetScreenHeight();
    if (x < EXPEND_EDGE_PAD)       x = EXPEND_EDGE_PAD;
    else if (x > sw - EXPEND_EDGE_PAD) x = sw - EXPEND_EDGE_PAD;
    if (y < EXPEND_EDGE_PAD)       y = EXPEND_EDGE_PAD;
    else if (y > sh - EXPEND_EDGE_PAD) y = sh - EXPEND_EDGE_PAD;
    *tx = x;
    *ty = y;
}

void loot_fx_slot_expend_at(const char* item_id, float from_x, float from_y) {
    if (!item_id || item_id[0] == '\0') return;

    float ttx, tty;
    expend_target(from_x, from_y, &ttx, &tty);
    spawn_delivery_token(from_x, from_y, ttx, tty, item_id, EXPEND_TTL);

    for (int n = 0; n < EXPEND_COUNT; n++) {
        ScrParticle* p = scr_alloc();
        if (!p) break;
        float tx, ty;
        expend_target(from_x, from_y, &tx, &ty);
        *p = (ScrParticle){
            .mode   = 0,
            .x      = from_x,
            .y      = from_y,
            .sx     = from_x,
            .sy     = from_y,
            .tx     = tx,
            .ty     = ty,
            .ttl    = EXPEND_TTL * lcg_range(0.9f, 1.1f),
            .size   = lcg_range(EXPEND_SIZE_MIN, EXPEND_SIZE_MAX),
            .tint   = LOOT_FX_TINT_GOLD,
            .active = true,
        };
    }
}

void loot_fx_slot_expend(const char* item_id) {
    if (!item_id || item_id[0] == '\0') return;
    Vector2 slot;
    if (!inventory_bar_item_slot_center(item_id, &slot)) return;
    loot_fx_slot_expend_at(item_id, slot.x, slot.y);
}

/* ── Vacuum flight (stage 3) ───────────────────────────────────────────── */

void loot_fx_push(const char* drop_id, const char* collector_id,
                  const char* item_id, float world_x, float world_y) {
    if (!item_id || item_id[0] == '\0') return;

    /* Burst tint is personal: gold when the local player was eligible for
     * this drop (contributed damage — the server only lets contributors
     * collect, so a self-collect is always gold), gray when another player
     * collects loot that was never ours. */
    bool eligible = (NULL != collector_id)
        && (0 == strcmp(collector_id, g_game_state.player_id));

    /* Stop the in-world idle render for this token. */
    if (drop_id && drop_id[0] != '\0') {
        DropAnim* a = drop_find(drop_id);
        if (a) {
            a->collected = true;
            if (a->eligible) eligible = true;
        }
    }

    /* Treasure burst rising from below the token's ground location. */
    spawn_burst(world_x, world_y + BURST_EMIT_DOWN,
                eligible ? LOOT_FX_TINT_GOLD : LOOT_FX_TINT_GRAY);

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

bool loot_fx_inbound_to_inventory(const char* item_id) {
    if (!item_id || item_id[0] == '\0') return false;
    for (int i = 0; i < LOOT_FX_MAX; i++) {
        const VacFlight* f = &s_flights[i];
        if (f->active && 0 == strcmp(f->item_id, item_id) &&
            0 == strcmp(f->collector_id, g_game_state.player_id)) {
            return true;
        }
    }
    for (int i = 0; i < LOOT_PENDING_MAX; i++) {
        if (s_pending[i].active && 0 == strcmp(s_pending[i].item_id, item_id)) {
            return true;
        }
    }
    return false;
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
        if (t >= 1.0f) {
            f->active = false;
            trigger_slot_delivery(f); /* hand the item off into its slot */
            continue;
        }

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

    /* Fire scheduled slot-arrival bursts. */
    for (int i = 0; i < LOOT_PENDING_MAX; i++) {
        if (!s_pending[i].active) continue;
        if (s_clock >= s_pending[i].fire_at) {
            spawn_slot_arrival(s_pending[i].tx, s_pending[i].ty);
            /* The quantity change becomes visible exactly as the item lands. */
            fx_inventory_bar_qty_notify_arrival(s_pending[i].item_id);
            s_pending[i].active = false;
        }
    }

    /* Screen-space delivery + arrival particles. */
    for (int i = 0; i < LOOT_SCR_PARTICLE_MAX; i++) {
        ScrParticle* p = &s_scr[i];
        if (!p->active) continue;
        p->age += dt;
        if (p->age >= p->ttl) { p->active = false; continue; }
        if (p->mode == 0) {
            /* Constant-speed horizontal travel with a vertical parabolic arc, so
             * the spark tosses up and settles into the slot rather than darting
             * straight in. */
            float t = p->age / p->ttl;
            p->x = p->sx + (p->tx - p->sx) * t;
            p->y = p->sy + (p->ty - p->sy) * t
                 - 4.0f * DELIVER_ARC_PX * t * (1.0f - t);
        } else {
            p->vy += ARRIVAL_GRAVITY * dt;
            p->x  += p->vx * dt;
            p->y  += p->vy * dt;
        }
    }

    /* Delivery tokens age out with their stream. */
    for (int i = 0; i < TOKEN_MAX; i++) {
        DeliveryToken* t = &s_tokens[i];
        if (!t->active) continue;
        t->age += dt;
        if (t->age >= t->ttl) t->active = false;
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

int loot_fx_delivery_token_slot_count(void) { return TOKEN_MAX; }

bool loot_fx_delivery_token_at(int i, LootFxDeliveryToken* out) {
    if (i < 0 || i >= TOKEN_MAX || !out) return false;
    const DeliveryToken* t = &s_tokens[i];
    if (!t->active) return false;

    /* Same constant-speed horizontal + parabolic arc as the homing stream,
     * lifted above it; the lift eases out so the token lands on the slot. */
    float p = t->age / t->ttl;
    out->x = t->sx + (t->tx - t->sx) * p;
    out->y = t->sy + (t->ty - t->sy) * p
           - 4.0f * DELIVER_ARC_PX * p * (1.0f - p)
           - TOKEN_LIFT_PX * (1.0f - p);
    out->size  = TOKEN_SIZE_START + (TOKEN_SIZE_END - TOKEN_SIZE_START) * p;
    out->alpha = (p < TOKEN_FADE_FROM)
               ? 1.0f
               : 1.0f - (p - TOKEN_FADE_FROM) / (1.0f - TOKEN_FADE_FROM);
    strncpy(out->item_id, t->item_id, MAX_ITEM_ID_LENGTH - 1);
    out->item_id[MAX_ITEM_ID_LENGTH - 1] = '\0';
    return true;
}

int loot_fx_screen_particle_slot_count(void) { return LOOT_SCR_PARTICLE_MAX; }

bool loot_fx_screen_particle_at(int i, LootFxScreenParticle* out) {
    if (i < 0 || i >= LOOT_SCR_PARTICLE_MAX || !out) return false;
    const ScrParticle* p = &s_scr[i];
    if (!p->active) return false;

    out->x     = p->x;
    out->y     = p->y;
    out->size  = p->size;
    out->tint  = p->tint;
    out->alpha = 1.0f - p->age / p->ttl;
    if (out->alpha < 0.0f) out->alpha = 0.0f;
    return true;
}
