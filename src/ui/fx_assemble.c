#include "ui/fx_assemble.h"

#include "fx/fx_shapes.h"

#include <math.h>
#include <raylib.h>
#include <stdint.h>
#include <string.h>

/* ── Tuning ─────────────────────────────────────────────────────────────── */

#define AFX_STREAM_COUNT  72   /* fragments converging on the card            */
#define AFX_ARC_COUNT     10   /* electric rays sweeping in toward the card   */

/* The field reaches from just outside the card to near the screen edges, so a
 * synthesis occupies the viewport rather than hugging the panel. The ring
 * breathes in and out on a continuous loop. */
#define AFX_MARGIN_FRACTION        0.94f  /* outer ring padding, fraction of the edge gap */
#define AFX_MARGIN_BREATH_FRACTION 0.12f
#define AFX_MARGIN_FLOOR           96.0f  /* px floor for a full-bleed card               */
#define AFX_BREATH_FREQ     0.9f
#define AFX_INTENSITY_RATE  3.6f   /* fade-in/out speed (per second)          */
#define AFX_EDGE_PAD        10.0f  /* px the stream dissolves outside the card */

/* Streams: each fragment falls from the outer ring to the card edge, then
 * respawns further out. Speed is normalised radius per second, so the travel
 * time is viewport-independent. */
#define AFX_STREAM_SPEED_MIN 0.30f
#define AFX_STREAM_SPEED_MAX 0.85f
#define AFX_STREAM_SIZE_MIN  10.0f
#define AFX_STREAM_SIZE_MAX  26.0f
#define AFX_STREAM_WOBBLE    0.05f   /* angular drift while falling inward    */

/* Rays: long bright dashes that sweep in toward the card and fade — the
 * discharge of a component landing. Slow and wide enough to read as a beam
 * rather than a flicker. */
#define AFX_ARC_PERIOD_MIN  0.90f
#define AFX_ARC_PERIOD_MAX  2.20f
#define AFX_ARC_LEN_MIN     140.0f
#define AFX_ARC_LEN_MAX     340.0f
#define AFX_ARC_WIDTH       4.0f

#define AFX_TRAIL_LEN       6

/* Electric blue family — the body of a fragment, and the hotter core of a
 * ray. */
static const Color AFX_FRAGMENT = {  90, 220, 245, 255 };
static const Color AFX_ARC_HOT  = { 210, 250, 255, 255 };

/* ── State ─────────────────────────────────────────────────────────────── */

typedef struct {
    float u;        /* angle on the ring, 0..1                              */
    float r;        /* normalised distance: 1 = outer ring, 0 = card edge   */
    float speed;    /* r per second                                          */
    float size;
    Vector2 trail[AFX_TRAIL_LEN];
    int     trail_head;
} AfxStream;

typedef struct {
    float u;        /* angle on the card edge                                */
    float len;
    float age, period;
} AfxArc;

static AfxStream s_streams[AFX_STREAM_COUNT];
static AfxArc    s_arcs[AFX_ARC_COUNT];
static Rectangle s_bounds    = { 0, 0, 0, 0 };
static float     s_intensity = 0.0f;
static double    s_clock     = 0.0;
static bool      s_requested = false;

/* Deterministic LCG — avoids perturbing the global rand() state. */
static uint32_t s_lcg = 0x5CE1BAu;
static float lcg_f01(void) {
    s_lcg = s_lcg * 1664525u + 1013904223u;
    return (float)(s_lcg >> 8) / (float)(1u << 24);
}
static float lcg_range(float lo, float hi) { return lo + lcg_f01() * (hi - lo); }

/* ── Geometry ──────────────────────────────────────────────────────────── */

static Vector2 card_center(void) {
    return (Vector2){ s_bounds.x + s_bounds.width * 0.5f, s_bounds.y + s_bounds.height * 0.5f };
}

/* Room between the card and the nearest screen edge — the outer ring never
 * reaches further, so the field stays on screen while scaling with the
 * viewport. */
static float outer_gap(void) {
    float sw = (float)GetScreenWidth();
    float sh = (float)GetScreenHeight();
    float g = fminf(fminf(s_bounds.x, sw - (s_bounds.x + s_bounds.width)),
                    fminf(s_bounds.y, sh - (s_bounds.y + s_bounds.height)));
    return g > 0.0f ? g : 0.0f;
}

static float current_margin(void) {
    float gap  = outer_gap();
    float base = gap * AFX_MARGIN_FRACTION;
    if (base < AFX_MARGIN_FLOOR) base = AFX_MARGIN_FLOOR;
    float breath = 0.5f + 0.5f * sinf((float)s_clock * AFX_BREATH_FREQ);
    return base + gap * AFX_MARGIN_BREATH_FRACTION * breath;
}

/* Point at angle u on the ellipse whose padding is `margin` px beyond the
 * card. An ellipse (not the card outline) makes the field read as a round
 * halo converging inward rather than a frame traced around the panel. */
static Vector2 ring_point(float u, float margin) {
    Vector2 c = card_center();
    float angle = (u - floorf(u)) * 6.2831853f;
    return (Vector2){ c.x + (s_bounds.width * 0.5f + margin) * cosf(angle),
                      c.y + (s_bounds.height * 0.5f + margin) * sinf(angle) };
}

/* Position of a stream fragment at normalised radius r: the outer ring at 1,
 * the card edge at 0. */
static Vector2 stream_point(float u, float r) {
    return ring_point(u, AFX_EDGE_PAD + (current_margin() - AFX_EDGE_PAD) * r);
}

