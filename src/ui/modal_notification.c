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
 */

#include "modal_notification.h"
#include "text.h"

#include "item_slot.h"
#include "modal.h"
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

static const Color C_TITLE = { 230, 235, 245, 255 };
static const Color C_BODY  = { 170, 180, 200, 230 };

static void show_next(void) {
    if (s_queue_head == s_queue_tail) {
        s_open = false;
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

    /* Start displaying if nothing is visible right now. */
    if (!s_open) show_next();
}

void modal_notification_init(void) {
    s_open = false;
    s_queue_head = 0;
    s_queue_tail = 0;
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

/* Screen-centred card. The notification stays until the player taps OK, so the
 * geometry is static (no slide/fade) and shared by draw + hit-test; its height is
 * content-driven (see notif_content_height). */
static Rectangle notif_card(void) {
    int sw = GetScreenWidth(), sh = GetScreenHeight();
    float h = notif_content_height();
    return (Rectangle){ (sw - MN_W) / 2.0f, (sh - h) / 2.0f, MN_W, h };
}

static Rectangle notif_ok(Rectangle card) {
    return (Rectangle){ card.x + (card.width - MN_OK_W) / 2.0f,
                        card.y + card.height - MN_OK_H - 12, MN_OK_W, MN_OK_H };
}

void modal_notification_update(float dt) {
    if (!s_open) return;
    s_age += dt; /* drives the pop-in only; dismissal is OK-only */
    /* Celebrate only when the notice carries an object layer (a reward item). */
    if (s_reward_item[0] != '\0') {
        Rectangle card = notif_card();
        if (s_reward_new) { fx_reward_trigger(card); s_reward_new = false; }
        fx_reward_show(card);
        s_reward_pop_age += dt;
    }
}

void modal_notification_draw(void) {
    if (!s_open) return;

    float a = modal_pop_alpha(s_age);
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
     * settles, so a fresh reward reads as a distinct, celebratory arrival. */
    if (s_reward_item[0] != '\0') {
        cy += MN_GAP;
        Rectangle slot = { card.x + (card.width - MN_SLOT) / 2.0f, cy, MN_SLOT, MN_SLOT };

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
        item_slot_draw_ex(pop_slot, &ol, obj_layers_mgr_get(), s_accent, tint_t);
    }

    /* OK button — bottom centre. */
    int mx = GetMouseX(), my = GetMouseY();
    Rectangle ok_r = notif_ok(card);
    bool hit = ((float)mx >= ok_r.x && (float)mx < ok_r.x + ok_r.width &&
                (float)my >= ok_r.y && (float)my < ok_r.y + ok_r.height);
    UIButtonStyle ok_btn = { .text = "OK", .font_size = 15,
                             .bg = { 50, 55, 80, 235 },
                             .text_color = C_TITLE, .rounded = true, .roundness = 0.25f };
    ui_button_draw(ok_r, &ok_btn, ui_button_resolve_state(true, false, hit));
}

bool modal_notification_handle_click(int mx, int my) {
    if (!s_open) return false;
    Rectangle ok_r = notif_ok(notif_card());
    if ((float)mx >= ok_r.x && (float)mx < ok_r.x + ok_r.width &&
        (float)my >= ok_r.y && (float)my < ok_r.y + ok_r.height) {
        show_next();
        return true;
    }
    return true; /* swallow clicks while a notification is up (modal) */
}
