/**
 * modal_notification — general-purpose transient notification toast.
 *
 * Shows a top-centre panel with a title + message + accent bar + optional
 * result slots, a confirm button, and supports queuing so that overlapping
 * notifications appear one at a time.
 *
 * Four shapes, each additive over the last:
 *   plain     title + message + OK.
 *   reward    plus the granted item's slot; OK flies it into the inventory.
 *   picker    plus a − / + quantity stepper and a running total; Cancel / Buy.
 *   assemble  plus the consumed inputs stacked over a progress bar charging
 *             over the recipe's duration over the produced outputs, framed by
 *             fx_assemble's electric field, and a Cancel that aborts before it
 *             completes.
 *
 * Architecture:
 *   - A fixed-size ring buffer holds pending notifications.
 *   - At most one notification is visible at any time.
 *   - Dismissing advances the queue (or the slot goes empty).
 *   - After the modal closes, a short click cooldown prevents accidental
 *     clicks on elements behind the notification.
 */

#include "modal_notification.h"
#include "text.h"

#include "fx_assemble.h"
#include "fx_grant_delivery.h"
#include "fx_inventory_bar_qty.h"
#include "item_slot.h"
#include "loot_fx.h"
#include "domain/local_player.h"
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
#define MN_SLOT_GAP   8
#define MN_OK_W       96
#define MN_OK_H       34
#define MN_QUEUE_CAP  16
#define MN_ITEMS_MAX  6
#define MN_PAD        18   /* inner horizontal padding (text wrap margin)  */
#define MN_TOP        16   /* top padding above the title                  */
#define MN_GAP        10   /* vertical gap between content blocks          */
#define MN_BOT        12   /* bottom padding below the button row          */
#define MN_CLOSE_COOLDOWN 0.4f  /* seconds to block clicks after close */
#define MN_SLIDE_DURATION 0.30f /* open: left -> center; close: center -> right */

/* Confirm-press pulse: a quick grow-then-shrink punch, independent of the
 * card's own close-slide / delivery wait — the button vanishes on this timeline
 * regardless of how long the rest of the notification stays on screen. */
#define MN_OK_PULSE_DURATION  0.34f
#define MN_OK_PULSE_GROW_FRAC 0.3f /* fraction of the duration spent growing */
#define MN_OK_PULSE_PEAK_SCALE 1.22f

/* Reward preview arrival flourish: it pops in slightly oversized (with a soft
 * overshoot) and its slot color transitions from the notification's accent
 * back to its normal tone as it settles. */
#define MN_REWARD_POP_DUR  0.45f
#define MN_REWARD_TINT_DUR  0.90f

/* Quantity stepper (picker entries only): − / + flanking the item slot, whose
 * own quantity badge is the count, with the running total priced under it. */
#define MN_STEP_BTN    40
#define MN_STEP_FONT   24
#define MN_STEP_GAP    12
#define MN_TOTAL_ICON  22
#define MN_TOTAL_FONT  17
#define MN_BTN_GAP     10

/* Assembly progress bar; the surrounding electric field is fx_assemble. */
#define MN_BAR_H 18

static const Color C_TITLE    = { 230, 235, 245, 255 };
static const Color C_BODY     = { 170, 180, 200, 230 };
static const Color C_ELECTRIC = {  90, 220, 245, 255 }; /* cyan synthesis */

/* ── Queued notification entry ────────────────────────────────────────── */

typedef struct {
    char item_id[64];
    int  qty;
} NotifItem;

typedef struct {
    char      title[96];
    char      message[160];
    NotifItem items[MN_ITEMS_MAX];
    int       item_count;
    Color     accent;
    /* Quantity picker — inactive when qty_max is 0. */
    int   qty_min;
    int   qty_max;
    char  price_item[64];
    int   price_qty;
    ModalNotificationConfirmFn on_confirm;
    /* Assembly — inactive when craft_seconds is 0. */
    float     craft_seconds;
    NotifItem inputs[MN_ITEMS_MAX];
    int       input_count;
    ModalNotificationCancelFn on_cancel;
} NotifEntry;

static NotifEntry s_queue[MN_QUEUE_CAP];
static int        s_queue_head = 0;
static int        s_queue_tail = 0;

static bool  s_open = false;
static float s_age  = 0.0f;
static char  s_title[96]   = {0};
static char  s_message[160] = {0};
static Color s_accent = { 90, 200, 110, 255 };
static NotifItem s_items[MN_ITEMS_MAX];
static int   s_item_count = 0;
static bool  s_reward_new = false;  /* fresh reward → fire the arrival flourish once */
static float s_reward_pop_age = 0.0f; /* time since the results started popping in */

