#include "fx_inventory_bar_qty.h"

#include "sim/local_player.h"
#include "sim/game_state.h"
#include "ui/inventory_bar.h"
#include "fx_loot.h"
#include "world/object_layer.h"
#include "ui/text.h"
#include "utils.h"

#include <math.h>
#include <raylib.h>
#include <stdio.h>
#include <string.h>

/* ── Tuning ─────────────────────────────────────────────────────────────── */

#define FQ_MAX          96      /* tracked items                                */
#define FQ_STACKS       CYBERIA_INVENTORY_SLOTS_MAX /* stacks remembered per item */
#define FQ_RELEASE_WAIT 1.6     /* s to wait for a pickup particle before firing */
#define FQ_HELD_WAIT    4.0     /* s fallback for externally-held changes (the
                                   holder refreshes pending_since while waiting) */
#define FQ_POPUP_TTL    1.30f   /* seconds a +/- popup lives                     */
#define FQ_POPUP_RISE   34.0f   /* px/s the popup floats upward                  */
#define FQ_FADE_FROM    0.62f   /* popup progress at which it starts fading      */
#define FQ_FONT_BASE    22      /* base glyph size (grows over life)            */
#define FQ_FONT_GROW    0.85f   /* extra scale gained by end of life            */
#define FQ_EVICT_SEC    3.0     /* forget an item unseen this long              */
#define FQ_PULSE_TTL    0.32f   /* slot pulse duration                          */
#define FQ_PULSE_AMP    0.18f   /* slot pulse peak extra scale                  */

static const Color FQ_GAIN = {  70, 240,  90, 255 }; /* vivid green + */
static const Color FQ_LOSS = { 245,  65,  55, 255 }; /* vivid red   - */

/* ── State ─────────────────────────────────────────────────────────────── */

