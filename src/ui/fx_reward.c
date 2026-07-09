#include "ui/fx_reward.h"

#include "fx_shapes.h"
#include "ui_icon.h"

#include <math.h>
#include <raylib.h>
#include <stdint.h>
#include <string.h>

/* ── Tuning ─────────────────────────────────────────────────────────────── */

#define RFX_STAR_COUNT      26
#define RFX_SPARK_COUNT     60

/* Three concentric rings (ellipses centered on the card) act as anchor
 * surfaces for stars and particles: inner, mid, and outer. The mid ring is
 * the original reference distance; inner/outer scale from it. Each anchor
 * picks one ring at init and stays there — bobbing/pulsing/spinning in
 * place, never traveling toward the card. */
#define RFX_RING_COUNT        3
#define RFX_RING_INNER_SCALE  0.55f  /* inner ring margin, relative to the mid ring */
#define RFX_RING_OUTER_SCALE  1.55f  /* outer ring margin, relative to the mid ring */

/* The mid ring's padding is scaled from the room between the card and the
 * nearest screen edge — so on any viewport the ring set reaches out close to
 * the edges, breathing (looping its padding in and out). */
#define RFX_MARGIN_FRACTION        0.80f  /* mid ring padding, fraction of the edge gap */
#define RFX_MARGIN_BREATH_FRACTION 0.10f  /* breathing range, fraction of the edge gap  */
#define RFX_MARGIN_FLOOR           64.0f  /* px floor for a full-bleed card              */
#define RFX_BREATH_FREQ     1.1f    /* rad/s — the padding loop rate             */
#define RFX_INTENSITY_RATE  3.2f    /* fade-in/out speed (per second)            */

/* Dispersion: every anchor (star or particle) sits at its own radius offset
 * from its assigned ring and wanders along it, so the halo reads as
 * scattered rather than three crisp circles. */
#define RFX_RING_JITTER_MIN -36.0f
#define RFX_RING_JITTER_MAX 100.0f
#define RFX_RING_U_JITTER    0.46f   /* fraction of a slot each anchor wanders along its ring */

#define RFX_FLOAT_AMP       9.0f    /* bob amplitude, px                         */
#define RFX_FLOAT_FREQ      2.0f    /* rad/s                                     */
#define RFX_PULSE_FREQ      3.0f    /* scale-pulse rate, rad/s                   */
#define RFX_ANCHOR_SPIN_MIN 14.0f   /* deg/s (magnitude)                         */
#define RFX_ANCHOR_SPIN_MAX 34.0f

#define RFX_STAR_SIZE_MIN   52.0f
#define RFX_STAR_SIZE_MAX   84.0f

#define RFX_TRAIL_LEN        5      /* ghost positions kept behind each arrival   */

/* Small particles — most render as plain sparks, a fraction as tiny stars,
 * for "both particles and stars" in the halo. Anchored on the same three
 * rings as the large stars, just smaller and more numerous. */
#define RFX_SPARK_SIZE_MIN      12.0f
#define RFX_SPARK_SIZE_MAX      24.0f
#define RFX_SPARK_STAR_CHANCE   0.25f   /* fraction of particles drawn as tiny stars */
#define RFX_SPARK_STAR_SIZE_MIN 20.0f
#define RFX_SPARK_STAR_SIZE_MAX 34.0f

/* Item arrival — when a new reward appears a wave of particles glides in
 * slowly from the screen edges (staggered so the wave trickles in rather
 * than bursting all at once), lands on one of the three rings with a soft
 * bounce, lingers there bobbing like the ambient anchors, then fades away. */
#define RFX_ARRIVAL_MAX       56
#define RFX_ARRIVAL_STAGGER_MAX 0.80f /* s, random start delay per particle      */
#define RFX_ARRIVAL_TRAVEL_MIN 2.30f  /* s, edge → ring (slow, readable)         */
#define RFX_ARRIVAL_TRAVEL_MAX 3.20f
#define RFX_ARRIVAL_SETTLE_MIN 2.0f   /* s, lingering on the ring before fading  */
#define RFX_ARRIVAL_SETTLE_MAX 3.0f
#define RFX_ARRIVAL_FADE       0.32f  /* fraction of the settle phase used to fade */
#define RFX_ARRIVAL_SIZE_MIN   18.0f
#define RFX_ARRIVAL_SIZE_MAX   30.0f
#define RFX_ARRIVAL_EDGE_PAD   16.0f  /* px the start sits beyond the screen edge */
#define RFX_ARRIVAL_STAR_SPIN  90.0f  /* deg/s tumble rate, arrivals drawn as stars */

