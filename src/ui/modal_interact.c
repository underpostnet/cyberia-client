#include "modal_interact.h"

#include "dialogue_data.h"
#include "interaction_bubble.h"
#include "inventory_modal.h"
#include "item_slot.h"
#include "modal.h"
#include "modal_dialogue.h"
#include "notification.h"
#include "object_layer.h"
#include "object_layers_management.h"
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
static bool  s_has_dialogue = false;
static Color s_border = { 80, 160, 220, 240 };

/* The paired modal_dialogue is opened once the skin dialogue resolves. */
static bool  s_dialogue_opened = false;

/* Cached alive layers — snapshot taken when the modal opens so the entity
 * stack stays visible even if the entity leaves the AOI. */
static ObjectLayerState s_cached_layers[IBUBBLE_MAX_LAYERS];
static int              s_cached_layer_count = 0;

/* Flag: the JS overlay is currently open and we're waiting for it to close
 * so we can re-open this modal. */
static bool  s_overlay_open = false;

/* ── Layout ───────────────────────────────────────────────────────────── */

#define MI_PAD        22
#define MI_HEADER_H   48
#define MI_CLOSE_SZ   40
#define MI_BTN_H      52
#define MI_SLOT_SZ    56
#define MI_SLOT_GAP   8
#define MI_FONT_NAME  22
#define MI_FONT_BTN   18
#define MI_FONT_LABEL 14
#define MI_FONT_STAT  14

static const Color C_BTN   = {  24,  30,  48, 255 };
static const Color C_TEXT  = { 220, 220, 230, 240 };
static const Color C_LABEL = { 150, 160, 190, 220 };
static const Color C_STAT  = { 120, 220, 140, 255 };

/* Top half of the screen with generous padding all round. */
static Rectangle card_rect(void) {
    int sw = GetScreenWidth();
    int sh = GetScreenHeight();
    float x = (float)MI_PAD;
    float y = (float)MI_PAD;
    float w = (float)(sw - 2 * MI_PAD);
    float h = sh * 0.5f - MI_PAD * 1.5f;
    return (Rectangle){ x, y, w, h };
}

static Rectangle close_rect(Rectangle card) {
    return (Rectangle){ card.x + card.width - MI_CLOSE_SZ - 10.0f, card.y + 4.0f,
                        (float)MI_CLOSE_SZ, (float)MI_CLOSE_SZ };
}

/* Two equal action buttons in a row below the header. */
static void button_rects(Rectangle card, Rectangle* chat, Rectangle* integration) {
    float bx = card.x + MI_PAD;
    float by = card.y + MI_HEADER_H + MI_PAD;
    float bw = (card.width - 3 * MI_PAD) * 0.5f;
    *chat        = (Rectangle){ bx, by, bw, MI_BTN_H };
    *integration = (Rectangle){ bx + bw + MI_PAD, by, bw, MI_BTN_H };
}

/* The active-item slot row sits below the buttons (after a section label). */
static float items_row_y(Rectangle card) {
    return card.y + MI_HEADER_H + MI_PAD + MI_BTN_H + MI_PAD + MI_FONT_LABEL + 6.0f;
}

static Rectangle item_slot_rect(Rectangle card, int i) {
    return (Rectangle){ card.x + MI_PAD + (float)i * (MI_SLOT_SZ + MI_SLOT_GAP),
                        items_row_y(card), (float)MI_SLOT_SZ, (float)MI_SLOT_SZ };
}

/* Baseline y of the stat-name row (the "Stack stats" label sits just above). */
static float stats_y(Rectangle card) {
    return items_row_y(card) + MI_SLOT_SZ + MI_PAD + MI_FONT_LABEL + 6.0f;
}

/* Sum of each of the six stats across the entity's active stack. */
static Stats stack_stats(void) {
    Stats t = { 0 };
    for (int i = 0; i < s_cached_layer_count; i++) {
        ObjectLayer* ol = lookup_cached_layer(s_cached_layers[i].item_id);
        if (!ol) continue;
        t.effect       += ol->data.stats.effect;
        t.resistance   += ol->data.stats.resistance;
        t.agility      += ol->data.stats.agility;
        t.range        += ol->data.stats.range;
        t.intelligence += ol->data.stats.intelligence;
        t.utility      += ol->data.stats.utility;
    }
    return t;
}

/* ── Snapshot alive layers ───────────────────────────────────────────── */

static void snapshot_alive_layers(void) {
    s_cached_layer_count = 0;
    int lc = 0;
    const ObjectLayerState* layers = interaction_bubble_get_alive_layers(s_entity_id, &lc);
    if (layers && lc > 0) {
        int n = lc < IBUBBLE_MAX_LAYERS ? lc : IBUBBLE_MAX_LAYERS;
        memcpy(s_cached_layers, layers, sizeof(ObjectLayerState) * n);
        s_cached_layer_count = n;
    }
}

/* ── Paired dialogue ──────────────────────────────────────────────────── */

/* The client fetches dialogue at /code/default-<itemId>, so the dialogue
 * group reported on dlg_complete is "default-<skin>". */