typedef struct {
    char   item_id[MAX_ITEM_ID_LENGTH];
    int    actual;        /* last known authoritative quantity, over all stacks */
    /* The stacks of the item last frame, and the bag slot of the latest change:
     * the one stack its hold, pulse and popup show on. */
    int    seen_slot[FQ_STACKS];
    int    seen_qty[FQ_STACKS];
    int    seen_count;
    int    cell;
    int    display;       /* badge value: the pre-change count while held       */
    /* A detected change held until the pickup particle lands (or fallback). */
    bool   pending;
    int    pending_delta; /* accumulated signed change while held               */
    double pending_since;
    /* Active popup. */
    int    delta;         /* signed change driving the current popup            */
    float  popup_age;     /* seconds since fired; < 0 = no popup                */
    float  pulse_age;     /* seconds since the slot pulse fired; < 0 = at rest  */
    bool   hidden;        /* first-copy slot held invisible until pickup lands  */
    bool   held;          /* external hold (reward delivery) — long fallback    */
    bool   silent;        /* next change lands with no FX (a visible transfer)  */
    bool   present;       /* item was in the local inventory this frame         */
    float  slot_x, slot_y;/* last-known center of the changed cell, for the expend */
    bool   has_slot;      /* that center has been captured at least once        */
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

static FqEntry* find(const char* id) {
    for (int i = 0; i < FQ_MAX; i++) {
        if (s_e[i].used && 0 == strcmp(s_e[i].item_id, id)) return &s_e[i];
    }
    return NULL;
}

static float fq_rank(const void* elem) {
    const FqEntry* e = elem;
    return e->used ? (float)-e->last_seen : INFINITY;
}

static FqEntry* alloc_entry(const char* id) {
    FqEntry* slot = pool_take(s_e, sizeof(*s_e), FQ_MAX, fq_rank);
    copy_str(slot->item_id, MAX_ITEM_ID_LENGTH, id);
    slot->popup_age = -1.0f;
    slot->pulse_age = -1.0f;
    slot->used = true;
    return slot;
}

/* Fire the held change as a visible popup; a gain (or a first-copy reveal)
 * also pulses the slot so the number and the scale pop read as one event. A
 * reduction additionally sprays the item outward — the reverse of the pickup
 * delivery parabola — so a loss reads as the item leaving the slot. */
static void release_pending(FqEntry* e) {
    if (e->pending_delta != 0) {
        e->delta = e->pending_delta;
        e->popup_age = 0.0f;
        if (e->pending_delta > 0 || e->hidden) e->pulse_age = 0.0f;
        /* A reduction sprays outward from the cell that changed, captured
         * while it held the stack. */
        if (e->pending_delta < 0 && e->has_slot) fx_loot_slot_expend_at(e->item_id, e->slot_x, e->slot_y);
    }
    /* The badge lands with the delivery: the moment the item visibly arrives is
     * the moment its slot must read the new total, so the count snaps here
     * rather than rolling toward it afterwards. */
    e->display = e->actual;
    e->hidden = false;
    e->held = false;
    e->pending = false;
    e->pending_delta = 0;
}

void fx_inventory_bar_qty_suppress(const char* item_id) {
    if (!item_id || item_id[0] == '\0') return;
    FqEntry* e = find(item_id);
    if (!e) e = alloc_entry(item_id);
    e->silent = true;
    e->last_seen = s_clock;
}

void fx_inventory_bar_qty_hold_for_delivery(const char* item_id) {
    if (!item_id || item_id[0] == '\0') return;
    FqEntry* e = find(item_id);
    if (!e) {
        /* Reward for an item not yet tracked: pre-register a hidden first-copy
         * entry so the slot reveals only when the delivery lands. */
        e = alloc_entry(item_id);
        e->hidden = true;
    }
    if (!e->present && 0 == e->actual) e->hidden = true;
    e->silent  = false;
    e->held    = true;
    e->pending = true;
    e->pending_since = s_clock; /* refreshed by the holder while it waits */
    e->last_seen     = s_clock;
}

void fx_inventory_bar_qty_notify_arrival(const char* item_id) {
    if (!item_id || item_id[0] == '\0') return;
    FqEntry* e = find(item_id);
    if (e && e->pending) release_pending(e);
}

/* ── Update ────────────────────────────────────────────────────────────── */

/* The bag slot whose stack changed since last frame: a stack that grew, shrank
 * or arrived, else one that left. */
static int changed_cell(const FqEntry* e, const int slots[], const int qtys[], int count) {
    for (int k = 0; k < count; k++) {
        int j = 0;
        while (j < e->seen_count && slots[k] != e->seen_slot[j]) j++;
        if (j == e->seen_count || qtys[k] != e->seen_qty[j]) return slots[k];
    }
    for (int j = 0; j < e->seen_count; j++) {
        int k = 0;
        while (k < count && slots[k] != e->seen_slot[j]) k++;
        if (k == count) return e->seen_slot[j];
    }
    return e->cell;
}

void fx_inventory_bar_qty_update(float dt) {
    s_clock += dt;

    for (int i = 0; i < FQ_MAX; i++) s_e[i].present = false;

    const GameState* gs = &g_game_state;
    for (int i = 0; i < g_local_player.inventory_count; i++) {
        const char* id = g_local_player.inventory[i].item_id;
        if (id[0] == '\0') continue;
        /* One entry per item: the total of its stacks, read at its first stack. */
        int qty = 0, count = 0, first = i;
        int slots[FQ_STACKS], qtys[FQ_STACKS];
        for (int k = 0; k < g_local_player.inventory_count; k++) {
            const ObjectLayerState* stack = &g_local_player.inventory[k];
            if (0 != strcmp(stack->item_id, id)) continue;
            if (0 == count) first = k;
            if (FQ_STACKS > count) {
                slots[count] = stack->slot;
                qtys[count] = stack->quantity;
            }
            count++;
            qty += stack->quantity;
        }
        if (first != i) continue;
        if (FQ_STACKS < count) count = FQ_STACKS;

        FqEntry* e = find(id);
        if (!e) {
            e = alloc_entry(id);
            e->actual = qty;
            e->cell = slots[0];
            if (fx_loot_inbound_to_inventory(id)) {
                /* First copy with its pickup still in flight: hold the slot
                 * hidden until the flight lands, then reveal + "+N" + tween. */
                e->hidden = true;
                e->display = 0;
                e->pending = true;
                e->pending_delta = qty;
                e->pending_since = s_clock;
            } else {
                /* Initial load / non-pickup grant: seed without a popup. */
                e->display = qty;
            }
        } else if (e->actual != qty) {
            e->cell = changed_cell(e, slots, qtys, count);
            if (e->silent) {
                /* A transfer between two visible surfaces: land the badge with
                 * no popup, and drop any hold that was waiting on it. */
                e->actual = qty;
                e->display = qty;
                e->hidden = e->held = e->pending = e->silent = false;
                e->pending_delta = 0;
            } else {
                /* Hold the change until the pickup particle reaches the slot. */
                if (!e->pending) { e->pending = true; e->pending_since = s_clock; }
                e->pending_delta += qty - e->actual;
                e->actual = qty;
            }
        }
        memcpy(e->seen_slot, slots, (size_t)count * sizeof slots[0]);
        memcpy(e->seen_qty, qtys, (size_t)count * sizeof qtys[0]);
        e->seen_count = count;
        e->present    = true;
        e->last_seen  = s_clock;
        /* Remember where the changed cell sits, so the expend can launch from
         * there once its stack is gone. */
        Vector2 sc = inventory_bar_cell_center(e->cell);
        e->slot_x = sc.x;
        e->slot_y = sc.y;
        e->has_slot = true;
    }

    for (int i = 0; i < FQ_MAX; i++) {
        FqEntry* e = &s_e[i];
        if (!e->used) continue;

        /* Last copy consumed (qty 1→0 drops the stack server-side, so the item
         * leaves the inventory): synthesize the decrement to 0 so it fires the
         * same -N popup + expend as any in-place reduction. */
        if (!e->present && e->actual > 0 && !e->hidden && !e->held) {
            if (e->silent) {
                e->actual = 0;
                e->display = 0;
                e->silent = e->pending = false;
                e->pending_delta = 0;
            } else {
                if (!e->pending) { e->pending = true; e->pending_since = s_clock; }
                e->pending_delta += -e->actual;
                e->actual = 0;
            }
        }

        /* Fallback: fire even without a pickup animation (quests, coins, etc.). */
        double wait = e->held ? FQ_HELD_WAIT : FQ_RELEASE_WAIT;
        if (e->pending && (s_clock - e->pending_since) > wait) {
            release_pending(e);
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

int fx_inventory_bar_qty_display(const char* item_id, int slot, int actual) {
    if (!item_id || item_id[0] == '\0') return actual;
    FqEntry* e = find(item_id);
    if (!e || slot != e->cell) return actual;
    /* The changed stack holds its share of the held change. */
    int held = actual + e->display - e->actual;
    return 0 > held ? 0 : held;
}

bool fx_inventory_bar_qty_slot_visible(const char* item_id) {
    if (!item_id || item_id[0] == '\0') return true;
    FqEntry* e = find(item_id);
    return !(e && e->hidden);
}

float fx_inventory_bar_qty_slot_scale(const char* item_id, int slot) {
    if (!item_id || item_id[0] == '\0') return 1.0f;
    FqEntry* e = find(item_id);
    if (!e || slot != e->cell || e->pulse_age < 0.0f) return 1.0f;
    /* Half-sine envelope: eases up to the peak and back with no bounce. */
    float t = e->pulse_age / FQ_PULSE_TTL;
    return 1.0f + FQ_PULSE_AMP * sinf(PI * t);
}

bool fx_inventory_bar_qty_slot_pulsing(const char* item_id, int slot) {
    if (!item_id || item_id[0] == '\0') return false;
    FqEntry* e = find(item_id);
    return e && slot == e->cell && 0.0f <= e->pulse_age;
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

void fx_inventory_bar_qty_draw(Rectangle rect, const char* item_id, int slot) {
    if (!item_id || item_id[0] == '\0') return;
    FqEntry* e = find(item_id);
    if (!e || slot != e->cell || e->popup_age < 0.0f) return;
    draw_popup(e, rect);
}

void fx_inventory_bar_qty_draw_bottom(Rectangle anchor) {
    /* Hidden-bar popups launch beside the toggle (which hugs the left screen
     * edge) and travel diagonally toward the upper right — the horizontal
     * drift matches draw_popup's vertical rise — so the glyph stays fully on
     * screen. Concurrent popups stagger upward along the same diagonal. */
    int shown = 0;
    for (int i = 0; i < FQ_MAX && shown < 6; i++) {
        const FqEntry* e = &s_e[i];
        if (!e->used || e->popup_age < 0.0f) continue;

        float drift = FQ_POPUP_RISE * e->popup_age;
        Rectangle slot = {
            anchor.x + anchor.width + drift,
            anchor.y - (float)shown * (anchor.height * 0.8f),
            anchor.width, anchor.height,
        };
        draw_popup(e, slot);
        shown++;
    }
}