/* Warm gold star tint (opacity comes from the show/close fade only). */
static const Color RFX_STAR_TINT = { 255, 236, 170, 255 };

/* ── State ─────────────────────────────────────────────────────────────── */

typedef enum { RFX_RING_INNER = 0, RFX_RING_MID = 1, RFX_RING_OUTER = 2 } RfxRing;

typedef struct {
    float u;             /* 0..1 anchor angle on its ring, fixed (no orbit)      */
    float phase;         /* bob / pulse phase                                    */
    float rot;           /* current rotation, degrees                            */
    float spin;          /* deg/s, signed                                        */
    float base;          /* base size, px                                        */
    float radius_jitter; /* px offset from the assigned ring, scatters the halo  */
    RfxRing layer;        /* which of the 3 concentric rings this anchors on      */
} RfxStar;

typedef struct {
    float u, phase, rot, spin, base, radius_jitter;
    RfxRing layer;
    bool  is_star;      /* draw as a small star icon instead of a square spark  */
} RfxSpark;

typedef struct {
    float sx, sy;       /* screen-edge start                                    */
    float u;            /* landing angle on the ring (0..1)                     */
    RfxRing layer;       /* which of the 3 concentric rings this lands on        */
    float x, y;         /* current position                                     */
    float delay;        /* seconds before this particle starts gliding in       */
    float travel_dur;   /* seconds edge → ring                                   */
    float settle_dur;   /* seconds lingering on the ring before fading           */
    float age, ttl;     /* ttl = delay + travel_dur + settle_dur                 */
    float size;
    float margin_jitter; /* px offset from the assigned ring radius, dispersion  */
    bool  is_star;      /* draw as a small star icon instead of a square spark  */
    Vector2 trail[RFX_TRAIL_LEN]; /* past positions, oldest at trail_head        */
    int     trail_head;
    bool  active;
} RfxArrival;

static RfxStar    s_stars[RFX_STAR_COUNT];
static RfxSpark   s_sparks[RFX_SPARK_COUNT];
static RfxArrival s_arrivals[RFX_ARRIVAL_MAX];
static Rectangle s_bounds     = { 0, 0, 0, 0 };
static float     s_intensity  = 0.0f;
static double    s_clock      = 0.0;
static bool      s_requested  = false;

/* Deterministic LCG — avoids perturbing the global rand() state. */
static uint32_t s_lcg = 0xA1330Du;
static float lcg_f01(void) {
    s_lcg = s_lcg * 1664525u + 1013904223u;
    return (float)(s_lcg >> 8) / (float)(1u << 24);
}
static float lcg_range(float lo, float hi) { return lo + lcg_f01() * (hi - lo); }

/* ── Geometry ──────────────────────────────────────────────────────────── */

/* Screen-space center of the card. */
static Vector2 card_center(void) {
    return (Vector2){ s_bounds.x + s_bounds.width * 0.5f, s_bounds.y + s_bounds.height * 0.5f };
}

/* Point at angle parameter u (0..1 = one full turn) on an ellipse centered on
 * the card, with per-axis radius rx/ry. An ellipse (not the card's rectangle
 * outline) is what makes each ring read as a round halo around the card
 * instead of a frame traced around it. */
static Vector2 ellipse_point(float rx, float ry, float u) {
    Vector2 c = card_center();
    float angle = u * 6.2831853f;
    return (Vector2){ c.x + rx * cosf(angle), c.y + ry * sinf(angle) };
}

/* Room between the card and the nearest screen edge, on whichever axis is
 * tightest — the mid ring never reaches out further than this, so the whole
 * ring set stays on screen while scaling with the viewport. */
static float outer_gap(void) {
    float sw = (float)GetScreenWidth();
    float sh = (float)GetScreenHeight();
    float gl = s_bounds.x;
    float gr = sw - (s_bounds.x + s_bounds.width);
    float gt = s_bounds.y;
    float gb = sh - (s_bounds.y + s_bounds.height);
    float g = fminf(fminf(gl, gr), fminf(gt, gb));
    return g > 0.0f ? g : 0.0f;
}

/* Mid ring padding, breathing in and out on a continuous loop. Scales with
 * the viewport so the ring set reaches from near the screen edges to the
 * card on any screen size. */
static float current_margin(void) {
    float gap  = outer_gap();
    float base = gap * RFX_MARGIN_FRACTION;
    if (base < RFX_MARGIN_FLOOR) base = RFX_MARGIN_FLOOR;
    float amp    = gap * RFX_MARGIN_BREATH_FRACTION;
    float breath = 0.5f + 0.5f * sinf((float)s_clock * RFX_BREATH_FREQ);
    return base + amp * breath;
}