static void open_paired_dialogue(const DialogueLine* lines, int count) {
    char code[96];
    snprintf(code, sizeof(code), "default-%s", s_dlg_item);
    modal_dialogue_open(s_entity_id, s_dlg_item, code,
                        MODAL_DIALOGUE_RENDER_ENTITY, lines, count);
    s_dialogue_opened = true;
}

/* ── JS overlay hand-off ──────────────────────────────────────────────── */

static void open_overlay(int tab) {
    s_overlay_open = true;
    char entity[64];
    strncpy(entity, s_entity_id, sizeof(entity) - 1);
    entity[sizeof(entity) - 1] = '\0';
    interaction_bubble_open_js_overlay(entity, tab);
}

/* Reopen the interaction modal with the cached entity context — used to
 * return here after a child modal (JS overlay, item detail) closes. */
static void modal_interact_reopen(void) {
    modal_interact_open(s_entity_id, s_display_name, s_dlg_item,
                        s_has_dialogue, s_border);
}

/* Called when the JS overlay closes — reopen the modal. */
void modal_interact_overlay_closed(void) {
    if (!s_overlay_open) return;
    s_overlay_open = false;
    modal_interact_reopen();
}

/* ── Public API ───────────────────────────────────────────────────────── */

void modal_interact_init(void) {
    s_open = false;
    s_overlay_open = false;
}

void modal_interact_open(const char* entity_id, const char* display_name,
                         const char* dialogue_item_id, bool has_dialogue,
                         Color border) {
    strncpy(s_entity_id, entity_id ? entity_id : "", sizeof(s_entity_id) - 1);
    s_entity_id[sizeof(s_entity_id) - 1] = '\0';
    strncpy(s_display_name, display_name ? display_name : "", sizeof(s_display_name) - 1);
    s_display_name[sizeof(s_display_name) - 1] = '\0';
    strncpy(s_dlg_item, dialogue_item_id ? dialogue_item_id : "", sizeof(s_dlg_item) - 1);
    s_dlg_item[sizeof(s_dlg_item) - 1] = '\0';

    s_has_dialogue    = has_dialogue && s_dlg_item[0] != '\0';
    s_border          = border;
    s_age             = 0.0f;
    s_dialogue_opened = false;
    s_overlay_open    = false;
    s_open            = true;

    /* Snapshot alive layers so the entity stack persists even if the
     * entity leaves the AOI while the modal is open. */
    snapshot_alive_layers();

    /* Both modals appear: the dialogue opens with the skin's text when one
     * exists (resolved async in update), otherwise render-only right away. */
    if (s_has_dialogue) dialogue_data_request(s_dlg_item);
    else                open_paired_dialogue(NULL, 0);

    LOG_INFO("[MODAL_INTERACT] Open: entity=%s dialogue=%d\n",
             s_entity_id, s_has_dialogue ? 1 : 0);
}

void modal_interact_close(void) {
    s_open = false;
    s_overlay_open = false;
    if (modal_dialogue_is_open()) modal_dialogue_close();
}

bool modal_interact_is_open(void) { return s_open; }

void modal_interact_update(float dt) {
    if (!s_open) return;
    s_age += dt;

    if (!s_dialogue_opened) {
        const DialogueDataSet* d = dialogue_data_get(s_dlg_item);
        if (d && d->state == DLG_DATA_READY && d->line_count > 0) {
            open_paired_dialogue(d->lines, d->line_count);
        } else if (d && (d->state == DLG_DATA_EMPTY || d->state == DLG_DATA_ERROR)) {
            open_paired_dialogue(NULL, 0);
        }
    }
}

/* Draw the six-stat stack totals as a labelled row. */
static void draw_stat_totals(Rectangle card) {
    Stats t = stack_stats();
    const char* names[6] = { "EFF", "RES", "AGI", "RNG", "INT", "UTL" };
    int values[6] = { t.effect, t.resistance, t.agility, t.range, t.intelligence, t.utility };

    float y = stats_y(card);
    DrawText("Stack stats", (int)(card.x + MI_PAD), (int)(y - MI_FONT_LABEL - 4),
             MI_FONT_LABEL, C_LABEL);

    float cell = (card.width - 2 * MI_PAD) / 6.0f;
    for (int i = 0; i < 6; i++) {
        float cx = card.x + MI_PAD + cell * i;
        DrawText(names[i], (int)cx, (int)y, MI_FONT_STAT, C_LABEL);
        char val[16];
        snprintf(val, sizeof(val), "%d", values[i]);
        DrawText(val, (int)cx, (int)(y + MI_FONT_STAT + 2), MI_FONT_STAT, C_STAT);
    }
}