/* Active quantity picker; qty_max 0 means the visible entry has none. */
static int   s_qty_min = 0;
static int   s_qty_max = 0;
static char  s_price_item[64] = {0};
static int   s_price_qty = 0;
static ModalNotificationConfirmFn s_on_confirm = NULL;

/* Active assembly; craft_total 0 means the visible entry is not one. */
static float s_craft_total = 0.0f;
static float s_craft_age   = 0.0f;
static NotifItem s_inputs[MN_ITEMS_MAX];
static int   s_input_count = 0;
static ModalNotificationCancelFn s_on_cancel = NULL;

/* True from a confirm-press until fx_grant_delivery has sequenced the server's
 * grant into the inventory (or given up on a rejected transaction). */
static bool  s_awaiting_grant = false;

/* Click cooldown timer — after the modal closes, clicks are swallowed for
 * MN_CLOSE_COOLDOWN seconds to prevent accidental triggers on elements
 * that were behind the notification. */
static float s_close_cooldown = 0.0f;

/* Closing slide (center -> right): while true the card keeps drawing/updating
 * at its old entry (s_open stays true) until the slide finishes, then
 * show_next() actually advances the queue. */
static bool  s_closing    = false;
static float s_close_age  = 0.0f;

/* True until the delivery flights (notification slot -> inventory slot,
 * loot_fx.c) land — the card holds at rest so the player watches the items
 * arrive before it closes. */
static bool  s_awaiting_delivery = false;

/* Confirm-press pulse: true from click until MN_OK_PULSE_DURATION elapses, at
 * which point the button is gone for good (see mn_ok_pulse). */
static bool  s_ok_pulsing   = false;
static float s_ok_pulse_age = 0.0f;

/* ── Mode predicates ──────────────────────────────────────────────────── */

static bool notif_has_picker(void)    { return s_qty_max > 0; }
static bool notif_is_assembling(void) { return s_craft_total > 0.0f; }

/* ── Queue ────────────────────────────────────────────────────────────── */

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
    memcpy(s_items, e->items, sizeof(s_items));
    s_item_count = e->item_count;
    s_qty_min    = e->qty_min;
    s_qty_max    = e->qty_max;
    s_on_confirm = e->on_confirm;
    strncpy(s_price_item, e->price_item, sizeof(s_price_item) - 1);
    s_price_item[sizeof(s_price_item) - 1] = '\0';
    s_price_qty = e->price_qty;
    memcpy(s_inputs, e->inputs, sizeof(s_inputs));
    s_input_count  = e->input_count;
    s_craft_total  = e->craft_seconds;
    s_craft_age    = 0.0f;
    s_on_cancel    = e->on_cancel;
    s_awaiting_grant = false;
    s_accent = e->accent;
    s_age    = 0.0f;
    s_open   = true;
    s_reward_new = (s_item_count > 0);
    s_reward_pop_age = 0.0f;
    /* Fresh entry starts with a clean, unpressed confirm button. */
    s_ok_pulsing = false;
    s_ok_pulse_age = 0.0f;
}

static NotifEntry* push_entry(const char* title, const char* message, Color accent) {
    int next = (s_queue_tail + 1) % MN_QUEUE_CAP;
    if (next == s_queue_head) {
        /* Queue full — overwrite the oldest entry (ring semantics). */
        s_queue_head = (s_queue_head + 1) % MN_QUEUE_CAP;
    }
    NotifEntry* e = &s_queue[s_queue_tail];
    s_queue_tail = next;

    memset(e, 0, sizeof(*e));
    strncpy(e->title, title ? title : "", sizeof(e->title) - 1);
    strncpy(e->message, message ? message : "", sizeof(e->message) - 1);
    e->accent = accent;
    return e;
}

static void add_entry_item(NotifEntry* e, const char* item_id, int qty) {
    if (!item_id || '\0' == item_id[0] || e->item_count >= MN_ITEMS_MAX) return;
    NotifItem* it = &e->items[e->item_count++];
    strncpy(it->item_id, item_id, sizeof(it->item_id) - 1);
    it->qty = qty;
    /* The gained quantity (and, for a first copy, its slot) stays held until
     * the delivery flight lands on it. */
    fx_inventory_bar_qty_hold_for_delivery(it->item_id);
}

/* Show the entry now when nothing else is up. */
static void present(void) {
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
    s_item_count = 0;
    s_qty_min = 0;
    s_qty_max = 0;
    s_price_item[0] = '\0';
    s_price_qty = 0;
    s_on_confirm = NULL;
    s_craft_total = 0.0f;
    s_input_count = 0;
    s_on_cancel = NULL;
}