/* Padding for a given ring layer, scaled from the shared breathing mid-ring
 * margin. */
static float ring_margin_for(RfxRing layer) {
    float mid = current_margin();
    switch (layer) {
        case RFX_RING_INNER: return mid * RFX_RING_INNER_SCALE;
        case RFX_RING_OUTER: return mid * RFX_RING_OUTER_SCALE;
        default:             return mid;
    }
}

/* Screen position at angle u on ring `layer`, plus an optional per-anchor
 * radius offset (extra) added to both axes, so different anchors can share
 * the same ring yet sit at different distances from the card. */
static Vector2 ring_pos_u_at(float u, float extra, RfxRing layer) {
    u -= floorf(u);
    float margin = ring_margin_for(layer) + extra;
    return ellipse_point(s_bounds.width * 0.5f + margin, s_bounds.height * 0.5f + margin, u);
}

/* Decelerating glide with a gentle overshoot-and-settle at the end, so the
 * arrival wave lands with a soft, visible bounce instead of just stopping. */
static float ease_out_back(float t) {
    const float c1 = 1.70158f;
    const float c3 = c1 + 1.0f;
    float u = t - 1.0f;
    return 1.0f + c3 * u * u * u + c1 * u * u;
}

/* A random point just off one of the four screen edges. */
static Vector2 random_edge_point(void) {
    float sw = (float)GetScreenWidth();
    float sh = (float)GetScreenHeight();
    switch ((int)(lcg_f01() * 4.0f) & 3) {
        case 0:  return (Vector2){ lcg_range(0.0f, sw), -RFX_ARRIVAL_EDGE_PAD };
        case 1:  return (Vector2){ sw + RFX_ARRIVAL_EDGE_PAD, lcg_range(0.0f, sh) };
        case 2:  return (Vector2){ lcg_range(0.0f, sw), sh + RFX_ARRIVAL_EDGE_PAD };
        default: return (Vector2){ -RFX_ARRIVAL_EDGE_PAD, lcg_range(0.0f, sh) };
    }
}

/* ── Anchor init ───────────────────────────────────────────────────────── */

/* Shared per-anchor random layout: an angle spread evenly around `count`
 * slots (with wander so it isn't perfectly even), a bob/pulse phase, a spin,
 * a radius offset from its ring, and one of the 3 rings (round-robin by
 * index, which still spreads each ring's share evenly since every Nth index
 * lands on the same ring). */
typedef struct {
    float u, phase, rot, spin, radius_jitter;
    RfxRing layer;
} RfxAnchorLayout;

static RfxAnchorLayout make_anchor_layout(int index, int count) {
    RfxAnchorLayout a;
    a.u = ((float)index + lcg_range(-RFX_RING_U_JITTER, RFX_RING_U_JITTER)) / (float)count;
    if (a.u < 0.0f) a.u += 1.0f;
    if (a.u >= 1.0f) a.u -= 1.0f;
    a.phase = lcg_range(0.0f, 6.2831853f);
    a.rot   = lcg_range(0.0f, 360.0f);
    a.spin  = lcg_range(RFX_ANCHOR_SPIN_MIN, RFX_ANCHOR_SPIN_MAX)
              * (lcg_f01() < 0.5f ? -1.0f : 1.0f);
    a.radius_jitter = lcg_range(RFX_RING_JITTER_MIN, RFX_RING_JITTER_MAX);
    a.layer = (RfxRing)(index % RFX_RING_COUNT);
    return a;
}

/* ── Lifecycle ─────────────────────────────────────────────────────────── */

void fx_reward_init(void) {
    memset(s_arrivals, 0, sizeof(s_arrivals));
    s_intensity = 0.0f;
    s_clock     = 0.0;
    s_requested = false;

    for (int i = 0; i < RFX_STAR_COUNT; i++) {
        RfxAnchorLayout a = make_anchor_layout(i, RFX_STAR_COUNT);
        s_stars[i] = (RfxStar){
            .u = a.u, .phase = a.phase, .rot = a.rot, .spin = a.spin,
            .base = lcg_range(RFX_STAR_SIZE_MIN, RFX_STAR_SIZE_MAX),
            .radius_jitter = a.radius_jitter, .layer = a.layer,
        };
    }
    for (int i = 0; i < RFX_SPARK_COUNT; i++) {
        RfxAnchorLayout a = make_anchor_layout(i, RFX_SPARK_COUNT);
        bool star = lcg_f01() < RFX_SPARK_STAR_CHANCE;
        s_sparks[i] = (RfxSpark){
            .u = a.u, .phase = a.phase, .rot = a.rot, .spin = a.spin,
            .base = star ? lcg_range(RFX_SPARK_STAR_SIZE_MIN, RFX_SPARK_STAR_SIZE_MAX)
                         : lcg_range(RFX_SPARK_SIZE_MIN, RFX_SPARK_SIZE_MAX),
            .radius_jitter = a.radius_jitter, .layer = a.layer, .is_star = star,
        };
    }
}

