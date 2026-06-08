#include "modal_interact.h"

#include "dialogue_data.h"
#include "interaction_bubble.h"
#include "modal.h"
#include "modal_dialogue.h"
#include "ui_button.h"
#include "util/log.h"

#include <raylib.h>
#include <stdio.h>
#include <string.h>

/* ── Module state ─────────────────────────────────────────────────────── */

static bool  s_open = false;
static float s_age  = 0.0f;

static char  s_entity_id[64]    = {0};
static char  s_display_name[64] = {0};
static char  s_dlg_item[128]    = {0};
static bool  s_has_talk = false;
static bool  s_is_player = false;
static bool  s_is_self   = false;
static Color s_border    = { 80, 160, 220, 240 };

/* Talk tab waits for the async dialogue fetch before opening modal_dialogue. */
static bool  s_talk_pending = false;

/* ── Layout ───────────────────────────────────────────────────────────── */

#define MI_CARD_W_FRAC   0.62f
#define MI_CARD_H        180
#define MI_CARD_W_MAX    420
#define MI_HEADER_H      40
#define MI_TAB_H         44
#define MI_PAD           14
#define MI_FONT_NAME     20
#define MI_FONT_TAB      18
#define MI_FONT_HINT     14

static const Color C_TAB     = {  24,  30,  48, 255 };
static const Color C_TAB_DIS = {  18,  18,  26, 200 };
static const Color C_TEXT    = { 220, 220, 230, 240 };
static const Color C_HINT    = { 140, 140, 160, 190 };
static const Color C_X       = { 210, 120, 120, 240 };

static Rectangle card_rect(void) {
    int sw = GetScreenWidth();
    int sh = GetScreenHeight();
    int w  = (int)(sw * MI_CARD_W_FRAC);
    if (w > MI_CARD_W_MAX) w = MI_CARD_W_MAX;
    int h  = MI_CARD_H;
    return (Rectangle){ (float)((sw - w) / 2), (float)((sh - h) / 2), (float)w, (float)h };
}

static Rectangle close_rect(Rectangle card) {
    float s = 24.0f;
    return (Rectangle){ card.x + card.width - s - 8.0f, card.y + 8.0f, s, s };
}

static Rectangle talk_tab_rect(Rectangle card) {
    return (Rectangle){ card.x + MI_PAD, card.y + MI_HEADER_H,
                        card.width * 0.5f - MI_PAD * 1.5f, MI_TAB_H };
}

static Rectangle chat_tab_rect(Rectangle card) {
    float half = card.width * 0.5f;
    return (Rectangle){ card.x + half + MI_PAD * 0.5f, card.y + MI_HEADER_H,
                        half - MI_PAD * 1.5f, MI_TAB_H };
}

static bool hit_rect(int mx, int my, Rectangle r) {
    return ((float)mx >= r.x && (float)mx < r.x + r.width &&
            (float)my >= r.y && (float)my < r.y + r.height);
}

/* ── Talk → modal_dialogue ────────────────────────────────────────────── */

/* The client fetches dialogue per item id at /code/default-<itemId>, so the
 * dialogue group the player reads is "default-<itemId>". That is the code
 * reported to the server on dlg_complete; an action's questDialogueCodes must
 * contain it to advance a talk objective. */
static void build_dialog_code(char* out, size_t cap, const char* item_id) {
    snprintf(out, cap, "default-%s", item_id ? item_id : "");
}

static bool try_open_dialogue(void) {
    const DialogueDataSet* d = dialogue_data_get(s_dlg_item);
    if (!d || d->state != DLG_DATA_READY || d->line_count <= 0) return false;

    char code[96];
    build_dialog_code(code, sizeof(code), s_dlg_item);

    char entity[64];
    strncpy(entity, s_entity_id, sizeof(entity) - 1);
    entity[sizeof(entity) - 1] = '\0';
    char item[128];
    strncpy(item, s_dlg_item, sizeof(item) - 1);
    item[sizeof(item) - 1] = '\0';

    modal_interact_close();
    modal_dialogue_open(entity, item, code, MODAL_DIALOGUE_RENDER_ENTITY,
                        d->lines, d->line_count);
    return true;
}

/* ── Public API ───────────────────────────────────────────────────────── */

void modal_interact_init(void) {
    s_open = false;
    s_talk_pending = false;
}

void modal_interact_open(const char* entity_id, const char* display_name,
                         const char* dialogue_item_id, bool has_talk,
                         bool is_player, bool is_self, Color border) {
    strncpy(s_entity_id, entity_id ? entity_id : "", sizeof(s_entity_id) - 1);
    s_entity_id[sizeof(s_entity_id) - 1] = '\0';
    strncpy(s_display_name, display_name ? display_name : "", sizeof(s_display_name) - 1);
    s_display_name[sizeof(s_display_name) - 1] = '\0';
    strncpy(s_dlg_item, dialogue_item_id ? dialogue_item_id : "", sizeof(s_dlg_item) - 1);
    s_dlg_item[sizeof(s_dlg_item) - 1] = '\0';

    s_has_talk     = has_talk && s_dlg_item[0] != '\0';
    s_is_player    = is_player;
    s_is_self      = is_self;
    s_border       = border;
    s_age          = 0.0f;
    s_talk_pending = false;
    s_open         = true;

    if (s_has_talk) dialogue_data_request(s_dlg_item);
    LOG_INFO("[MODAL_INTERACT] Open: entity=%s talk=%d\n", s_entity_id, s_has_talk ? 1 : 0);
}

