/**
 * modal_notification — general-purpose transient notification toast.
 *
 * Shows a top-centre panel with a title + message + accent bar + optional
 * reward item_slot, an OK button to dismiss, and supports queuing so that
 * overlapping notifications appear one at a time.
 *
 * Architecture:
 *   - A fixed-size ring buffer holds pending notifications.
 *   - At most one notification is visible at any time.
 *   - The player dismisses it with the OK button → the next queued entry
 *     appears (or the slot goes empty).
 *   - The auto-timeout path (hold + fade) still exists for the auto-timer
 *     if the player doesn't click OK, but clicking OK is the primary action.
 *   - After the modal closes, a 0.8-second click cooldown prevents
 *     accidental clicks on elements behind the notification.
 */

#include "modal_notification.h"
#include "text.h"

#include "fx_inventory_bar_qty.h"
#include "game_state.h"
#include "item_slot.h"
#include "loot_fx.h"
#include "modal.h"
#include "modal_interact.h"
#include "inventory_modal.h"
#include "object_layer.h"
#include "object_layers_management.h"
#include "ol_as_animated_ico.h"
#include "fx_reward.h"
#include "ui_button.h"
#include "ui_icon.h"

#include <raylib.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#define MN_W          360
#define MN_FONT_TITLE 19
#define MN_FONT_BODY  13
#define MN_SLOT       76
#define MN_OK_W       96
#define MN_OK_H       34
#define MN_QUEUE_CAP  16
#define MN_PAD        18   /* inner horizontal padding (text wrap margin)  */
#define MN_TOP        16   /* top padding above the title                  */
#define MN_GAP        10   /* vertical gap between content blocks          */
#define MN_BOT        12   /* bottom padding below the OK button           */
#define MN_CLOSE_COOLDOWN 0.4f  /* seconds to block clicks after close */
#define MN_SLIDE_DURATION 0.30f /* open: left -> center; close: center -> right */

/* OK-press pulse: a quick grow-then-shrink punch, independent of the card's
 * own close-slide / delivery wait — the button vanishes on this timeline
 * regardless of how long the rest of the notification stays on screen. */
#define MN_OK_PULSE_DURATION  0.34f
#define MN_OK_PULSE_GROW_FRAC 0.3f /* fraction of the duration spent growing */
#define MN_OK_PULSE_PEAK_SCALE 1.22f

/* Reward preview arrival flourish: it pops in slightly oversized (with a soft
 * overshoot) and its slot color transitions from the notification's accent
 * back to its normal tone as it settles. */
#define MN_REWARD_POP_DUR  0.45f
#define MN_REWARD_TINT_DUR  0.90f

/* Quantity stepper (picker entries only): ◀ / ▶ around a "x" and the live
 * count, sitting between the item slot and the confirm button, with the
 * running total priced beneath the message. */
#define MN_STEP_H      40
#define MN_STEP_BTN    40
#define MN_STEP_FONT   22
#define MN_STEP_GAP    12
#define MN_STEP_HASH   "x"
#define MN_TOTAL_ICON  22
#define MN_TOTAL_FONT  17
#define MN_BTN_GAP     10

/* A picker waits for the server before animating: the confirmed items must be
 * in the inventory before the flight launches, or it would aim at the bar
 * fallback instead of the item's own slot. MN_GRANT_TIMEOUT gives up when the
 * purchase was rejected; MN_SPEND_LEAD separates the spend FX from the arrival
 * so the two read as cause and effect rather than one blur. */
#define MN_GRANT_TIMEOUT 3.0f
#define MN_SPEND_LEAD    0.25f

/* ── Queued notification entry ────────────────────────────────────────── */
typedef struct {
    char  title[96];
    char  message[160];
    char  reward_item[64];
    int   reward_qty;
    Color accent;
    /* Quantity picker — inactive when qty_max is 0. */
    int   qty_min;
    int   qty_max;
    char  price_item[64];
    int   price_qty;
    ModalNotificationConfirmFn on_confirm;
} NotifEntry;

static NotifEntry s_queue[MN_QUEUE_CAP];
static int        s_queue_head = 0;
static int        s_queue_tail = 0;

static bool  s_open = false;
static float s_age  = 0.0f;
static char  s_title[96]   = {0};
static char  s_message[160] = {0};
static Color s_accent = { 90, 200, 110, 255 };
static char  s_reward_item[64] = {0};
static int   s_reward_qty = 0;
static bool  s_reward_new = false;  /* fresh reward → fire the arrival flourish once */
static float s_reward_pop_age = 0.0f; /* time since this reward started popping in */