void modal_interact_draw(void) {
    if (!s_open) return;
    int sw = GetScreenWidth();
    int sh = GetScreenHeight();
    int mx = GetMouseX(), my = GetMouseY();

    /* Dim backdrop + a more transparent panel than the shared default, so the
     * world stays visible behind the upper-half modal. */
    modal_draw_overlay(sw, sh, s_age);
    Rectangle card = modal_scale_rect(card_rect(), modal_pop_scale(s_age));
    float a = modal_pop_alpha(s_age);
    Color bg = MODAL_PANEL_BG;
    bg.a = (unsigned char)(150 * a);
    DrawRectangleRec(card, bg);
    Color bc = s_border;
    bc.a = (unsigned char)(bc.a * a);
    DrawRectangleLinesEx(card, 1.0f, bc);

    /* Header: status-tinted strip + display name + close button. */
    DrawRectangle((int)card.x, (int)card.y, (int)card.width, MI_HEADER_H,
                  (Color){ s_border.r, s_border.g, s_border.b, 40 });
    if (s_display_name[0] != '\0') {
        DrawText(s_display_name, (int)(card.x + MI_PAD),
                 (int)(card.y + (MI_HEADER_H - MI_FONT_NAME) / 2), MI_FONT_NAME, C_TEXT);
    }

    Rectangle xr = close_rect(card);
    UIButtonStyle close_btn = { .icon_id = "close-yellow", .no_fill = true };
    ui_button_draw(xr, &close_btn,
                   ui_button_resolve_state(true, false, ui_button_hit(xr, mx, my)));

    Rectangle chat, integration;
    button_rects(card, &chat, &integration);
    UIButtonStyle chat_btn = { .text = "Chat", .icon_id = "chat",
                               .font_size = MI_FONT_BTN, .bg = C_BTN, .text_color = C_TEXT };
    ui_button_draw(chat, &chat_btn,
                   ui_button_resolve_state(true, false, ui_button_hit(chat, mx, my)));

    /* Unread-chat badge on the Chat button — accumulates until clicked. */
    int unread = notification_count(NOTIF_CHAT, s_entity_id);
    if (unread > 0) {
        float br = 11.0f;
        float bx = chat.x + chat.width - br - 4.0f;
        float by = chat.y + br + 4.0f;
        DrawCircle((int)bx, (int)by, br, (Color){ 210, 60, 60, 240 });
        char txt[8];
        snprintf(txt, sizeof(txt), "%d", unread > 99 ? 99 : unread);
        int bfs = 11;
        int tw = MeasureText(txt, bfs);
        DrawText(txt, (int)(bx - tw * 0.5f), (int)(by - bfs * 0.5f), bfs,
                 (Color){ 255, 255, 255, 245 });
    }
    UIButtonStyle integration_btn = { .text = "Integration", .icon_id = "reload",
                                      .font_size = MI_FONT_BTN, .bg = C_BTN, .text_color = C_TEXT };
    ui_button_draw(integration, &integration_btn,
                   ui_button_resolve_state(true, false, ui_button_hit(integration, mx, my)));

    /* Active items — reuse the shared inventory slot primitive. */
    DrawText("Active items", (int)(card.x + MI_PAD),
             (int)(items_row_y(card) - MI_FONT_LABEL - 6), MI_FONT_LABEL, C_LABEL);
    ObjectLayersManager* olm = obj_layers_mgr_get();
    for (int i = 0; i < s_cached_layer_count; i++) {
        item_slot_draw(item_slot_rect(card, i), &s_cached_layers[i], olm);
    }

    draw_stat_totals(card);
}

bool modal_interact_handle_click(int mx, int my) {
    if (!s_open) return false;
    if (s_age < MODAL_POP_DURATION) return true; /* swallow taps during pop-in */

    Rectangle card = card_rect();

    if (ui_button_hit(close_rect(card), mx, my)) {
        modal_interact_close();
        return true;
    }

    Rectangle chat, integration;
    button_rects(card, &chat, &integration);
    if (ui_button_hit(chat, mx, my)) {
        notification_clear(NOTIF_CHAT, s_entity_id); /* reset badge on view */
        open_overlay(INTERACT_OVERLAY_TAB_CHAT);
        return true;
    }
    if (ui_button_hit(integration, mx, my)) {
        open_overlay(INTERACT_OVERLAY_TAB_INTEGRATION);
        return true;
    }

    /* Active-item slot → inspect that item (read-only, no activate); closing
     * it returns here. Inspection context regardless of ownership. */
    for (int i = 0; i < s_cached_layer_count; i++) {
        if (item_slot_hit(item_slot_rect(card, i), mx, my)) {
            ObjectLayerState ols = s_cached_layers[i];
            modal_interact_close();
            inventory_modal_open_external(&ols);
            inventory_modal_set_on_close(modal_interact_reopen);
            return true;
        }
    }

    if (!ui_button_hit(card, mx, my)) {
        modal_interact_close();
        return true;
    }

    return true; /* swallow clicks inside the card */
}

/* ── Exported for draw_dialogue_sprite in modal_dialogue.c ───────────── */

const ObjectLayerState* modal_interact_get_cached_layers(int* out_count) {
    if (out_count) *out_count = s_cached_layer_count;
    return s_cached_layer_count > 0 ? s_cached_layers : NULL;
}