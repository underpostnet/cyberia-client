#include "ui/fx_inventory_bar_qty.h"

#include "game_state.h"
#include "loot_fx.h"
#include "object_layer.h"
#include "text.h"

#include <math.h>
#include <raylib.h>
#include <stdio.h>
#include <string.h>

/* ── Tuning ─────────────────────────────────────────────────────────────── */

#define FQ_MAX          96      /* tracked items                                */
#define FQ_TWEEN_RATE   9.0f    /* count-toward-target smoothing (per second)   */
#define FQ_RELEASE_WAIT 1.6     /* s to wait for a pickup particle before firing */
#define FQ_POPUP_TTL    1.30f   /* seconds a +/- popup lives                     */
#define FQ_POPUP_RISE   34.0f   /* px/s the popup floats upward                  */
#define FQ_FADE_FROM    0.62f   /* popup progress at which it starts fading      */
#define FQ_FONT_BASE    22      /* base glyph size (grows over life)            */
#define FQ_FONT_GROW    0.85f   /* extra scale gained by end of life            */
#define FQ_EVICT_SEC    3.0     /* forget an item unseen this long              */
#define FQ_PULSE_TTL    0.28f   /* slot pulse duration                          */
#define FQ_PULSE_AMP    0.12f   /* slot pulse peak extra scale                  */

static const Color FQ_GAIN = {  70, 240,  90, 255 }; /* vivid green + */
static const Color FQ_LOSS = { 245,  65,  55, 255 }; /* vivid red   - */

/* ── State ─────────────────────────────────────────────────────────────── */

typedef struct {
    char   item_id[MAX_ITEM_ID_LENGTH];
    int    actual;        /* last known authoritative quantity                  */
    float  display;       /* tweened badge value                                */
    /* A detected change held until the pickup particle lands (or fallback). */
    bool   pending;
    int    pending_delta; /* accumulated signed change while held               */
    double pending_since;
    /* Active popup. */
    int    delta;         /* signed change driving the current popup            */
    float  popup_age;     /* seconds since fired; < 0 = no popup                */
    float  pulse_age;     /* seconds since the slot pulse fired; < 0 = at rest  */
    bool   hidden;        /* first-copy slot held invisible until pickup lands  */
    double last_seen;     /* eviction sentinel                                  */
    bool   used;
} FqEntry;

static FqEntry s_e[FQ_MAX];
static double  s_clock = 0.0;

/* ── Lifecycle ─────────────────────────────────────────────────────────── */

void fx_inventory_bar_qty_init(void) {
    memset(s_e, 0, sizeof(s_e));
    s_clock = 0.0;
}

void fx_inventory_bar_qty_reset(void) {
    memset(s_e, 0, sizeof(s_e));
}

static FqEntry* find(const char* id) {
    for (int i = 0; i < FQ_MAX; i++) {
        if (s_e[i].used && 0 == strcmp(s_e[i].item_id, id)) return &s_e[i];
    }
    return NULL;
}

static FqEntry* alloc_entry(const char* id) {
    FqEntry* slot = NULL;
    double oldest = 1e18;
    int    oldest_i = 0;
    for (int i = 0; i < FQ_MAX; i++) {
        if (!s_e[i].used) { slot = &s_e[i]; break; }
        if (s_e[i].last_seen < oldest) { oldest = s_e[i].last_seen; oldest_i = i; }
    }
    if (!slot) slot = &s_e[oldest_i];
    memset(slot, 0, sizeof(*slot));
    strncpy(slot->item_id, id, MAX_ITEM_ID_LENGTH - 1);
    slot->popup_age = -1.0f;
    slot->pulse_age = -1.0f;
    slot->used = true;
    return slot;
}

/* Fire the held change as a visible popup; a gain (or a first-copy reveal)
 * also pulses the slot so the number and the scale pop read as one event. */
static void release_pending(FqEntry* e) {
    if (e->pending_delta != 0) {
        e->delta = e->pending_delta;
        e->popup_age = 0.0f;
        if (e->pending_delta > 0 || e->hidden) e->pulse_age = 0.0f;
    }
    e->hidden = false;
    e->pending = false;
    e->pending_delta = 0;
}

void fx_inventory_bar_qty_notify_arrival(const char* item_id) {
    if (!item_id || item_id[0] == '\0') return;
    FqEntry* e = find(item_id);
    if (e && e->pending) release_pending(e);
}

/* ── Update ────────────────────────────────────────────────────────────── */