/* Active quantity picker; qty_max 0 means the visible entry has none. */
static int   s_qty_min = 0;
static int   s_qty_max = 0;
static char  s_price_item[64] = {0};
static int   s_price_qty = 0;
static ModalNotificationConfirmFn s_on_confirm = NULL;

/* True from a picker's OK-press until the confirmed items land in the
 * inventory (or MN_GRANT_TIMEOUT gives up on a rejected purchase). */
static bool  s_awaiting_grant = false;
static float s_grant_age      = 0.0f;
static int   s_grant_baseline = 0;
/* >= 0 once the spend FX has fired: counts down MN_SPEND_LEAD before the
 * item's slot→inventory flight launches. */
static float s_spend_age      = -1.0f;

/* Click cooldown timer — after the modal closes, clicks are swallowed for
 * MN_CLOSE_COOLDOWN seconds to prevent accidental triggers on elements
 * that were behind the notification. */
static float s_close_cooldown = 0.0f;

/* Closing slide (center -> right, on OK press): while true the card keeps
 * drawing/updating at its old entry (s_open stays true) until the slide
 * finishes, then show_next() actually advances the queue. */
static bool  s_closing    = false;
static float s_close_age  = 0.0f;

/* True from OK-press (reward case) until the delivery flight (notification
 * slot -> inventory slot, loot_fx.c) lands — the card holds at rest (no
 * slide) so the player watches the item arrive before it closes. */
static bool  s_awaiting_delivery = false;

/* OK-press pulse: true from click until MN_OK_PULSE_DURATION elapses, at
 * which point the button is gone for good (see mn_ok_pulse). */
static bool  s_ok_pulsing   = false;
static float s_ok_pulse_age = 0.0f;

static const Color C_TITLE = { 230, 235, 245, 255 };
static const Color C_BODY  = { 170, 180, 200, 230 };

static void show_next(void) {
    if (s_queue_head == s_queue_tail) {
        s_open = false;
        /* Start the close cooldown so clicks behind the notification are
         * blocked for a brief window after dismissal. */
        s_close_cooldown = MN_CLOSE_COOLDOWN;
        return;
    }
    const NotifEntry* e = &s_queue[s_queue_head];
    s_queue_head = (s_queue_head + 1) % MN_QUEUE_CAP;

    strncpy(s_title, e->title, sizeof(s_title) - 1);
    s_title[sizeof(s_title) - 1] = '\0';
    strncpy(s_message, e->message, sizeof(s_message) - 1);
    s_message[sizeof(s_message) - 1] = '\0';
    strncpy(s_reward_item, e->reward_item, sizeof(s_reward_item) - 1);
    s_reward_item[sizeof(s_reward_item) - 1] = '\0';
    s_reward_qty = e->reward_qty;
    s_qty_min    = e->qty_min;
    s_qty_max    = e->qty_max;
    s_on_confirm = e->on_confirm;
    strncpy(s_price_item, e->price_item, sizeof(s_price_item) - 1);
    s_price_item[sizeof(s_price_item) - 1] = '\0';
    s_price_qty = e->price_qty;
    s_awaiting_grant = false;
    s_grant_age = 0.0f;
    s_spend_age = -1.0f;
    s_accent = e->accent;
    s_age    = 0.0f;
    s_open   = true;
    s_reward_new = (s_reward_item[0] != '\0');
    s_reward_pop_age = 0.0f;
    /* Fresh entry starts with a clean, unpressed OK button. */
    s_ok_pulsing = false;
    s_ok_pulse_age = 0.0f;
}

static void push_queue(const char* title, const char* message, Color accent,
                       const char* reward_item_id, int reward_quantity,
                       int qty_min, int qty_max, const char* price_item_id,
                       int price_quantity, ModalNotificationConfirmFn on_confirm) {
    int next = (s_queue_tail + 1) % MN_QUEUE_CAP;
    if (next == s_queue_head) {
        /* Queue full — overwrite the oldest entry (ring semantics). */
        s_queue_head = (s_queue_head + 1) % MN_QUEUE_CAP;
    }
    NotifEntry* e = &s_queue[s_queue_tail];
    s_queue_tail = next;

    strncpy(e->title, title ? title : "", sizeof(e->title) - 1);
    e->title[sizeof(e->title) - 1] = '\0';
    strncpy(e->message, message ? message : "", sizeof(e->message) - 1);
    e->message[sizeof(e->message) - 1] = '\0';
    strncpy(e->reward_item, reward_item_id ? reward_item_id : "", sizeof(e->reward_item) - 1);
    e->reward_item[sizeof(e->reward_item) - 1] = '\0';
    e->reward_qty = reward_quantity;
    e->qty_min = qty_min;
    e->qty_max = qty_max;
    strncpy(e->price_item, price_item_id ? price_item_id : "", sizeof(e->price_item) - 1);
    e->price_item[sizeof(e->price_item) - 1] = '\0';
    e->price_qty = price_quantity;
    e->on_confirm = on_confirm;
    e->accent = accent;

    /* A reward's inventory change (and, for a first copy, its slot) stays held
     * until the OK-press delivery flight lands on the slot. */
    if (e->reward_item[0] != '\0') fx_inventory_bar_qty_hold_for_delivery(e->reward_item);

    /* Start displaying if nothing is visible right now. */
    if (!s_open) show_next();
}

