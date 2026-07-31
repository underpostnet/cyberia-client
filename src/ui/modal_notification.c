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
#include "item_slot.h"
#include "loot_fx.h"
#include "modal.h"
#include "modal_interact.h"
#include "inventory_modal.h"
#include "object_layer.h"
#include "object_layers_management.h"
#include "fx_reward.h"
#include "ui_button.h"

#include <raylib.h>
#include <math.h>
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

/* ── Queued notification entry ────────────────────────────────────────── */
typedef struct {
    char  title[96];
    char  message[160];
    char  reward_item[64];
    int   reward_qty;
    Color accent;
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
                       const char* reward_item_id, int reward_quantity) {
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
    s_ok_pulsing = false;
    s_ok_pulse_age = 0.0f;
}

void modal_notification_show(const char* title, const char* message, Color accent) {
    push_queue(title, message, accent, NULL, 0);
}

void modal_notification_show_reward(const char* title, const char* message, Color accent,
                                    const char* reward_item_id, int reward_quantity) {
    push_queue(title, message, accent, reward_item_id, reward_quantity);
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

/* Card height derived from the wrapped content so text never overflows and the
 * card grows with longer messages (and the active font size / family). */
static float notif_content_height(void) {
    int iw = notif_inner_w();
    float h = MN_TOP;
    h += (float)text_wrap(s_title, 0, 0, iw, MN_FONT_TITLE, C_TITLE, true, false);
    if (s_message[0] != '\0') {
        h += MN_GAP + (float)text_wrap(s_message, 0, 0, iw, MN_FONT_BODY, C_BODY, true, false);
    }
    if (s_reward_item[0] != '\0') {
        h += MN_GAP + MN_SLOT;
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

static Rectangle notif_ok(Rectangle card) {
    return (Rectangle){ card.x + (card.width - MN_OK_W) / 2.0f,
                        card.y + card.height - MN_OK_H - 12, MN_OK_W, MN_OK_H };
}

/* Resting rect of the reward slot (below title + message) — shared by the
 * draw pass and the OK-press delivery launch. */
static Rectangle notif_reward_slot(Rectangle card) {
    int iw = notif_inner_w();
    float cy = card.y + MN_TOP;
    cy += (float)text_wrap(s_title, 0, 0, iw, MN_FONT_TITLE, C_TITLE, true, false);
    if (s_message[0] != '\0') {
        cy += MN_GAP + (float)text_wrap(s_message, 0, 0, iw, MN_FONT_BODY, C_BODY, true, false);
    }
    cy += MN_GAP;
    return (Rectangle){ card.x + (card.width - MN_SLOT) / 2.0f, cy, MN_SLOT, MN_SLOT };
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

    s_age += dt; /* drives the open slide-in; dismissal is OK-only */
    /* Celebrate only when the notice carries an object layer (a reward item). */
    if (s_reward_item[0] != '\0') {
        Rectangle card = notif_card();
        if (s_reward_new) { fx_reward_trigger(card); s_reward_new = false; }
        fx_reward_show(card);
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

    /* OK button — bottom centre, pixel-retro style with check icon. Once
     * pressed it plays a quick grow-then-shrink pulse and is gone for good
     * after MN_OK_PULSE_DURATION, independent of the close-slide / delivery
     * wait the rest of the card may still be playing. */
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
                .icon_id = "check",
                .label = "OK",
                .font_size = 15,
                .text_color = (Color){ C_TITLE.r, C_TITLE.g, C_TITLE.b,
                                       (unsigned char)((float)C_TITLE.a * palpha) },
                .selected = false,
                .enabled = false,
            };
            ui_button_pixel_retro_draw(pulse_r, &pulse_btn, false);
        }
    } else if (!s_closing && !s_awaiting_delivery) {
        int mx = GetMouseX(), my = GetMouseY();
        bool hit = ((float)mx >= ok_r.x && (float)mx < ok_r.x + ok_r.width &&
                    (float)my >= ok_r.y && (float)my < ok_r.y + ok_r.height);
        UIButtonPixelRetroStyle ok_btn = {
            .bg = (Color){ 50, 55, 80, 235 },
            .icon_id = "check",
            .label = "OK",
            .font_size = 15,
            .text_color = C_TITLE,
            .selected = false,
            .enabled = true,
        };
        ui_button_pixel_retro_draw(ok_r, &ok_btn, hit);
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
    if (s_closing || s_awaiting_delivery) return true;
    Rectangle card = notif_card();
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