void fx_reward_reset(void) {
    memset(s_arrivals, 0, sizeof(s_arrivals));
    s_intensity = 0.0f;
    s_requested = false;
}

void fx_reward_show(Rectangle modal_bounds) {
    s_bounds    = modal_bounds;
    s_requested = true;
}

void fx_reward_trigger(Rectangle modal_bounds) {
    s_bounds    = modal_bounds;
    s_requested = true; /* ensure the celebration is up when the wave lands */

    for (int n = 0; n < RFX_ARRIVAL_MAX; n++) {
        Vector2 st = random_edge_point();
        float delay  = lcg_range(0.0f, RFX_ARRIVAL_STAGGER_MAX);
        float travel = lcg_range(RFX_ARRIVAL_TRAVEL_MIN, RFX_ARRIVAL_TRAVEL_MAX);
        float settle = lcg_range(RFX_ARRIVAL_SETTLE_MIN, RFX_ARRIVAL_SETTLE_MAX);
        s_arrivals[n] = (RfxArrival){
            .sx = st.x, .sy = st.y, .x = st.x, .y = st.y,
            .u = lcg_f01(),
            .layer = (RfxRing)(int)(lcg_f01() * RFX_RING_COUNT),
            .delay = delay,
            .travel_dur = travel,
            .settle_dur = settle,
            .ttl = delay + travel + settle,
            .size = lcg_range(RFX_ARRIVAL_SIZE_MIN, RFX_ARRIVAL_SIZE_MAX),
            .margin_jitter = lcg_range(RFX_RING_JITTER_MIN, RFX_RING_JITTER_MAX),
            .is_star = lcg_f01() < RFX_SPARK_STAR_CHANCE,
            .active = true,
        };
        for (int k = 0; k < RFX_TRAIL_LEN; k++) s_arrivals[n].trail[k] = (Vector2){ st.x, st.y };
    }
}

/* ── Update ────────────────────────────────────────────────────────────── */

/* Records a particle's current position into its ring-buffer trail; oldest
 * sample always lives at *head after the push. */
static void trail_push(Vector2* trail, int* head, Vector2 pos) {
    trail[*head] = pos;
    *head = (*head + 1) % RFX_TRAIL_LEN;
}

void fx_reward_update(float dt) {
    s_clock += dt;

    /* Fade toward the requested state. */
    float target = s_requested ? 1.0f : 0.0f;
    float step = RFX_INTENSITY_RATE * dt;
    if (s_intensity < target) {
        s_intensity += step;
        if (s_intensity > target) s_intensity = target;
    } else if (s_intensity > target) {
        s_intensity -= step;
        if (s_intensity < target) s_intensity = target;
    }

    for (int i = 0; i < RFX_STAR_COUNT; i++) {
        s_stars[i].rot += s_stars[i].spin * dt;
        if (s_stars[i].rot >= 360.0f) s_stars[i].rot -= 360.0f;
        else if (s_stars[i].rot < 0.0f) s_stars[i].rot += 360.0f;
    }
    for (int i = 0; i < RFX_SPARK_COUNT; i++) {
        s_sparks[i].rot += s_sparks[i].spin * dt;
        if (s_sparks[i].rot >= 360.0f) s_sparks[i].rot -= 360.0f;
        else if (s_sparks[i].rot < 0.0f) s_sparks[i].rot += 360.0f;
    }

    /* Arrival wave — glide in from the edge, land on its ring with a bounce,
     * then linger there (bobbing like the ambient anchors) before fading. */
    for (int i = 0; i < RFX_ARRIVAL_MAX; i++) {
        RfxArrival* a = &s_arrivals[i];
        if (!a->active) continue;
        a->age += dt;
        if (a->age >= a->ttl) { a->active = false; continue; }
        if (a->age < a->delay) continue; /* still waiting its turn to glide in */

        float t = a->age - a->delay;
        if (t < a->travel_dur) {
            float e = ease_out_back(t / a->travel_dur);
            Vector2 tgt = ring_pos_u_at(a->u, a->margin_jitter, a->layer);
            a->x = a->sx + (tgt.x - a->sx) * e;
            a->y = a->sy + (tgt.y - a->sy) * e;
        } else {
            Vector2 p = ring_pos_u_at(a->u, a->margin_jitter, a->layer);
            float bob = sinf((float)s_clock * RFX_FLOAT_FREQ + a->u * 6.2831853f)
                        * RFX_FLOAT_AMP * 0.6f;
            a->x = p.x;
            a->y = p.y + bob;
        }
        trail_push(a->trail, &a->trail_head, (Vector2){ a->x, a->y });
    }

    s_requested = false; /* re-armed each frame by fx_reward_show */
}