void modal_notification_init(void) {
    s_open = false;
    s_queue_head = 0;
    s_queue_tail = 0;
    s_close_cooldown = 0.0f;
    s_closing = false;
    s_close_age = 0.0f;
    s_awaiting_delivery = false;
    s_awaiting_grant = false;
    s_ok_pulsing = false;
    s_ok_pulse_age = 0.0f;
    s_qty_min = 0;
    s_qty_max = 0;
    s_price_item[0] = '\0';
    s_price_qty = 0;
    s_on_confirm = NULL;
}

void modal_notification_show(const char* title, const char* message, Color accent) {
    push_queue(title, message, accent, NULL, 0, 0, 0, NULL, 0, NULL);
}

void modal_notification_show_reward(const char* title, const char* message, Color accent,
                                    const char* reward_item_id, int reward_quantity) {
    push_queue(title, message, accent, reward_item_id, reward_quantity, 0, 0, NULL, 0, NULL);
}

void modal_notification_show_picker(const char* title, const char* message, Color accent,
                                    const char* item_id, int min_quantity, int max_quantity,
                                    const char* price_item_id, int price_quantity,
                                    ModalNotificationConfirmFn on_confirm) {
    if (min_quantity < 1) min_quantity = 1;
    if (max_quantity < min_quantity) max_quantity = min_quantity;
    push_queue(title, message, accent, item_id, min_quantity,
               min_quantity, max_quantity, price_item_id, price_quantity, on_confirm);
}

/* Inner content width available for wrapped title / message text. */
static int notif_inner_w(void) { return MN_W - 2 * MN_PAD; }

/* Overshoot ease — used for the reward slot's pop-in so it briefly grows past
 * its final size before settling, reading as a small "impact". */
static float mn_ease_out_back(float t) {
    static const float c1 = 1.70158f;
    static const float c3 = c1 + 1.0f;
    float u = t - 1.0f;
    return 1.0f + c3 * u * u * u + c1 * u * u;
}

/* Slide-in ease (decelerating) and slide-out ease (accelerating). */
static float mn_ease_out_cubic(float t) {
    float u = 1.0f - t;
    return 1.0f - u * u * u;
}
static float mn_ease_in_cubic(float t) {
    return t * t * t;
}

/* 0..1 progress of whichever slide is currently active. */
static float mn_slide_progress(void) {
    float age = s_closing ? s_close_age : s_age;
    float t = age / MN_SLIDE_DURATION;
    return t < 0.0f ? 0.0f : (t > 1.0f ? 1.0f : t);
}

/* Horizontal offset applied to the resting card position: enters from the
 * left (negative offset shrinking to 0), exits to the right (offset going
 * positive past the card's own width so it fully leaves the screen). */
static float mn_slide_offset_x(float card_w) {
    float t = mn_slide_progress();
    if (s_closing) return (card_w + 60.0f) * mn_ease_in_cubic(t);
    return -(float)GetScreenWidth() * (1.0f - mn_ease_out_cubic(t));
}

/* Fade paired with the slide: fades in while entering, fades out while
 * leaving — the card is never both fully opaque and off-screen. */
static float mn_slide_alpha(void) {
    float t = mn_slide_progress();
    return s_closing ? 1.0f - mn_ease_in_cubic(t) : mn_ease_out_cubic(t);
}

/* OK-press pulse shape: grow (ease-out) to MN_OK_PULSE_PEAK_SCALE, then shrink
 * back through 0 (also ease-out, so the shrink front-loads its motion and
 * tapers gently into nothing instead of snapping away at the last instant). */
static void mn_ok_pulse(float* out_scale, float* out_alpha) {
    float t = s_ok_pulse_age / MN_OK_PULSE_DURATION;
    if (t > 1.0f) t = 1.0f;
    if (t < MN_OK_PULSE_GROW_FRAC) {
        float g = t / MN_OK_PULSE_GROW_FRAC;
        *out_scale = 1.0f + (MN_OK_PULSE_PEAK_SCALE - 1.0f) * mn_ease_out_cubic(g);
        *out_alpha = 1.0f;
    } else {
        float s = (t - MN_OK_PULSE_GROW_FRAC) / (1.0f - MN_OK_PULSE_GROW_FRAC);
        float e = mn_ease_out_cubic(s);
        *out_scale = MN_OK_PULSE_PEAK_SCALE * (1.0f - e);
        *out_alpha = 1.0f - e;
    }
}