void modal_notification_show(const char* title, const char* message, Color accent) {
    push_entry(title, message, accent);
    present();
}

void modal_notification_show_reward(const char* title, const char* message, Color accent,
                                    const char* reward_item_id, int reward_quantity) {
    NotifEntry* e = push_entry(title, message, accent);
    add_entry_item(e, reward_item_id, reward_quantity);
    present();
}

void modal_notification_show_picker(const char* title, const char* message, Color accent,
                                    const char* item_id, int min_quantity, int max_quantity,
                                    const char* price_item_id, int price_quantity,
                                    ModalNotificationConfirmFn on_confirm) {
    if (min_quantity < 1) min_quantity = 1;
    if (max_quantity < min_quantity) max_quantity = min_quantity;

    NotifEntry* e = push_entry(title, message, accent);
    add_entry_item(e, item_id, min_quantity);
    e->qty_min = min_quantity;
    e->qty_max = max_quantity;
    strncpy(e->price_item, price_item_id ? price_item_id : "", sizeof(e->price_item) - 1);
    e->price_qty = price_quantity;
    e->on_confirm = on_confirm;
    present();
}

void modal_notification_show_assemble(const ModalNotificationAssemble* a) {
    if (!a || a->output_count <= 0) return;

    NotifEntry* e = push_entry(a->title, a->message, a->accent);
    for (int i = 0; i < a->output_count; i++) {
        add_entry_item(e, a->outputs[i].item_id, a->outputs[i].quantity);
    }
    for (int i = 0; i < a->input_count && e->input_count < MN_ITEMS_MAX; i++) {
        if (!a->inputs[i].item_id || '\0' == a->inputs[i].item_id[0]) continue;
        NotifItem* in = &e->inputs[e->input_count++];
        strncpy(in->item_id, a->inputs[i].item_id, sizeof(in->item_id) - 1);
        in->qty = a->inputs[i].quantity;
    }
    e->craft_seconds = a->craft_seconds > 0.0f ? a->craft_seconds : 0.01f;
    e->on_cancel = a->on_cancel;
    present();
}

void modal_notification_set_assemble_duration(float craft_seconds) {
    if (!notif_is_assembling() || craft_seconds <= 0.0f) return;
    s_craft_total = craft_seconds;
    if (s_craft_age > s_craft_total) s_craft_age = s_craft_total;
}

/* ── Geometry ─────────────────────────────────────────────────────────── */

/* Inner content width available for wrapped title / message text. */
static int notif_inner_w(void) { return MN_W - 2 * MN_PAD; }

/* Slot side length for the result row — full size for one item, shrunk to fit
 * once several share the row. */
static float notif_slot_size(void) {
    int widest = s_item_count > s_input_count ? s_item_count : s_input_count;
    if (widest <= 1) return MN_SLOT;
    float fit = ((float)notif_inner_w() - (float)(widest - 1) * MN_SLOT_GAP) / (float)widest;
    return fit < MN_SLOT ? fit : MN_SLOT;
}

/* Overshoot ease — used for the result slots' pop-in so they briefly grow past
 * their final size before settling, reading as a small "impact". */
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

/* Confirm-press pulse shape: grow (ease-out) to MN_OK_PULSE_PEAK_SCALE, then
 * shrink back through 0 (also ease-out, so the shrink front-loads its motion
 * and tapers gently into nothing instead of snapping away at the last
 * instant). */
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

/* Card height derived from the wrapped content so text never overflows and the
 * card grows with longer messages (and the active font size / family). */