/* ── Draw ──────────────────────────────────────────────────────────────── */

/* Draws a fading, tapering comet trail behind a particle from its oldest
 * sample up to (excluding) its current position, which the caller draws
 * itself at full size/alpha. */
static void draw_trail(const Vector2* trail, int head, float size, float alpha) {
    for (int k = 0; k < RFX_TRAIL_LEN; k++) {
        int idx = (head + k) % RFX_TRAIL_LEN;             /* oldest → newest   */
        float w = (float)(k + 1) / (float)(RFX_TRAIL_LEN + 1); /* 0excl .. <1  */
        fx_shape_spark(trail[idx].x, trail[idx].y, size * w, FX_SPARK_GOLD, alpha * w * w);
    }
}

/* Draws one particle — a small tumbling star icon or a plain spark. */
static void draw_particle(Vector2 pos, float size, float alpha, bool is_star, float rot) {
    if (alpha < 0.0f) alpha = 0.0f;
    if (is_star) {
        Color tint = RFX_STAR_TINT;
        tint.a = (unsigned char)(255.0f * alpha + 0.5f);
        ui_icon_draw_ex("star", pos.x, pos.y, size, rot, tint);
    } else {
        fx_shape_spark(pos.x, pos.y, size, FX_SPARK_GOLD, alpha);
    }
}

void fx_reward_draw(void) {
    if (s_intensity <= 0.001f) return;

    float grow = 0.55f + 0.45f * s_intensity; /* anchors scale in on show */

    /* Small ambient particles — anchored on one of the 3 rings, bobbing and
     * pulsing in place. */
    for (int i = 0; i < RFX_SPARK_COUNT; i++) {
        const RfxSpark* p = &s_sparks[i];
        Vector2 at = ring_pos_u_at(p->u, p->radius_jitter, p->layer);
        float bob = sinf((float)s_clock * RFX_FLOAT_FREQ + p->phase) * RFX_FLOAT_AMP;
        float pulse = 0.80f + 0.20f * sinf((float)s_clock * RFX_PULSE_FREQ + p->phase * 1.7f);
        float size = p->base * pulse * grow;
        draw_particle((Vector2){ at.x, at.y + bob }, size, s_intensity, p->is_star, p->rot);
    }

    /* Arrival wave — the one-shot flourish, faded during its final stretch. */
    for (int i = 0; i < RFX_ARRIVAL_MAX; i++) {
        const RfxArrival* a = &s_arrivals[i];
        if (!a->active) continue;
        if (a->age < a->delay) continue; /* still waiting its turn to glide in */
        float t = a->age - a->delay;
        float alpha = s_intensity;
        if (t >= a->travel_dur) {
            float st = (t - a->travel_dur) / a->settle_dur;
            float fade_from = 1.0f - RFX_ARRIVAL_FADE;
            if (st > fade_from) alpha *= 1.0f - (st - fade_from) / (1.0f - fade_from);
        }
        draw_trail(a->trail, a->trail_head, a->size, alpha);
        draw_particle((Vector2){ a->x, a->y }, a->size, alpha, a->is_star, a->age * RFX_ARRIVAL_STAR_SPIN);
    }

    /* Large ambient stars — anchored on one of the 3 rings, over the small
     * particles so the halo reads foreground-to-background. */
    unsigned char sa = (unsigned char)(255.0f * s_intensity + 0.5f);
    for (int i = 0; i < RFX_STAR_COUNT; i++) {
        const RfxStar* st = &s_stars[i];
        Vector2 at = ring_pos_u_at(st->u, st->radius_jitter, st->layer);
        float bob = sinf((float)s_clock * RFX_FLOAT_FREQ + st->phase) * RFX_FLOAT_AMP;
        float pulse = 0.80f + 0.20f * sinf((float)s_clock * RFX_PULSE_FREQ + st->phase * 1.7f);
        float size = st->base * pulse * grow;

        Color tint = RFX_STAR_TINT;
        tint.a = sa;
        ui_icon_draw_ex("star", at.x, at.y + bob, size, st->rot, tint);
    }
}