/* True while the visible entry offers a quantity stepper. */
static bool notif_has_picker(void) { return s_qty_max > 0; }

/* Card height derived from the wrapped content so text never overflows and the
 * card grows with longer messages (and the active font size / family). */
static float notif_content_height(void) {
    int iw = notif_inner_w();
    float h = MN_TOP;
    h += (float)text_wrap(s_title, 0, 0, iw, MN_FONT_TITLE, C_TITLE, true, false);
    if (s_message[0] != '\0') {
        h += MN_GAP + (float)text_wrap(s_message, 0, 0, iw, MN_FONT_BODY, C_BODY, true, false);
    }
    if (notif_has_picker()) {
        h += MN_GAP + MN_TOTAL_ICON; /* running total, right under the message */
    }
    if (s_reward_item[0] != '\0') {
        h += MN_GAP + MN_SLOT;
    }
    if (notif_has_picker()) {
        h += MN_GAP + MN_STEP_H;
    }
    h += MN_GAP + MN_OK_H + MN_BOT;
    return h;
}

/* Screen-centred card, shared by draw + hit-test; its height is content-driven
 * (see notif_content_height). Slides in from the left on open and out to the
 * right on close (see mn_slide_offset_x) — resting position is unaffected
 * once a slide settles, so hit-testing during the resting window matches the
 * drawn position exactly. */
static Rectangle notif_card(void) {
    int sw = GetScreenWidth(), sh = GetScreenHeight();
    float h = notif_content_height();
    float x = (sw - MN_W) / 2.0f + mn_slide_offset_x(MN_W);
    return (Rectangle){ x, (sh - h) / 2.0f, MN_W, h };
}

/* Confirm button. A picker pairs it with Cancel, so the two share the bottom
 * row: Cancel on the left, the confirm on the right. */
static Rectangle notif_ok(Rectangle card) {
    float y = card.y + card.height - MN_OK_H - 12;
    if (notif_has_picker()) {
        return (Rectangle){ card.x + card.width * 0.5f + MN_BTN_GAP * 0.5f, y, MN_OK_W, MN_OK_H };
    }
    return (Rectangle){ card.x + (card.width - MN_OK_W) / 2.0f, y, MN_OK_W, MN_OK_H };
}

static Rectangle notif_cancel(Rectangle card) {
    return (Rectangle){ card.x + card.width * 0.5f - MN_BTN_GAP * 0.5f - MN_OK_W,
                        card.y + card.height - MN_OK_H - 12, MN_OK_W, MN_OK_H };
}

/* Running total row (picker only): the price sprite and `qty × unit price`,
 * directly under the per-unit message. */
static Rectangle notif_total_row(Rectangle card) {
    int iw = notif_inner_w();
    float cy = card.y + MN_TOP;
    cy += (float)text_wrap(s_title, 0, 0, iw, MN_FONT_TITLE, C_TITLE, true, false);
    if (s_message[0] != '\0') {
        cy += MN_GAP + (float)text_wrap(s_message, 0, 0, iw, MN_FONT_BODY, C_BODY, true, false);
    }
    return (Rectangle){ card.x + MN_PAD, cy + MN_GAP, (float)iw, MN_TOTAL_ICON };
}

/* Resting rect of the reward slot (below title + message + total) — shared by
 * the draw pass and the confirm-press delivery launch. */
static Rectangle notif_reward_slot(Rectangle card) {
    int iw = notif_inner_w();
    float cy = card.y + MN_TOP;
    cy += (float)text_wrap(s_title, 0, 0, iw, MN_FONT_TITLE, C_TITLE, true, false);
    if (s_message[0] != '\0') {
        cy += MN_GAP + (float)text_wrap(s_message, 0, 0, iw, MN_FONT_BODY, C_BODY, true, false);
    }
    if (notif_has_picker()) cy += MN_GAP + MN_TOTAL_ICON;
    cy += MN_GAP;
    return (Rectangle){ card.x + (card.width - MN_SLOT) / 2.0f, cy, MN_SLOT, MN_SLOT };
}

/* Stepper row, centred under the item slot: [◀] [wallet] N [▶]. The centre
 * block is measured at the widest count so the arrows never shift as the
 * player steps through the range. */