static float notif_content_height(void) {
    int iw = notif_inner_w();
    float h = MN_TOP;
    h += (float)text_wrap(s_title, 0, 0, iw, MN_FONT_TITLE, C_TITLE, true, false);
    if (s_message[0] != '\0') {
        h += MN_GAP + (float)text_wrap(s_message, 0, 0, iw, MN_FONT_BODY, C_BODY, true, false);
    }
    if (notif_is_assembling() && s_input_count > 0) h += MN_GAP + notif_slot_size();
    if (notif_is_assembling())                      h += MN_GAP + MN_BAR_H;
    if (s_item_count > 0)                           h += MN_GAP + notif_slot_size();
    if (notif_has_picker())                         h += MN_GAP + MN_TOTAL_ICON;
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

/* Content baseline under the title + message, where the mode-specific blocks
 * start stacking. */
static float notif_body_top(Rectangle card) {
    int iw = notif_inner_w();
    float cy = card.y + MN_TOP;
    cy += (float)text_wrap(s_title, 0, 0, iw, MN_FONT_TITLE, C_TITLE, true, false);
    if (s_message[0] != '\0') {
        cy += MN_GAP + (float)text_wrap(s_message, 0, 0, iw, MN_FONT_BODY, C_BODY, true, false);
    }
    return cy;
}

/* Rect of slot `index` in a centred row of `count` slots whose top is at `top`. */
static Rectangle notif_row_slot(Rectangle card, float top, int count, int index) {
    float size = notif_slot_size();
    float row_w = (float)count * size + (float)(count - 1) * MN_SLOT_GAP;
    return (Rectangle){ card.x + (card.width - row_w) * 0.5f + (float)index * (size + MN_SLOT_GAP),
                        top, size, size };
}

/* Consumed-input slots (assembly only), directly under the message. */
static Rectangle notif_input_slot(Rectangle card, int index) {
    return notif_row_slot(card, notif_body_top(card) + MN_GAP, s_input_count, index);
}

/* Assembly progress bar, between the inputs it consumes and the outputs it
 * produces — the bar is the transformation between the two rows. */
static Rectangle notif_bar(Rectangle card) {
    float top = notif_body_top(card);
    if (s_input_count > 0) top += MN_GAP + notif_slot_size();
    return (Rectangle){ card.x + MN_PAD, top + MN_GAP, (float)notif_inner_w(), MN_BAR_H };
}

/* Resting rect of result slot `index` — shared by the draw pass and the
 * delivery launches. */
static Rectangle notif_item_slot(Rectangle card, int index) {
    float top = notif_body_top(card);
    if (notif_is_assembling()) {
        if (s_input_count > 0) top += MN_GAP + notif_slot_size();
        top += MN_GAP + MN_BAR_H;
    }
    return notif_row_slot(card, top + MN_GAP, s_item_count, index);
}

/* Running total row (picker only): the price sprite and `qty × unit price`,
 * directly under the item slot. */
static Rectangle notif_total_row(Rectangle card) {
    Rectangle slot = notif_item_slot(card, 0);
    return (Rectangle){ card.x + MN_PAD, slot.y + slot.height + MN_GAP,
                        (float)notif_inner_w(), MN_TOTAL_ICON };
}

/* Bottom row. A picker pairs Cancel with the confirm; an assembly offers only
 * Cancel; everything else only the confirm. A zeroed rect means "absent". */
static void notif_buttons(Rectangle card, Rectangle* cancel, Rectangle* confirm) {
    float y = card.y + card.height - MN_OK_H - 12;
    *cancel = (Rectangle){ 0 };
    *confirm = (Rectangle){ 0 };
    if (notif_is_assembling()) {
        *cancel = (Rectangle){ card.x + (card.width - MN_OK_W) / 2.0f, y, MN_OK_W, MN_OK_H };
        return;
    }
    if (notif_has_picker()) {
        *cancel  = (Rectangle){ card.x + card.width * 0.5f - MN_BTN_GAP * 0.5f - MN_OK_W,
                                y, MN_OK_W, MN_OK_H };
        *confirm = (Rectangle){ card.x + card.width * 0.5f + MN_BTN_GAP * 0.5f, y, MN_OK_W, MN_OK_H };
        return;
    }
    *confirm = (Rectangle){ card.x + (card.width - MN_OK_W) / 2.0f, y, MN_OK_W, MN_OK_H };
}

/* The − / + steppers flank the slot, vertically centred on it. The slot's own
 * quantity badge carries the count, so the row needs no separate readout. */
static Rectangle notif_step_dec(Rectangle card) {
    Rectangle slot = notif_item_slot(card, 0);
    return (Rectangle){ slot.x - MN_STEP_GAP - MN_STEP_BTN,
                        slot.y + (slot.height - MN_STEP_BTN) * 0.5f,
                        MN_STEP_BTN, MN_STEP_BTN };
}

static Rectangle notif_step_inc(Rectangle card) {
    Rectangle slot = notif_item_slot(card, 0);
    return (Rectangle){ slot.x + slot.width + MN_STEP_GAP,
                        slot.y + (slot.height - MN_STEP_BTN) * 0.5f,
                        MN_STEP_BTN, MN_STEP_BTN };
}

/* Clamp and apply a stepper delta; no-op once an end of the range is reached. */
static void notif_step_quantity(int delta) {
    int next = s_items[0].qty + delta;
    if (next < s_qty_min) next = s_qty_min;
    if (next > s_qty_max) next = s_qty_max;
    s_items[0].qty = next;
}

/* ── Result lifecycle ─────────────────────────────────────────────────── */

static void hold_result_items(void) {
    for (int i = 0; i < s_item_count; i++) fx_inventory_bar_qty_hold_for_delivery(s_items[i].item_id);
}

/* Release the held changes with no delta, so the card can leave without firing
 * a popup for items that never arrived. */
static void release_result_items(void) {
    for (int i = 0; i < s_item_count; i++) fx_inventory_bar_qty_notify_arrival(s_items[i].item_id);
}

static bool results_inbound(void) {
    for (int i = 0; i < s_item_count; i++) {
        if (loot_fx_inbound_to_inventory(s_items[i].item_id)) return true;
    }
    return false;
}

/* Hand every result to fx_grant_delivery, each flying from its own slot. */
static void begin_result_delivery(Rectangle card, const char* const* spent, int spent_count) {
    FxGrantGain gains[MN_ITEMS_MAX];
    for (int i = 0; i < s_item_count; i++) {
        Rectangle slot = notif_item_slot(card, i);
        gains[i].item_id = s_items[i].item_id;
        gains[i].origin  = (Vector2){ slot.x + slot.width * 0.5f, slot.y + slot.height * 0.5f };
    }
    fx_grant_delivery_begin(gains, s_item_count, spent, spent_count);
    s_awaiting_grant = true;
}

static void start_closing(void) {
    s_closing = true;
    s_close_age = 0.0f;
}

/* An assembly runs on top of an open interact session, so it participates in
 * the freeze chain the same way modal_dialogue does: hand the freeze back to
 * "interact" when the card is done, and never leave the player exposed between
 * the two. */
static void assemble_release_freeze(void) {
    if (modal_interact_is_open()) local_player_request_freeze(true, "interact");
}

void modal_notification_abort_assemble(void) {
    if (!s_open || !notif_is_assembling() || s_awaiting_grant || s_awaiting_delivery) return;
    s_on_cancel = NULL;
    assemble_release_freeze();
    release_result_items();
    start_closing();
}

/* ── Update ───────────────────────────────────────────────────────────── */

void modal_notification_update(float dt) {
    /* Tick down the close cooldown even while the modal is closed. */
    if (s_close_cooldown > 0.0f) {
        s_close_cooldown -= dt;
        if (s_close_cooldown < 0.0f) s_close_cooldown = 0.0f;
    }

    if (!s_open) return;

    /* Confirm-press pulse runs on its own clock, independent of the
     * close-slide / delivery-wait branches below — the button vanishes on this
     * timeline regardless of how long the rest of the card stays on screen. */
    if (s_ok_pulsing) {
        s_ok_pulse_age += dt;
        if (s_ok_pulse_age >= MN_OK_PULSE_DURATION) s_ok_pulsing = false;
    }

    /* Close-slide in progress: keep drawing at the sliding position, but skip
     * the open-state logic below until it finishes, at which point the queue
     * actually advances. */
    if (s_closing) {
        s_close_age += dt;
        if (s_close_age >= MN_SLIDE_DURATION) {
            s_closing = false;
            s_close_age = 0.0f;
            show_next();
        }
        return;
    }

    /* Delivery in flight: hold the card at rest (no slide) and keep the
     * inventory holds fresh until the items land on their slots, then play the
     * close-slide. Falls straight through to closing on the very frame no
     * flight actually started (e.g. no inventory slot resolved). */
    if (s_awaiting_delivery) {
        hold_result_items();
        if (!results_inbound()) {
            s_awaiting_delivery = false;
            start_closing();
        }
        return;
    }

    /* Confirmed transaction: hold the card at rest while fx_grant_delivery
     * waits for the server's grant and sequences the spend → arrival FX. It
     * clears either by launching the flights or by giving up on a rejected
     * transaction; loot_fx tells the two apart. */
    if (s_awaiting_grant) {
        if (fx_grant_delivery_waiting()) return;
        s_awaiting_grant = false;
        if (results_inbound()) s_awaiting_delivery = true;
        else                   start_closing();
        return;
    }

    /* Charging assembly: the ingredients were consumed the moment the request
     * went out, so their loss FX is released right away (idempotent — it lands
     * on the first frame the debit registers). The outputs stay held until the
     * bar fills and the grant arrives. */
    if (notif_is_assembling()) {
        Rectangle card = notif_card();
        s_age += dt;
        for (int i = 0; i < s_input_count; i++) {
            fx_inventory_bar_qty_notify_arrival(s_inputs[i].item_id);
        }
        hold_result_items();
        fx_assemble_show(card);
        local_player_keep_freeze();
        s_craft_age += dt;
        if (s_craft_age >= s_craft_total) {
            /* The inputs are spent: spray each out of its own card slot, the
             * reverse of the arrival parabola, while the outputs fly in. */
            for (int i = 0; i < s_input_count; i++) {
                Rectangle slot = notif_input_slot(card, i);
                loot_fx_slot_expend_at(s_inputs[i].item_id,
                                       slot.x + slot.width * 0.5f,
                                       slot.y + slot.height * 0.5f);
            }
            begin_result_delivery(card, NULL, 0);
            assemble_release_freeze();
        }
        return;
    }

    s_age += dt; /* drives the open slide-in; dismissal is button-only */
    /* Celebrate a gift, never a trade: a picker is the player spending their
     * own currency, so it gets the plain card without the reward stars. */
    if (s_item_count > 0) {
        if (!notif_has_picker()) {
            Rectangle card = notif_card();
            if (s_reward_new) { fx_reward_trigger(card); s_reward_new = false; }
            fx_reward_show(card);
        }
        s_reward_pop_age += dt;
    }

    /* Keep the visible + queued holds alive while the player reads — they
     * release when the confirm-press delivery lands on the inventory slot. */
    hold_result_items();
    for (int i = s_queue_head; i != s_queue_tail; i = (i + 1) % MN_QUEUE_CAP) {
        for (int k = 0; k < s_queue[i].item_count; k++) {
            fx_inventory_bar_qty_hold_for_delivery(s_queue[i].items[k].item_id);
        }
    }
}

/* ── Draw ─────────────────────────────────────────────────────────────── */

static void draw_progress_bar(Rectangle bar, float alpha) {
    float t = s_craft_total > 0.0f ? s_craft_age / s_craft_total : 1.0f;
    if (t > 1.0f) t = 1.0f;

    DrawRectangleRec(bar, (Color){ 0, 0, 0, (unsigned char)(220 * alpha) });
    Rectangle inner = { bar.x + 2.0f, bar.y + 2.0f, bar.width - 4.0f, bar.height - 4.0f };
    DrawRectangleRec(inner, (Color){ 18, 26, 40, (unsigned char)(235 * alpha) });

    Rectangle fill = { inner.x, inner.y, inner.width * t, inner.height };
    DrawRectangleRec(fill, (Color){ C_ELECTRIC.r, C_ELECTRIC.g, C_ELECTRIC.b,
                                    (unsigned char)(220 * alpha) });
    /* Leading edge reads as the charge front. */
    if (t > 0.0f && t < 1.0f) {
        DrawRectangle((int)(fill.x + fill.width - 2.0f), (int)inner.y, 2, (int)inner.height,
                      (Color){ 255, 255, 255, (unsigned char)(230 * alpha) });
    }

    /* Black-outlined, so the readout stays legible over both the filled and
     * unfilled halves of the bar as the charge front passes under it. */
    char pct[8];
    snprintf(pct, sizeof(pct), "%d%%", (int)(t * 100.0f));
    int fs = MN_FONT_BODY;
    int x = (int)(bar.x + (bar.width - (float)MeasureText(pct, fs)) * 0.5f);
    int y = (int)(bar.y + (bar.height - (float)text_line_height(fs)) * 0.5f);
    Color outline = { 0, 0, 0, (unsigned char)(240 * alpha) };
    for (int dy = -1; dy <= 1; dy++) {
        for (int dx = -1; dx <= 1; dx++) {
            if (dx || dy) DrawText(pct, x + dx, y + dy, fs, outline);
        }
    }
    DrawText(pct, x, y, fs, (Color){ 235, 245, 255, (unsigned char)(240 * alpha) });
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
    Color border = notif_is_assembling() ? C_ELECTRIC : s_accent;
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

    bool charging = notif_is_assembling() && !s_awaiting_grant && !s_awaiting_delivery && !s_closing;

    /* Consumed inputs — shown for the whole assembly so the trade is legible,
     * and emptied once they have sprayed out of their slots. */
    for (int i = 0; i < s_input_count; i++) {
        Rectangle slot = notif_input_slot(card, i);
        ObjectLayerState ol = { 0 };
        if (charging) {
            strncpy(ol.item_id, s_inputs[i].item_id, sizeof(ol.item_id) - 1);
            ol.quantity = s_inputs[i].qty;
        }
        item_slot_draw_ex(slot, &ol, obj_layers_mgr_get(), C_ELECTRIC, 0.35f, true);
    }
    if (notif_is_assembling()) draw_progress_bar(notif_bar(card), a);

    /* Result slots — centred, popping in oversized (soft overshoot) with their
     * colour transitioning from the accent back to normal as they settle. Once
     * the delivery flights launch the icons leave the slots empty: the loot_fx
     * tokens are what's seen flying to the inventory, so the items must vanish
     * here to sell that motion, and they stay empty through the close-slide. */
    for (int i = 0; i < s_item_count; i++) {
        Rectangle slot = notif_item_slot(card, i);
        if (s_awaiting_delivery || s_closing) {
            ObjectLayerState empty_ol = { 0 };
            item_slot_draw_ex(slot, &empty_ol, obj_layers_mgr_get(), s_accent, 0.0f, true);
            continue;
        }
        /* An assembly keeps its slots at a fixed size for the whole run, so
         * the result reads as materialising in place; a reward pops in. */
        float pop_t = fminf(1.0f, s_reward_pop_age / MN_REWARD_POP_DUR);
        float scale = notif_is_assembling() ? 1.0f : 0.55f + 0.45f * mn_ease_out_back(pop_t);
        float sw = slot.width * scale, sh = slot.height * scale;
        Rectangle pop_slot = { slot.x + (slot.width - sw) * 0.5f,
                               slot.y + (slot.height - sh) * 0.5f, sw, sh };
        /* A charging assembly keeps its slots washed in the synthesis colour
         * until the result is real. */
        Color tint = charging ? C_ELECTRIC : s_accent;
        float tint_t = charging ? 0.55f
                                : 1.0f - fminf(1.0f, s_reward_pop_age / MN_REWARD_TINT_DUR);

        ObjectLayerState ol = { 0 };
        strncpy(ol.item_id, s_items[i].item_id, sizeof(ol.item_id) - 1);
        ol.active = !charging;
        ol.quantity = s_items[i].qty;
        item_slot_draw_ex(pop_slot, &ol, obj_layers_mgr_get(), tint, tint_t, !charging);
    }

    Rectangle cancel_r, confirm_r;
    notif_buttons(card, &cancel_r, &confirm_r);
    int mx = GetMouseX(), my = GetMouseY();

    /* Quantity steppers and running total — drawn only while the entry is
     * still answerable; once Buy is pressed the choice is locked and they
     * leave with the slot. */
    if (notif_has_picker() && !s_awaiting_grant && !s_awaiting_delivery && !s_closing) {
        Rectangle dec = notif_step_dec(card);
        Rectangle inc = notif_step_inc(card);
        bool can_dec = s_items[0].qty > s_qty_min;
        bool can_inc = s_items[0].qty < s_qty_max;

        /* A stepper at the end of its range is disabled outright — muted fill,
         * no hover response, no click. */
        UIButtonPixelRetroStyle step = { .font_size = MN_STEP_FONT, .text_color = C_TITLE };
        step.label = "-";
        step.enabled = can_dec;
        step.bg = (Color){ 50, 55, 80, (unsigned char)((can_dec ? 235.0f : 90.0f) * a) };
        ui_button_pixel_retro_draw(dec, &step, can_dec && ui_button_hit(dec, mx, my));
        step.label = "+";
        step.enabled = can_inc;
        step.bg = (Color){ 50, 55, 80, (unsigned char)((can_inc ? 235.0f : 90.0f) * a) };
        ui_button_pixel_retro_draw(inc, &step, can_inc && ui_button_hit(inc, mx, my));

        /* Running total — the price item's own sprite beside what this purchase
         * costs at the selected count, so "10 coin each" always has its sum. */
        Rectangle row = notif_total_row(card);
        char total[24];
        snprintf(total, sizeof(total), "%d", s_price_qty * s_items[0].qty);
        float total_w = MN_TOTAL_ICON + 6.0f + (float)MeasureText(total, MN_TOTAL_FONT);
        float total_x = row.x + (row.width - total_w) * 0.5f;
        ol_as_ico_draw(obj_layers_mgr_get(), s_price_item, (int)total_x, (int)row.y,
                       MN_TOTAL_ICON, OL_ICO_DEFAULT_DIR, 0, Fade(WHITE, a));
        Color total_c = { 255, 215, 0, (unsigned char)(230.0f * a) };
        DrawText(total, (int)(total_x + MN_TOTAL_ICON + 6.0f),
                 (int)(row.y + (MN_TOTAL_ICON - (float)text_line_height(MN_TOTAL_FONT)) * 0.5f),
                 MN_TOTAL_FONT, total_c);
    }

    /* Confirm button — a picker answers "how many do I buy?", so it reads
     * "Buy"; everything else acknowledges with "OK". Once pressed it plays a
     * quick grow-then-shrink pulse and is gone for good after
     * MN_OK_PULSE_DURATION, independent of the close-slide / delivery wait the
     * rest of the card may still be playing. */
    const char* ok_label = notif_has_picker() ? "Buy" : "OK";
    const char* ok_icon  = notif_has_picker() ? "wallet" : "check";
    if (s_ok_pulsing && confirm_r.width > 0.0f) {
        float pscale, palpha;
        mn_ok_pulse(&pscale, &palpha);
        /* Skip once faint enough that a rounded-to-zero text alpha would
         * fall back to opaque white (see ui_button_pixel_retro_draw) — the
         * button has effectively vanished by this point anyway. */
        if (pscale > 0.02f && palpha > 0.02f) {
            Rectangle pulse_r = {
                confirm_r.x + confirm_r.width  * 0.5f * (1.0f - pscale),
                confirm_r.y + confirm_r.height * 0.5f * (1.0f - pscale),
                confirm_r.width  * pscale,
                confirm_r.height * pscale,
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
        if (confirm_r.width > 0.0f) {
            UIButtonPixelRetroStyle ok_btn = {
                .bg = notif_has_picker() ? (Color){ 38, 138, 76, 235 } : (Color){ 50, 55, 80, 235 },
                .icon_id = ok_icon,
                .label = ok_label,
                .font_size = 15,
                .text_color = C_TITLE,
                .selected = false,
                .enabled = true,
            };
            ui_button_pixel_retro_draw(confirm_r, &ok_btn, ui_button_hit(confirm_r, mx, my));
        }
        /* A purchase is refusable and an assembly is abortable: both pair with
         * a Cancel that stops the operation. */
        if (cancel_r.width > 0.0f) {
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

    /* Synthesis field last, so the convergence reads over the panel instead of
     * being buried by it. Streams dissolve just outside the card edge, so the
     * bar and slots stay legible underneath. */
    fx_assemble_draw();
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
     * (no double-fire on the buttons, nothing behind should react either). */
    if (s_closing || s_awaiting_grant || s_awaiting_delivery) return true;

    Rectangle card = notif_card();
    Rectangle cancel_r, confirm_r;
    notif_buttons(card, &cancel_r, &confirm_r);

    /* Cancel — stops the operation. A running assembly tells the server to
     * refund; a picker simply sends nothing. Either way the held result FX is
     * released with no change, so no popup fires. */
    if (cancel_r.width > 0.0f && ui_button_hit(cancel_r, mx, my)) {
        if (s_on_cancel) { s_on_cancel(); s_on_cancel = NULL; }
        if (notif_is_assembling()) assemble_release_freeze();
        s_on_confirm = NULL;
        release_result_items();
        start_closing();
        return true;
    }

    /* Quantity stepper — adjusts the pending count without dismissing. */
    if (notif_has_picker()) {
        if (ui_button_hit(notif_step_dec(card), mx, my)) { notif_step_quantity(-1); return true; }
        if (ui_button_hit(notif_step_inc(card), mx, my)) { notif_step_quantity(+1); return true; }
    }

    if (confirm_r.width > 0.0f && ui_button_hit(confirm_r, mx, my)) {
        /* Grow-then-shrink punch, then gone for good — runs on its own clock
         * regardless of how long the card itself stays on screen after. */
        s_ok_pulsing = true;
        s_ok_pulse_age = 0.0f;
        /* A picker hands its confirmed count to the opener, then leaves the
         * spend → arrival FX to fx_grant_delivery and holds until it settles. */
        if (s_on_confirm) {
            ModalNotificationConfirmFn confirm = s_on_confirm;
            s_on_confirm = NULL;
            const char* spent = s_price_item;
            begin_result_delivery(card, &spent, 1);
            confirm(s_items[0].item_id, s_items[0].qty);
            return true;
        }
        if (s_item_count > 0) {
            /* Reward arrival — the same presentation as a world pickup,
             * launched from the notification's slots. Landing releases the held
             * slot reveal / +N popup / pulse. The card holds at rest until the
             * flights land, so the player watches the items travel before the
             * notification closes. */
            for (int i = 0; i < s_item_count; i++) {
                Rectangle slot = notif_item_slot(card, i);
                loot_fx_reward_delivery(s_items[i].item_id,
                                        slot.x + slot.width * 0.5f,
                                        slot.y + slot.height * 0.5f);
            }
            s_awaiting_delivery = true;
        } else {
            /* Nothing to deliver — play the center -> right close slide right
             * away; show_next() fires once it finishes. */
            start_closing();
        }
        return true;
    }
    return true; /* swallow all clicks while a notification is up */
}
