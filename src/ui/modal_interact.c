#include "modal_interact.h"

#include "dialogue_data.h"
#include "interaction_bubble.h"
#include "modal.h"
#include "modal_dialogue.h"
#include "ui_button.h"
#include "object_layers_management.h"
#include "ol_stack_ico.h"
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

#define MI_PAD        14
#define MI_HEADER_H   44
#define MI_FONT_NAME  20
#define MI_FONT_BTN   18

static const Color C_BTN  = {  24,  30,  48, 255 };
static const Color C_TEXT = { 220, 220, 230, 240 };
static const Color C_X    = { 210, 120, 120, 240 };

/* Top half of the screen with padding all round. */
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
    float s = 28.0f;
    return (Rectangle){ card.x + card.width - s - 8.0f, card.y + 8.0f, s, s };
}

/* Two equal action buttons filling the body below the header. */
static void button_rects(Rectangle card, Rectangle* chat, Rectangle* integration) {
    float bx = card.x + MI_PAD;
    float by = card.y + MI_HEADER_H + MI_PAD;
    float bw = (card.width - 3 * MI_PAD) * 0.5f;
    float bh = card.height - MI_HEADER_H - 2 * MI_PAD;
    *chat        = (Rectangle){ bx, by, bw, bh };
    *integration = (Rectangle){ bx + bw + MI_PAD, by, bw, bh };
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

/* Called when the JS overlay closes — reopen the modal. */
void modal_interact_overlay_closed(void) {
    if (!s_overlay_open) return;
    s_overlay_open = false;
    /* Reopen the interaction modal with the cached data. */
    modal_interact_open(s_entity_id, s_display_name, s_dlg_item,
                        s_has_dialogue, s_border);
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

void modal_interact_draw(void) {
    if (!s_open) return;
    int sw = GetScreenWidth();
    int sh = GetScreenHeight();

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

    Rectangle chat, integration;
    button_rects(card, &chat, &integration);

    UIButtonStyle chat_btn = { .text = "Chat", .icon_id = "chat",
                               .font_size = MI_FONT_BTN, .bg = C_BTN, .text_color = C_TEXT };
    ui_button_draw(chat, &chat_btn,
                   ui_button_resolve_state(true, false, ui_button_hit(chat, mx, my)));

    UIButtonStyle integration_btn = { .text = "Integration", .icon_id = "reload",
                                      .font_size = MI_FONT_BTN, .bg = C_BTN, .text_color = C_TEXT };
    ui_button_draw(integration, &integration_btn,
                   ui_button_resolve_state(true, false, ui_button_hit(integration, mx, my)));
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
        open_overlay(INTERACT_OVERLAY_TAB_CHAT);
        return true;
    }
    if (ui_button_hit(integration, mx, my)) {
        open_overlay(INTERACT_OVERLAY_TAB_INTEGRATION);
        return true;
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