static Rectangle notif_step_row(Rectangle card) {
    Rectangle slot = notif_reward_slot(card);
    char widest[16];
    snprintf(widest, sizeof(widest), MN_STEP_HASH "%d", s_qty_max);
    float centre_w = (float)MeasureText(widest, MN_STEP_FONT);
    float row_w = 2.0f * (MN_STEP_BTN + MN_STEP_GAP) + centre_w;
    return (Rectangle){ card.x + (card.width - row_w) * 0.5f,
                        slot.y + slot.height + MN_GAP, row_w, MN_STEP_H };
}

static Rectangle notif_step_dec(Rectangle card) {
    Rectangle row = notif_step_row(card);
    return (Rectangle){ row.x, row.y, MN_STEP_BTN, row.height };
}

static Rectangle notif_step_inc(Rectangle card) {
    Rectangle row = notif_step_row(card);
    return (Rectangle){ row.x + row.width - MN_STEP_BTN, row.y, MN_STEP_BTN, row.height };
}

/* Clamp and apply a stepper delta; no-op once an end of the range is reached. */
static void notif_step_quantity(int delta) {
    int next = s_reward_qty + delta;
    if (next < s_qty_min) next = s_qty_min;
    if (next > s_qty_max) next = s_qty_max;
    s_reward_qty = next;
}

void modal_notification_update(float dt) {
    /* Tick down the close cooldown even while the modal is closed. */
    if (s_close_cooldown > 0.0f) {
        s_close_cooldown -= dt;
        if (s_close_cooldown < 0.0f) s_close_cooldown = 0.0f;
    }

    if (!s_open) return;

    /* OK-press pulse runs on its own clock, independent of the close-slide /
     * delivery-wait branches below — the button vanishes on this timeline
     * regardless of how long the rest of the card stays on screen. */
    if (s_ok_pulsing) {
        s_ok_pulse_age += dt;
        if (s_ok_pulse_age >= MN_OK_PULSE_DURATION) s_ok_pulsing = false;
    }

    /* Close-slide in progress: keep drawing at the sliding position, but skip
     * the open-state logic below (reward pop-in, holds) until it finishes,
     * at which point the queue actually advances. */
    if (s_closing) {
        s_close_age += dt;
        if (s_close_age >= MN_SLIDE_DURATION) {
            s_closing = false;
            s_close_age = 0.0f;
            show_next();
        }
        return;
    }

    /* Reward delivery in flight: hold the card at rest (no slide) and keep
     * its inventory hold fresh until the item lands on its slot, then play
     * the close-slide (see loot_fx_inbound_to_inventory). Falls straight
     * through to closing on the very frame the flight never actually
     * started (e.g. no inventory slot resolved for the item). */
    if (s_awaiting_delivery) {
        if (s_reward_item[0] != '\0') fx_inventory_bar_qty_hold_for_delivery(s_reward_item);
        if (!loot_fx_inbound_to_inventory(s_reward_item)) {
            s_awaiting_delivery = false;
            s_closing = true;
            s_close_age = 0.0f;
        }
        return;
    }

    /* Confirmed purchase: hold the card until the server's grant lands in the
     * inventory. Launching the flight at OK-press would aim at the bar
     * fallback, since a first copy has no slot until the items arrive. The
     * spend FX is released on arrival and leads the item in by MN_SPEND_LEAD,
     * so the currency visibly leaves before the goods drop in. */
    if (s_awaiting_grant) {
        if (s_reward_item[0] != '\0') fx_inventory_bar_qty_hold_for_delivery(s_reward_item);
        s_grant_age += dt;
        if (s_spend_age >= 0.0f) {
            /* Re-asserted every frame of the lead: releasing is a no-op once
             * it has fired, so this lands the spend FX on the first frame the
             * quantity FX has actually registered the debit — no dependency on
             * module update order. */
            if (s_price_item[0] != '\0') fx_inventory_bar_qty_notify_arrival(s_price_item);
            s_spend_age += dt;
            if (s_spend_age >= MN_SPEND_LEAD) {
                Rectangle slot = notif_reward_slot(notif_card());
                loot_fx_reward_delivery(s_reward_item, slot.x + slot.width * 0.5f,
                                        slot.y + slot.height * 0.5f);
                s_awaiting_grant = false;
                s_awaiting_delivery = true;
            }
        } else if (game_state_item_quantity(s_reward_item) > s_grant_baseline) {
            /* The grant landed: the price item's "-N" popup and outward spray
             * play first, while the bought item is still sitting in the card. */
            s_spend_age = 0.0f;
        } else if (s_grant_age >= MN_GRANT_TIMEOUT) {
            /* Rejected or dropped — release the hold and leave with no FX. */
            if (s_reward_item[0] != '\0') fx_inventory_bar_qty_notify_arrival(s_reward_item);
            s_awaiting_grant = false;
            s_closing = true;
            s_close_age = 0.0f;
        }
        return;
    }

    s_age += dt; /* drives the open slide-in; dismissal is OK-only */
    /* Celebrate a gift, never a trade: a picker is the player spending their
     * own currency, so it gets the plain card without the reward stars. */
    if (s_reward_item[0] != '\0') {
        if (!notif_has_picker()) {
            Rectangle card = notif_card();
            if (s_reward_new) { fx_reward_trigger(card); s_reward_new = false; }
            fx_reward_show(card);
        }
        s_reward_pop_age += dt;
    }

    /* Keep the visible + queued reward holds alive while the player reads —
     * they release when the OK-press delivery lands on the inventory slot. */
    if (s_reward_item[0] != '\0') fx_inventory_bar_qty_hold_for_delivery(s_reward_item);
    for (int i = s_queue_head; i != s_queue_tail; i = (i + 1) % MN_QUEUE_CAP) {
        if (s_queue[i].reward_item[0] != '\0')
            fx_inventory_bar_qty_hold_for_delivery(s_queue[i].reward_item);
    }
}