/* ── Lifecycle ─────────────────────────────────────────────────────────── */

static void spawn_stream(AfxStream* s, bool anywhere) {
    s->u = lcg_f01();
    s->r = anywhere ? lcg_f01() : 1.0f;
    s->speed = lcg_range(AFX_STREAM_SPEED_MIN, AFX_STREAM_SPEED_MAX);
    s->size = lcg_range(AFX_STREAM_SIZE_MIN, AFX_STREAM_SIZE_MAX);
    Vector2 at = stream_point(s->u, s->r);
    for (int k = 0; k < AFX_TRAIL_LEN; k++) s->trail[k] = at;
    s->trail_head = 0;
}

void fx_assemble_init(void) {
    s_intensity = 0.0f;
    s_clock     = 0.0;
    s_requested = false;

    for (int i = 0; i < AFX_STREAM_COUNT; i++) spawn_stream(&s_streams[i], true);
    for (int i = 0; i < AFX_ARC_COUNT; i++) {
        s_arcs[i] = (AfxArc){
            .u = lcg_f01(),
            .len = lcg_range(AFX_ARC_LEN_MIN, AFX_ARC_LEN_MAX),
            .age = lcg_f01(),
            .period = lcg_range(AFX_ARC_PERIOD_MIN, AFX_ARC_PERIOD_MAX),
        };
    }
}

void fx_assemble_reset(void) {
    s_intensity = 0.0f;
    s_requested = false;
}

void fx_assemble_show(Rectangle card_bounds) {
    s_bounds    = card_bounds;
    s_requested = true;
}

/* ── Update ────────────────────────────────────────────────────────────── */

static void trail_push(Vector2* trail, int* head, Vector2 pos) {
    trail[*head] = pos;
    *head = (*head + 1) % AFX_TRAIL_LEN;
}

void fx_assemble_update(float dt) {
    s_clock += dt;

    float target = s_requested ? 1.0f : 0.0f;
    float step = dt * AFX_INTENSITY_RATE;
    if (s_intensity < target) {
        s_intensity += step;
        if (s_intensity > target) s_intensity = target;
    } else if (s_intensity > target) {
        s_intensity -= step;
        if (s_intensity < target) s_intensity = target;
    }

    if (s_intensity > 0.001f) {
        for (int i = 0; i < AFX_STREAM_COUNT; i++) {
            AfxStream* s = &s_streams[i];
            /* Accelerate as it closes in, so the pull toward the card reads. */
            s->r -= s->speed * (0.45f + 0.55f * (1.0f - s->r)) * dt;
            s->u += AFX_STREAM_WOBBLE * dt;
            if (s->r <= 0.0f) { spawn_stream(s, false); continue; }
            trail_push(s->trail, &s->trail_head, stream_point(s->u, s->r));
        }
        for (int i = 0; i < AFX_ARC_COUNT; i++) {
            AfxArc* a = &s_arcs[i];
            a->age += dt;
            if (a->age >= a->period) {
                a->age = 0.0f;
                a->u = lcg_f01();
                a->len = lcg_range(AFX_ARC_LEN_MIN, AFX_ARC_LEN_MAX);
                a->period = lcg_range(AFX_ARC_PERIOD_MIN, AFX_ARC_PERIOD_MAX);
            }
        }
    }

    s_requested = false; /* re-armed each frame by fx_assemble_show */
}

/* ── Draw ──────────────────────────────────────────────────────────────── */

void fx_assemble_draw(void) {
    if (s_intensity <= 0.001f) return;

    float grow = 0.55f + 0.45f * s_intensity;

    /* Rays sweeping in toward the card, brightest as they arrive. */
    for (int i = 0; i < AFX_ARC_COUNT; i++) {
        const AfxArc* a = &s_arcs[i];
        float t = a->age / a->period;
        /* Travel the outer reach down to the card edge, fading over the last
         * third so the beam lands rather than blinking out mid-flight. */
        float reach = current_margin() * (1.0f - t);
        float alpha = t < 0.66f ? 1.0f : 1.0f - (t - 0.66f) / 0.34f;
        if (alpha <= 0.0f) continue;
        Vector2 from = ring_point(a->u, AFX_EDGE_PAD + reach);
        Vector2 to   = ring_point(a->u + 0.010f, AFX_EDGE_PAD + reach + a->len);
        Color hot = AFX_ARC_HOT;
        hot.a = (unsigned char)(255.0f * s_intensity * alpha);
        DrawLineEx(from, to, AFX_ARC_WIDTH, hot);
    }

    /* Converging streams, comet-trailed so the direction of travel reads. */
    for (int i = 0; i < AFX_STREAM_COUNT; i++) {
        const AfxStream* s = &s_streams[i];
        /* Brightest as it reaches the card, faded while still far out. */
        float near = 1.0f - s->r;
        float alpha = s_intensity * (0.30f + 0.70f * near);
        float size = s->size * grow;

        for (int k = 0; k < AFX_TRAIL_LEN; k++) {
            int idx = (s->trail_head + k) % AFX_TRAIL_LEN;
            float w = (float)(k + 1) / (float)(AFX_TRAIL_LEN + 1);
            fx_shape_spark(s->trail[idx].x, s->trail[idx].y, size * w, AFX_FRAGMENT,
                           alpha * w * w);
        }
        Vector2 at = stream_point(s->u, s->r);
        fx_shape_spark(at.x, at.y, size, AFX_FRAGMENT, alpha);
    }

}