void fx_inventory_bar_qty_update(float dt) {
    s_clock += dt;

    const GameState* gs = &g_game_state;
    for (int i = 0; i < gs->full_inventory_count; i++) {
        const char* id = gs->full_inventory[i].item_id;
        if (id[0] == '\0') continue;
        int qty = gs->full_inventory[i].quantity;

        FqEntry* e = find(id);
        if (!e) {
            e = alloc_entry(id);
            e->actual = qty;
            if (loot_fx_inbound_to_inventory(id)) {
                /* First copy with its pickup still in flight: hold the slot
                 * hidden until the flight lands, then reveal + "+N" + tween. */
                e->hidden = true;
                e->display = 0.0f;
                e->pending = true;
                e->pending_delta = qty;
                e->pending_since = s_clock;
            } else {
                /* Initial load / non-pickup grant: seed without a popup. */
                e->display = (float)qty;
            }
        } else if (e->actual != qty) {
            /* Hold the change until the pickup particle reaches the slot. */
            if (!e->pending) { e->pending = true; e->pending_since = s_clock; }
            e->pending_delta += qty - e->actual;
            e->actual = qty;
        }
        e->last_seen = s_clock;
    }

    for (int i = 0; i < FQ_MAX; i++) {
        FqEntry* e = &s_e[i];
        if (!e->used) continue;

        /* Fallback: fire even without a pickup animation (quests, coins, etc.). */
        if (e->pending && (s_clock - e->pending_since) > FQ_RELEASE_WAIT) {
            release_pending(e);
        }

        /* The badge only counts once the change has been released. */
        if (!e->pending) {
            float gap = (float)e->actual - e->display;
            e->display += gap * fminf(1.0f, dt * FQ_TWEEN_RATE);
            if (fabsf((float)e->actual - e->display) < 0.5f) e->display = (float)e->actual;
        }

        if (e->popup_age >= 0.0f) {
            e->popup_age += dt;
            if (e->popup_age >= FQ_POPUP_TTL) e->popup_age = -1.0f;
        }

        if (e->pulse_age >= 0.0f) {
            e->pulse_age += dt;
            if (e->pulse_age >= FQ_PULSE_TTL) e->pulse_age = -1.0f;
        }

        if (s_clock - e->last_seen > FQ_EVICT_SEC) e->used = false;
    }
}

/* ── Queries / draw ────────────────────────────────────────────────────── */

int fx_inventory_bar_qty_display(const char* item_id, int actual) {
    if (!item_id || item_id[0] == '\0') return actual;
    FqEntry* e = find(item_id);
    if (!e) return actual;
    return (int)(e->display + 0.5f);
}

bool fx_inventory_bar_qty_slot_visible(const char* item_id) {
    if (!item_id || item_id[0] == '\0') return true;
    FqEntry* e = find(item_id);
    return !(e && e->hidden);
}

float fx_inventory_bar_qty_slot_scale(const char* item_id) {
    if (!item_id || item_id[0] == '\0') return 1.0f;
    FqEntry* e = find(item_id);
    if (!e || e->pulse_age < 0.0f) return 1.0f;
    /* Half-sine envelope: eases up to the peak and back with no bounce. */
    float t = e->pulse_age / FQ_PULSE_TTL;
    return 1.0f + FQ_PULSE_AMP * sinf(PI * t);
}

static void draw_popup(const FqEntry* e, Rectangle slot) {
    float t = e->popup_age / FQ_POPUP_TTL;
    float alpha = (t < FQ_FADE_FROM) ? 1.0f : 1.0f - (t - FQ_FADE_FROM) / (1.0f - FQ_FADE_FROM);
    if (alpha < 0.0f) alpha = 0.0f;

    /* Glyph grows across its life so the change reads as a satisfying pop.
     * The outline thickness is fixed (not derived from fs) so it stays a
     * crisp, solid edge instead of smearing into a distorted blob as the
     * glyph grows across its life. */
    int fs = (int)(FQ_FONT_BASE * (1.0f + FQ_FONT_GROW * t) + 0.5f);
    const int border = 2;

    int n = (e->delta < 0) ? -e->delta : e->delta;
    char buf[24];
    snprintf(buf, sizeof(buf), "%s%d", (e->delta < 0) ? "-" : "+", n);

    Color body = (e->delta < 0) ? FQ_LOSS : FQ_GAIN;
    unsigned char a = (unsigned char)(255.0f * alpha + 0.5f);

    int tw = MeasureText(buf, fs);
    int cx = (int)(slot.x + slot.width * 0.5f) - tw / 2;
    int cy = (int)(slot.y - fs - 4 - FQ_POPUP_RISE * e->popup_age);

    /* Solid, opaque black border — concentric 8-direction rings at a capped
     * thickness (no translucent drop shadow, no size-dependent distortion). */
    Color outline = { 0, 0, 0, a };
    for (int o = 1; o <= border; o++)
        for (int dy = -1; dy <= 1; dy++)
            for (int dx = -1; dx <= 1; dx++)
                if (dx || dy)
                    DrawText(buf, cx + dx * o, cy + dy * o, fs, outline);

    body.a = a;
    DrawText(buf, cx, cy, fs, body);
}

void fx_inventory_bar_qty_draw(Rectangle slot, const char* item_id) {
    if (!item_id || item_id[0] == '\0') return;
    FqEntry* e = find(item_id);
    if (!e || e->popup_age < 0.0f) return;
    draw_popup(e, slot);
}

void fx_inventory_bar_qty_draw_bottom(Rectangle anchor) {
    int shown = 0;
    for (int i = 0; i < FQ_MAX && shown < 6; i++) {
        const FqEntry* e = &s_e[i];
        if (!e->used || e->popup_age < 0.0f) continue;

        int column = shown % 3 - 1;
        int row = shown / 3;
        float lane = anchor.width;
        Rectangle slot = {
            anchor.x + anchor.width * 0.5f + column * (lane + 6.0f) - lane * 0.5f,
            anchor.y - row * (lane * 0.6f), lane, anchor.height,
        };
        draw_popup(e, slot);
        shown++;
    }
}