void modal_notification_draw(void) {
    if (!s_open) return;

    float a = mn_slide_alpha();
    Rectangle card = notif_card();

    /* Dim the screen so a stacked notification reads as modal. */
    DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(),
                  (Color){ 0, 0, 0, (unsigned char)(90 * a) });

    /* Celebration behind the card — frames it without covering the text. */
    fx_reward_draw();

    Color bg = MODAL_PANEL_BG;
    bg.a = (unsigned char)(245 * a);
    DrawRectangleRec(card, bg);
    Color border = s_accent; /* accent now tints the whole frame, not a side bar */
    border.a = (unsigned char)(border.a * a);
    DrawRectangleLinesEx(card, 2.0f, border);

    Color title = C_TITLE; title.a = (unsigned char)(title.a * a);
    Color body  = C_BODY;  body.a  = (unsigned char)(body.a * a);

    int   ix = (int)(card.x + MN_PAD);
    int   iw = notif_inner_w();

    /* Title — wrapped + centred. */
    float cy = card.y + MN_TOP;
    cy += (float)text_wrap(s_title, ix, (int)cy, iw, MN_FONT_TITLE, title, true, true);

    /* Message — wrapped + centred. */
    if (s_message[0] != '\0') {
        cy += MN_GAP;
        cy += (float)text_wrap(s_message, ix, (int)cy, iw, MN_FONT_BODY, body, true, true);
    }

    /* Running total — the price item's own sprite beside what this purchase
     * costs at the selected count, so "10 coin each" always has its sum. */
    if (notif_has_picker()) {
        Rectangle row = notif_total_row(card);
        char total[24];
        snprintf(total, sizeof(total), "%d", s_price_qty * s_reward_qty);
        float total_w = MN_TOTAL_ICON + 6.0f + (float)MeasureText(total, MN_TOTAL_FONT);
        float total_x = row.x + (row.width - total_w) * 0.5f;
        ol_as_ico_draw(obj_layers_mgr_get(), s_price_item, (int)total_x, (int)row.y,
                       MN_TOTAL_ICON, OL_ICO_DEFAULT_DIR, 0, Fade(WHITE, a));
        Color total_c = { 255, 215, 0, (unsigned char)(230.0f * a) };
        DrawText(total, (int)(total_x + MN_TOTAL_ICON + 6.0f),
                 (int)(row.y + (MN_TOTAL_ICON - (float)text_line_height(MN_TOTAL_FONT)) * 0.5f),
                 MN_TOTAL_FONT, total_c);
    }

    /* Reward item slot — centred, pops in oversized (soft overshoot) and its
     * color transitions from the notification's accent back to normal as it
     * settles, so a fresh reward reads as a distinct, celebratory arrival.
     * Once the delivery flight launches (s_awaiting_delivery), the icon
     * itself leaves the slot empty — the loot_fx token is what's seen flying
     * on to the inventory, so the item must vanish here to sell that motion.
     * For a reward, s_closing only starts once that flight has already
     * landed (see modal_notification_update), so the slot stays empty
     * through the close-slide too instead of the item reappearing. */
    if (s_reward_item[0] != '\0') {
        Rectangle slot = notif_reward_slot(card);

        if (s_awaiting_delivery || s_closing) {
            ObjectLayerState empty_ol = { 0 };
            item_slot_draw_ex(slot, &empty_ol, obj_layers_mgr_get(), s_accent, 0.0f, true);
        } else {
            float pop_t  = fminf(1.0f, s_reward_pop_age / MN_REWARD_POP_DUR);
            float pop    = mn_ease_out_back(pop_t);
            float scale  = 0.55f + 0.45f * pop;
            float sw = MN_SLOT * scale, sh = MN_SLOT * scale;
            Rectangle pop_slot = { slot.x + (slot.width - sw) * 0.5f,
                                   slot.y + (slot.height - sh) * 0.5f, sw, sh };

            float tint_t = 1.0f - fminf(1.0f, s_reward_pop_age / MN_REWARD_TINT_DUR);

            ObjectLayerState ol = { 0 };
            strncpy(ol.item_id, s_reward_item, sizeof(ol.item_id) - 1);
            ol.active = true;
            ol.quantity = s_reward_qty;
            item_slot_draw_ex(pop_slot, &ol, obj_layers_mgr_get(), s_accent, tint_t, true);
        }
    }

    /* Quantity stepper — drawn only while the entry is still answerable; once
     * OK is pressed the choice is locked and the row leaves with the slot. */
    if (notif_has_picker() && !s_awaiting_grant && !s_awaiting_delivery && !s_closing) {
        int smx = GetMouseX(), smy = GetMouseY();
        Rectangle dec = notif_step_dec(card);
        Rectangle inc = notif_step_inc(card);
        bool can_dec = s_reward_qty > s_qty_min;
        bool can_inc = s_reward_qty < s_qty_max;

        /* An arrow at the end of its range is disabled outright — muted fill,
         * no hover response, no click. */
        UIButtonPixelRetroStyle arrow = { .font_size = MN_FONT_BODY };
        arrow.icon_id = "arrow-left";
        arrow.enabled = can_dec;
        arrow.bg = (Color){ 50, 55, 80, (unsigned char)((can_dec ? 235.0f : 90.0f) * a) };
        ui_button_pixel_retro_draw(dec, &arrow, can_dec && ui_button_hit(dec, smx, smy));
        arrow.icon_id = "arrow-right";
        arrow.enabled = can_inc;
        arrow.bg = (Color){ 50, 55, 80, (unsigned char)((can_inc ? 235.0f : 90.0f) * a) };
        ui_button_pixel_retro_draw(inc, &arrow, can_inc && ui_button_hit(inc, smx, smy));

        Rectangle row = notif_step_row(card);
        char count[16];
        snprintf(count, sizeof(count), MN_STEP_HASH "%d", s_reward_qty);
        Color count_c = C_TITLE;
        count_c.a = (unsigned char)((float)count_c.a * a);
        float count_x = row.x + (row.width - (float)MeasureText(count, MN_STEP_FONT)) * 0.5f;
        DrawText(count, (int)count_x,
                 (int)(row.y + (row.height - (float)text_line_height(MN_STEP_FONT)) * 0.5f),
                 MN_STEP_FONT, count_c);
    }

    /* Confirm button — bottom centre, pixel-retro style. A picker is answering
     * "how many do I buy?", so it reads "Buy"; everything else acknowledges
     * with "OK". Once pressed it plays a quick grow-then-shrink pulse and is
     * gone for good after MN_OK_PULSE_DURATION, independent of the close-slide
     * / delivery wait the rest of the card may still be playing. */
    const char* ok_label = notif_has_picker() ? "Buy" : "OK";
    const char* ok_icon  = notif_has_picker() ? "wallet" : "check";
    Rectangle ok_r = notif_ok(card);
    if (s_ok_pulsing) {
        float pscale, palpha;
        mn_ok_pulse(&pscale, &palpha);
        /* Skip once faint enough that a rounded-to-zero text alpha would
         * fall back to opaque white (see ui_button_pixel_retro_draw) — the
         * button has effectively vanished by this point anyway. */
        if (pscale > 0.02f && palpha > 0.02f) {
            Rectangle pulse_r = {
                ok_r.x + ok_r.width  * 0.5f * (1.0f - pscale),
                ok_r.y + ok_r.height * 0.5f * (1.0f - pscale),
                ok_r.width  * pscale,
                ok_r.height * pscale,
            };
            UIButtonPixelRetroStyle pulse_btn = {
                .bg = (Color){ 50, 55, 80, (unsigned char)(235.0f * palpha) },
                .icon_id = ok_icon,
                .label = ok_label,
                .font_size = 15,
                .text_color = (Color){ C_TITLE.r, C_TITLE.g, C_TITLE.b,
                                       (unsigned char)((float)C_TITLE.a * palpha) },
                .selected = false,
                .enabled = false,
            };
            ui_button_pixel_retro_draw(pulse_r, &pulse_btn, false);
        }
    } else if (!s_closing && !s_awaiting_grant && !s_awaiting_delivery) {
        int mx = GetMouseX(), my = GetMouseY();
        UIButtonPixelRetroStyle ok_btn = {
            .bg = notif_has_picker() ? (Color){ 38, 138, 76, 235 } : (Color){ 50, 55, 80, 235 },
            .icon_id = ok_icon,
            .label = ok_label,
            .font_size = 15,
            .text_color = C_TITLE,
            .selected = false,
            .enabled = true,
        };
        ui_button_pixel_retro_draw(ok_r, &ok_btn, ui_button_hit(ok_r, mx, my));

        /* A purchase is refusable: pair the confirm with a Cancel that closes
         * the card with nothing sent. */
        if (notif_has_picker()) {
            Rectangle cancel_r = notif_cancel(card);
            UIButtonPixelRetroStyle cancel_btn = {
                .bg = (Color){ 90, 50, 58, 235 },
                .icon_id = "close",
                .label = "Cancel",
                .font_size = 15,
                .text_color = C_TITLE,
                .selected = false,
                .enabled = true,
            };
            ui_button_pixel_retro_draw(cancel_r, &cancel_btn, ui_button_hit(cancel_r, mx, my));
        }
    }
}