void modal_interact_close(void) {
    s_open = false;
    s_talk_pending = false;
}

bool modal_interact_is_open(void) { return s_open; }

void modal_interact_update(float dt) {
    if (!s_open) return;
    s_age += dt;
    if (s_talk_pending) {
        const DialogueDataSet* d = dialogue_data_get(s_dlg_item);
        if (d && (d->state == DLG_DATA_READY || d->state == DLG_DATA_EMPTY ||
                  d->state == DLG_DATA_ERROR)) {
            if (!try_open_dialogue()) {
                s_talk_pending = false; /* no dialogue to show; keep modal open */
            }
        }
    }
}

void modal_interact_draw(void) {
    if (!s_open) return;
    int sw = GetScreenWidth();
    int sh = GetScreenHeight();

    /* Shared dim backdrop + standardized panel chrome with pop-in. All
     * sub-rects derive from the scaled card so geometry stays consistent. */
    modal_draw_overlay(sw, sh, s_age);
    Rectangle card = modal_scale_rect(card_rect(), modal_pop_scale(s_age));
    modal_draw_panel_ex(card, s_age, s_border, 1.0f);

    /* Header: status-tinted strip + display name + close button. */
    DrawRectangle((int)card.x, (int)card.y, (int)card.width, MI_HEADER_H,
                  (Color){ s_border.r, s_border.g, s_border.b, 40 });
    if (s_display_name[0] != '\0') {
        DrawText(s_display_name, (int)(card.x + MI_PAD),
                 (int)(card.y + (MI_HEADER_H - MI_FONT_NAME) / 2), MI_FONT_NAME, C_TEXT);
    }
    int mx = GetMouseX(), my = GetMouseY();

    Rectangle xr = close_rect(card);
    UIButtonStyle close_btn = { .text = "X", .font_size = 18,
                                .text_color = C_X, .no_fill = true };
    ui_button_draw(xr, &close_btn,
                   ui_button_resolve_state(true, false, ui_button_hit(xr, mx, my)));

    /* Tabs */
    Rectangle tr = talk_tab_rect(card);
    UIButtonStyle talk_btn = { .text = s_has_talk ? "Talk" : NULL,
                               .font_size = MI_FONT_TAB,
                               .bg = C_TAB, .bg_disabled = C_TAB_DIS,
                               .text_color = C_TEXT };
    ui_button_draw(tr, &talk_btn,
                   s_has_talk ? ui_button_resolve_state(true, false, ui_button_hit(tr, mx, my))
                              : UI_BUTTON_DISABLED);

    Rectangle cr = chat_tab_rect(card);
    UIButtonStyle chat_btn = { .text = s_is_self ? "Profile" : "Chat / Profile",
                               .font_size = MI_FONT_TAB,
                               .bg = C_TAB, .text_color = C_TEXT };
    ui_button_draw(cr, &chat_btn,
                   ui_button_resolve_state(true, false, ui_button_hit(cr, mx, my)));

    /* Content hint */
    const char* hint = s_talk_pending ? "Loading dialogue..."
                                      : "Choose an interaction";
    DrawText(hint, (int)(card.x + MI_PAD),
             (int)(card.y + MI_HEADER_H + MI_TAB_H + MI_PAD), MI_FONT_HINT, C_HINT);
}

bool modal_interact_handle_click(int mx, int my) {
    if (!s_open) return false;
    if (s_age < MODAL_POP_DURATION) return true; /* swallow taps during pop-in */

    Rectangle card = card_rect();

    if (hit_rect(mx, my, close_rect(card))) {
        modal_interact_close();
        return true;
    }

    if (s_has_talk && hit_rect(mx, my, talk_tab_rect(card))) {
        /* Open dialogue now if ready, else mark pending and let update poll. */
        if (!try_open_dialogue()) {
            s_talk_pending = true;
            dialogue_data_request(s_dlg_item);
        }
        return true;
    }

    if (hit_rect(mx, my, chat_tab_rect(card))) {
        char entity[64];
        strncpy(entity, s_entity_id, sizeof(entity) - 1);
        entity[sizeof(entity) - 1] = '\0';
        modal_interact_close();
        interaction_bubble_open_js_overlay(entity);
        return true;
    }

    if (!hit_rect(mx, my, card)) {
        /* Tap outside → dismiss, no side effects. */
        modal_interact_close();
        return true;
    }

    return true; /* swallow clicks inside the card */
}