bool modal_notification_is_open(void) {
    return s_open;
}

bool modal_notification_is_on_cooldown(void) {
    return s_close_cooldown > 0.0f;
}

bool modal_notification_handle_click(int mx, int my) {
    /* During the close cooldown, swallow clicks only when there is a modal
     * behind the notification that could accidentally receive them (interact
     * modal's quest cards, inventory modal buttons, etc.). Without a modal
     * behind, clicks pass through normally. */
    if (s_close_cooldown > 0.0f &&
        (modal_interact_is_open() || inventory_modal_is_open())) {
        return true;
    }
    if (s_close_cooldown > 0.0f) return false;

    if (!s_open) return false;
    /* Close-slide or delivery wait already playing — swallow further clicks
     * (no double-fire on the OK button, nothing behind should react either). */
    if (s_closing || s_awaiting_grant || s_awaiting_delivery) return true;
    Rectangle card = notif_card();

    /* Quantity stepper — adjusts the pending count without dismissing. */
    if (notif_has_picker()) {
        if (ui_button_hit(notif_step_dec(card), mx, my)) { notif_step_quantity(-1); return true; }
        if (ui_button_hit(notif_step_inc(card), mx, my)) { notif_step_quantity(+1); return true; }
        /* Cancel — nothing is sent, and the item's held quantity FX is released
         * with no change so no popup fires. */
        if (ui_button_hit(notif_cancel(card), mx, my)) {
            s_on_confirm = NULL;
            if (s_reward_item[0] != '\0') fx_inventory_bar_qty_notify_arrival(s_reward_item);
            s_closing = true;
            s_close_age = 0.0f;
            return true;
        }
    }

    Rectangle ok_r = notif_ok(card);
    /* Only the OK button is clickable; all other clicks are swallowed
     * so buttons behind the notification modal are not accidentally
     * triggered. */
    if ((float)mx >= ok_r.x && (float)mx < ok_r.x + ok_r.width &&
        (float)my >= ok_r.y && (float)my < ok_r.y + ok_r.height) {
        /* Grow-then-shrink punch, then gone for good — runs on its own clock
         * regardless of how long the card itself stays on screen after. */
        s_ok_pulsing = true;
        s_ok_pulse_age = 0.0f;
        /* A picker hands its confirmed count to the opener and then waits for
         * the server's grant before animating anything — see the
         * s_awaiting_grant branch in modal_notification_update. */
        if (s_on_confirm) {
            ModalNotificationConfirmFn confirm = s_on_confirm;
            s_on_confirm = NULL;
            s_grant_baseline = game_state_item_quantity(s_reward_item);
            s_grant_age = 0.0f;
            s_spend_age = -1.0f;
            s_awaiting_grant = true;
            confirm(s_reward_item, s_reward_qty);
            return true;
        }
        if (s_reward_item[0] != '\0') {
            /* Reward arrival — the same presentation as a world pickup,
             * launched from the notification's reward slot. Landing releases
             * the held slot reveal / +N popup / pulse. The card holds at rest
             * until the flight lands (modal_notification_update), so the
             * player watches the item travel before the notification closes. */
            Rectangle slot = notif_reward_slot(card);
            loot_fx_reward_delivery(s_reward_item,
                                    slot.x + slot.width * 0.5f,
                                    slot.y + slot.height * 0.5f);
            s_awaiting_delivery = true;
        } else {
            /* Nothing to deliver — play the center -> right close slide right
             * away; show_next() fires once it finishes. */
            s_closing = true;
            s_close_age = 0.0f;
        }
        return true;
    }
    return true; /* swallow all clicks while a notification is up */
}