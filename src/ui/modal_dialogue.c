/**
 * @file modal_dialogue.c
 * @brief Dialogue/lore modal implementation — uses modal.c for rendering.
 *
 * Architecture:
 *   - Wraps a Modal struct (from modal.c) for the text box rendering.
 *   - Manages its own state for line progression and typewriter effect.
 *   - Sends "dialogue_start" / "dialogue_end" JSON messages to the Go
 *     server so it can grant / revoke damage immunity.
 *   - Left column renders the entity's full alive OL stack via ol_stack_ico.
 */

#include "modal_dialogue.h"
#include "text.h"

#include "domain/local_player.h"
#include "domain/viewport.h"
#include "game_state.h"
#include "interaction_bubble.h"
#include "inventory_bar.h"
#include "modal_interact.h"
#include "modal.h"
#include "toolbar.h"
#include "object_layer.h"
#include "object_layers_management.h"
#include "ol_as_animated_ico.h"
#include "ol_stack_ico.h"
#include "ui_button.h"
#include "ui_button_pixel_retro.h"
#include "ui_icon.h"
#include "util/log.h"

#include <assert.h>
#include <math.h>
#include <raylib.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ── Module state ─────────────────────────────────────────────────────── */

static bool  s_open       = false;
static float s_age        = 0.0f;

/* Dialogue content */
static DialogueLine s_lines[DIALOGUE_MAX_LINES];
static int          s_line_count  = 0;
static int          s_current     = 0;   /* index of the line being displayed */

/* Entity / item context */
static char s_entity_id[64]   = {0};
static char s_item_id[128]    = {0};
static char s_dialog_code[96] = {0};
static ModalDialogueRender s_render = MODAL_DIALOGUE_RENDER_ITEM;
/* ITEM (inventory lore) owns its own dismissal; ENTITY is paired with
 * modal_interact, which owns dismissal. dlg_* server frames are sent only
 * when the dialogue actually has lines. */
static bool s_auto_dismiss = true;
static bool s_dlg_started  = false;
static bool s_quest_style  = false;
static bool s_ended        = false;  /* all lines read at least once (entity) */

/* On mobile, paired entity dialogue stays hidden until the interact footer
 * opens the fullscreen reader. ITEM (inventory lore) opens fullscreen
 * directly. */
static bool      s_fullscreen = false;
static Rectangle s_fs_close_rect;  /* fullscreen close button     */

/* Typewriter state */
static float s_char_timer    = 0.0f;
static int   s_chars_visible = 0;
static bool  s_line_complete = false;

/* One-shot close callback (cleared after firing) */
static ModalDialogueOnClose s_on_close = NULL;

/* The underlying modal used purely for the text-box chrome.
 * We call modal_draw_struct() for the background/border then overlay
 * our custom content on top.  */
static Modal s_modal;

/* ── Layout constants ───────────────────────────────────────────────────── */

/* The dialogue panel fills the entire bottom half of the screen (minus the
 * inventory bar) with generous padding and large readable fonts.
 * No fixed pixel sizes — everything is derived from screen dimensions
 * so it scales naturally across resolutions.                               */

#define DLG_SPRITE_FRAC        0.50f    /* desktop: sprite column = half card   */
#define DLG_SPRITE_FRAC_MOBILE 0.3333f  /* mobile: 1/3 sprite, 2/3 dialogue     */
#define DLG_FONT_SPEAKER    22
#define DLG_FONT_TEXT       18
#define DLG_FONT_HINT       14
#define DLG_PAD_FRAC        0.03f   /* padding as fraction of panel width      */
#define DLG_PAD_MIN         12
#define DLG_PAD_MAX         32
#define DLG_TYPEWRITER_SPD  0.022f  /* seconds per character                   */

/* Vertical split: panel occupies [50 % … bottom − bar_h] of the screen.   */
#define DLG_TOP_FRAC        0.56f
#define DLG_SIDE_PAD        18
#define DLG_SIDE_PAD_MOBILE  8
#define DLG_PANEL_GAP        6.0f

/* ── Colours ──────────────────────────────────────────────────────────── */

static const Color C_SPEAKER   = { 200, 220, 255, 255 };
static const Color C_TEXT      = { 220, 220, 230, 240 };
static const Color C_HINT      = { 140, 140, 160, 180 };
static const Color C_CARD_BORD = {  70,  70, 120, 200 };
/* ── Helpers ──────────────────────────────────────────────────────────── */

/* Mobile fullscreen reader for an explicitly opened entity dialogue or any
 * inventory-lore dialogue. */
static bool dlg_fullscreen(void) {
    return viewport_is_mobile() && s_line_count > 0 &&
           (s_fullscreen || MODAL_DIALOGUE_RENDER_ITEM == s_render);
}

static bool dlg_hidden_on_mobile(void) {
    return viewport_is_mobile() && MODAL_DIALOGUE_RENDER_ENTITY == s_render &&
           !s_fullscreen;
}

/* Desktop paired dialogue collapsed by its close button — the interact modal
 * reclaims the bottom half until the Dialog button restores it. */
static bool s_collapsed = false;

static bool dlg_hidden(void) {
    if (dlg_hidden_on_mobile()) return true;
    return !viewport_is_mobile() && MODAL_DIALOGUE_RENDER_ENTITY == s_render &&
           s_collapsed;
}

/* True while an inventory-lore dialogue is up (RENDER_ITEM) — the one opened
 * from the inventory modal's Dialog button. Lets the bar switch straight to a
 * new slot, closing this dialogue chain. */
bool modal_dialogue_is_item_lore(void) {
    return s_open && MODAL_DIALOGUE_RENDER_ITEM == s_render;
}

bool modal_dialogue_is_collapsed(void) {
    return s_open && !viewport_is_mobile() &&
           MODAL_DIALOGUE_RENDER_ENTITY == s_render && s_collapsed;
}

bool modal_dialogue_is_fullscreen(void) {
    return s_open && dlg_fullscreen();
}

/* Mobile: open the fullscreen reader. Desktop: restore the collapsed paired
 * dialogue to its bottom-half panel. */
void modal_dialogue_show_fullscreen(void) {
    if (!s_open || MODAL_DIALOGUE_RENDER_ENTITY != s_render || s_line_count <= 0) return;
    if (viewport_is_mobile()) s_fullscreen = true;
    else                      s_collapsed  = false;
    s_age = 0.0f;
}

static Rectangle panel_rect(int sw, int sh) {
    if (dlg_fullscreen()) {
        /* Below the top toolbar so the reader never collides with it. */
        float top = toolbar_height();
        float bot = (float)sh - inventory_bar_visible_height();
        if (bot <= top) bot = top + 60.0f;
        return (Rectangle){ 0.0f, top, (float)sw, bot - top };
    }
    float top = modal_interact_is_open()
              ? modal_interact_layout_bottom() + DLG_PANEL_GAP
              : sh * DLG_TOP_FRAC;
    float bot = (float)sh - inventory_bar_visible_height();
    if (bot <= top) bot = top + 60.0f;
    return (Rectangle){ 0.0f, top, (float)sw, bot - top };
}

static int dlg_side_pad(void) {
    return viewport_is_mobile() ? DLG_SIDE_PAD_MOBILE : DLG_SIDE_PAD;
}

static int dlg_pad(int sw) {
    if (viewport_is_mobile()) {
        /* Mobile: tight padding so the text column gets the space. */
        int p = (int)(sw * 0.015f);
        if (p < 6)  p = 6;
        if (p > 10) p = 10;
        return p;
    }
    int p = (int)(sw * DLG_PAD_FRAC);
    if (p < DLG_PAD_MIN) p = DLG_PAD_MIN;
    if (p > DLG_PAD_MAX) p = DLG_PAD_MAX;
    return p;
}

static int dlg_sprite_size(float card_w) {
    float frac = viewport_is_mobile() ? DLG_SPRITE_FRAC_MOBILE : DLG_SPRITE_FRAC;
    return (int)(card_w * frac);
}

static bool hit_rect(int mx, int my, Rectangle r) {
    return ((float)mx >= r.x && (float)mx < r.x + r.width &&
            (float)my >= r.y && (float)my < r.y + r.height);
}

/* Quest-talk switcher button — gold pixel-retro (ui_button_pixel_retro):
 * `icon_id` on the left (quest for a mission, close for the "Cancel Dialog"
 * state), wrapped label filling the rest. */
#define DLG_QT_MAX 8
static Rectangle s_qt_btn_rect[DLG_QT_MAX];
static int       s_qt_btn_index[DLG_QT_MAX]; /* drawn slot → quest-talk index */
static int       s_qt_btn_count = 0;

static void draw_quest_talk_btn(Rectangle r, bool selected, const char* icon_id,
                                const char* label, int font) {
    UIButtonPixelRetroStyle st = {
        .bg = (Color){ 225, 191, 5, 255 }, /* gold */
        .icon_id = icon_id, .label = label, .font_size = font,
        .selected = selected, .enabled = true, .wrap_label = true,
    };
    ui_button_pixel_retro_draw(r, &st, hit_rect(GetMouseX(), GetMouseY(), r));
}

/* Draw the left-column sprite: a single item (inventory lore) or the
 * entity's full active stack (interaction). */
static void draw_dialogue_sprite(int icon_x, int icon_y, int icon_sz) {
    ObjectLayersManager* mgr = obj_layers_mgr_get();
    if (!mgr) return;

    if (MODAL_DIALOGUE_RENDER_ITEM == s_render) {
        if (s_item_id[0] != '\0') {
            ol_as_ico_draw(mgr, s_item_id, icon_x, icon_y, icon_sz,
                           "down_idle", 0, WHITE);
        }
        return;
    }

    int lc = 0;
    const ObjectLayerState* layers = NULL;

    if (MODAL_DIALOGUE_RENDER_ENTITY == s_render) {
        /* Try the cached snapshot first (persists across AOI changes). */
        layers = modal_interact_get_cached_layers(&lc);
    }

    /* Fall back to the live bubble data. */
    if (!layers || lc <= 0) {
        layers = interaction_bubble_get_alive_layers(s_entity_id, &lc);
    }

    if (layers && lc > 0) {
        ol_stack_ico_draw(mgr, layers, lc, icon_x, icon_y, icon_sz,
                          "down_idle", 0, WHITE);
    }
}

/* ── Public API ──────────────────────────────────────────────────────── */

void modal_dialogue_init(void) {
    s_open       = false;
    s_line_count = 0;
    s_current    = 0;
    s_on_close   = NULL;
    modal_init_struct(&s_modal);
}

void modal_dialogue_open(const char* entity_id, const char* item_id,
                         const char* dialog_code, ModalDialogueRender render,
                         const DialogueLine* lines, int line_count) {
    int n = (lines && line_count > 0) ? line_count : 0;
    if (n > DIALOGUE_MAX_LINES) n = DIALOGUE_MAX_LINES;
    if (n > 0) memcpy(s_lines, lines, sizeof(DialogueLine) * n);

    s_line_count = n;
    s_current    = 0;
    s_render     = render;
    s_auto_dismiss = (MODAL_DIALOGUE_RENDER_ITEM == render);

    strncpy(s_entity_id, entity_id ? entity_id : "", sizeof(s_entity_id) - 1);
    s_entity_id[sizeof(s_entity_id) - 1] = '\0';
    strncpy(s_item_id, item_id ? item_id : "", sizeof(s_item_id) - 1);
    s_item_id[sizeof(s_item_id) - 1] = '\0';
    strncpy(s_dialog_code, dialog_code ? dialog_code : "", sizeof(s_dialog_code) - 1);
    s_dialog_code[sizeof(s_dialog_code) - 1] = '\0';

    s_age           = 0.0f;
    s_char_timer    = 0.0f;
    s_chars_visible = 0;
    s_line_complete = false;
    s_ended         = false;
    s_fullscreen    = false;
    s_collapsed     = false;
    s_open          = true;

    LOG_INFO("[MODAL_DIALOGUE] Open: entity=%s item=%s code=%s lines=%d\n",
             s_entity_id, s_item_id, s_dialog_code, s_line_count);

    /* Only a dialogue with lines freezes the player (modal protection) and
     * participates in the dlg_* / quest-talk handshake. Render-only stays
     * passive. */
    s_dlg_started = (s_line_count > 0);
    if (s_dlg_started) local_player_request_dialogue_start(s_entity_id, s_item_id);
}

/* Finish the dialogue. `completed` true → all lines were read (dlg_complete,
 * the only path that can satisfy a quest-talk objective); false → dismissed
 * early (dlg_cancel, no progress). Both release the dialogue freeze. */
static void modal_dialogue_finish(bool completed) {
    if (!s_open) return;
    s_open = false;

    LOG_INFO("[MODAL_DIALOGUE] %s: entity=%s item=%s\n",
             completed ? "Complete" : "Cancel", s_entity_id, s_item_id);

    /* Bridge-safe ordering: fire on_close BEFORE the dlg frame so a callback
     * that opens another modal (e.g. inventory) sends its freeze_start first
     * and overrides the server freeze reason; the dialogue thaw is then
     * rejected by the reason-match check — zero gap. */
    ModalDialogueOnClose cb = s_on_close;
    s_on_close = NULL;
    if (cb) cb();

    if (s_dlg_started) {
        /* Interact session still open: re-bridge the freeze to "interact"
         * before the dlg frame so the dialogue thaw is rejected and the
         * player stays protected for the rest of the session. */
        if (modal_interact_is_open())
            local_player_request_freeze(true, "interact");
        if (completed)
            local_player_request_dialogue_complete(s_entity_id, s_item_id, s_dialog_code);
        else
            local_player_request_dialogue_cancel(s_entity_id, s_item_id);
    }

    s_dlg_started = false;
    s_line_count  = 0;
    s_current     = 0;
    s_quest_style = false;
    s_fullscreen  = false;
    s_collapsed   = false;
}

void modal_dialogue_set_quest_style(bool on) {
    s_quest_style = on;
}

/* Emit dlg_complete (the server validates quest-talk objectives) WITHOUT
 * closing — the entity dialogue stays open so the player can repeat it. */
static void emit_dlg_complete(void) {
    if (!s_dlg_started) return;
    s_dlg_started = false;
    /* Same re-bridge as modal_dialogue_finish: the interact session keeps
     * the freeze across the completion frame. */
    if (modal_interact_is_open())
        local_player_request_freeze(true, "interact");
    local_player_request_dialogue_complete(s_entity_id, s_item_id, s_dialog_code);
}

void modal_dialogue_close(void) {
    modal_dialogue_finish(false);
}

void modal_dialogue_set_on_close(ModalDialogueOnClose cb) {
    s_on_close = cb;
}

bool modal_dialogue_is_open(void) { return s_open; }

void modal_dialogue_update(float dt) {
    if (!s_open) return;
    if (dlg_hidden()) return;
    s_age += dt;

    /* Typewriter: reveal one character at a time */
    if (!s_line_complete && s_current < s_line_count) {
        int total = (int)strlen(s_lines[s_current].text);
        s_char_timer += dt;
        while (s_char_timer >= DLG_TYPEWRITER_SPD && s_chars_visible < total) {
            s_char_timer -= DLG_TYPEWRITER_SPD;
            s_chars_visible++;
        }
        if (s_chars_visible >= total) {
            s_line_complete = true;
        }
    }
}

void modal_dialogue_draw(void) {
    if (!s_open || dlg_hidden()) return;

    int sw = GetScreenWidth();
    int sh = GetScreenHeight();

    /* Pop-in animation */
    float pop = 0.12f;
    float t   = s_age < pop ? s_age / pop : 1.0f;
    float ease = 1.0f - powf(1.0f - t, 3.0f);
    float fade = s_age / 0.18f;
    if (fade > 1.0f) fade = 1.0f;
    unsigned char oa = (unsigned char)(100.0f * fade);

    /* Dim only the bottom half */
    Rectangle panel = panel_rect(sw, sh);
    DrawRectangle(0, (int)panel.y, sw,
                  (int)(panel.height + inventory_bar_visible_height()),
                  (Color){ 0, 0, 0, oa });

    /* Slide-up: panel starts 20px below its rest position. The card is inset
     * horizontally by DLG_SIDE_PAD to match the interaction modal's margin so
     * the two screen halves frame with the same padding. */
    float slide_offset = 20.0f * (1.0f - ease);
    Rectangle card = {
        panel.x + dlg_side_pad(), panel.y + slide_offset,
        panel.width - 2 * dlg_side_pad(), panel.height
    };
    /* Shared modal pop: the card scales in from its centre on open and when
     * the mobile reader opens. */
    card = modal_scale_rect(card, modal_pop_scale(s_age));

    /* Card background — translucent so the world reads through. Quest-talk
     * dialogues get a yellow frame to mark the mission context; the quest icon
     * sits inline before the speaker label (below). */
    Color bg = MODAL_PANEL_BG;
    bg.a = 190;
    DrawRectangleRec(card, bg);
    if (s_quest_style) {
        DrawRectangleLinesEx(card, 2.0f, (Color){ 230, 200, 60, 230 });
    } else {
        DrawLine((int)card.x, (int)card.y,
                 (int)(card.x + card.width), (int)card.y, C_CARD_BORD);
    }

    int pad       = dlg_pad(sw);

    /* ── Layout: sprite left-aligned (half width), text to the right ── */
    float x0 = card.x;
    float y0 = card.y;

    /* Left-column width: half the card on desktop, a third on mobile (the
     * dialogue text keeps the rest). The drawn icon is capped to the available
     * card height so it never overflows the bottom edge, then centred inside
     * its column. */
    int col_w    = dlg_sprite_size(card.width);
    int avail_h  = (int)card.height - 2 * pad;
    if (avail_h < 4) avail_h = 4;
    int icon_sz  = col_w < avail_h ? col_w : avail_h;    /* cap to available height  */
    int icon_x   = (int)(x0 + (col_w - icon_sz) * 0.5f); /* centre horizontally      */
    int icon_y   = (int)(y0 + (card.height - icon_sz) * 0.5f); /* centre vertically  */

    /* Left column sprite: single item (inventory lore) or the entity's
     * full active stack (interaction). Always drawn. */
    draw_dialogue_sprite(icon_x, icon_y, icon_sz);

    /* Render-only: no dialogue lines → show the entity, no text. */
    if (s_line_count == 0) return;

    /* Close button in the top-right corner of the card. Always visible so the
     * player can dismiss the dialogue without completing all lines. */
    float fs_close_sz = 34.0f;
    s_fs_close_rect = (Rectangle){ card.x + card.width - fs_close_sz - 8.0f,
                                   card.y + 8.0f, fs_close_sz, fs_close_sz };
    UIButtonStyle cb = { .icon_id = "close-yellow", .no_fill = true };
    ui_button_draw(s_fs_close_rect, &cb, UI_BUTTON_NORMAL);

    /* Speaker name + progress — right of sprite column */
    const DialogueLine* line = &s_lines[s_current];
    int fs_speaker = DLG_FONT_SPEAKER;
    float txt_x    = x0 + col_w + pad;
    float txt_max  = card.x + card.width - txt_x - pad;
    float ty       = y0 + pad;

    /* Quest-talk switcher bar. On the default greeting, one pixel-art button
     * per available quest-talk (labelled with what it advances). Once a
     * quest-talk is selected, only that button remains — labelled "Cancel
     * Dialog" — so pressing it returns to the greeting and the full list. */
    s_qt_btn_count = 0;
    if (MODAL_DIALOGUE_RENDER_ENTITY == s_render) {
        int qt_n = modal_interact_quest_talk_count();
        if (qt_n > DLG_QT_MAX) qt_n = DLG_QT_MAX;
        int qt_sel = modal_interact_quest_talk_selected();
        int qt_font = viewport_is_mobile() ? DLG_FONT_HINT : DLG_FONT_TEXT;
        /* Reserve the top-right close button so the wide buttons clear it. */
        float bw = txt_max - (fs_close_sz + 8.0f);
        if (bw < 60.0f) bw = 60.0f;
        int label_w = (int)bw - (int)(DLG_FONT_SPEAKER + 6.0f + 18.0f);
        if (label_w < 20) label_w = 20;
        int   lo = qt_sel >= 0 ? qt_sel : 0;
        int   hi = qt_sel >= 0 ? qt_sel + 1 : qt_n;
        for (int i = lo; i < hi; i++) {
            bool        cancel   = qt_sel >= 0;
            const char* qt_label = cancel ? "Cancel Dialog" : modal_interact_quest_talk_label(i);
            const char* qt_icon  = cancel ? "close" : "quest";
            int   lh = text_wrap(qt_label, 0, 0, label_w, qt_font, WHITE, false, false);
            float bh = (float)lh + 16.0f;
            float min_bh = (float)DLG_FONT_SPEAKER + 20.0f;
            if (bh < min_bh) bh = min_bh;
            int slot = s_qt_btn_count++;
            s_qt_btn_rect[slot]  = (Rectangle){ txt_x, ty, bw, bh };
            s_qt_btn_index[slot] = i;
            draw_quest_talk_btn(s_qt_btn_rect[slot], i == qt_sel, qt_icon, qt_label, qt_font);
            ty += bh + 4.0f;
        }
        if (s_qt_btn_count > 0) ty += 2.0f;
    }

    if (line->speaker[0] != '\0') {
        /* Quest-talk dialogues prefix the speaker label with the quest icon. */
        int name_x = (int)txt_x;
        if (s_quest_style) {
            float qs = (float)fs_speaker;
            ui_icon_draw_ex("quest", txt_x + qs * 0.5f, ty + qs * 0.5f, qs, 0.0f, WHITE);
            name_x = (int)(txt_x + qs + 6.0f);
        }
        DrawText(line->speaker, name_x, (int)ty, fs_speaker, C_SPEAKER);
    }

    if (s_line_count > 1) {
        char prog[16];
        snprintf(prog, sizeof(prog), "%d / %d", s_current + 1, s_line_count);
        int pfs = DLG_FONT_HINT;
        int pw  = MeasureText(prog, pfs);
        /* Reserve the close button's slot so they never overlap. */
        float prog_right = card.x + card.width - pad - fs_close_sz - 10.0f;
        DrawText(prog, (int)(prog_right - pw), (int)(ty + 2), pfs, C_HINT);
    }

    float text_y = ty + fs_speaker + 6.0f;

    /* ── Dialogue text (typewriter, word-wrapped) ──────────────────── */
    int fs = DLG_FONT_TEXT;

    char partial[DIALOGUE_MAX_TEXT];
    int len = s_chars_visible;
    if (len > (int)sizeof(partial) - 1) len = (int)sizeof(partial) - 1;
    if (len > (int)strlen(line->text))  len = (int)strlen(line->text);
    memcpy(partial, line->text, len);
    partial[len] = '\0';

    {
        char copy[DIALOGUE_MAX_TEXT];
        strncpy(copy, partial, sizeof(copy) - 1);
        copy[sizeof(copy) - 1] = '\0';

        char line_buf[512] = {0};
        float cur_y = text_y;
        char* tok = strtok(copy, " ");
        while (tok) {
            char test[512];
            if (line_buf[0] == '\0')
                snprintf(test, sizeof(test), "%s", tok);
            else
                snprintf(test, sizeof(test), "%s %s", line_buf, tok);

            if (MeasureText(test, fs) > (int)txt_max && line_buf[0] != '\0') {
                DrawText(line_buf, (int)txt_x, (int)cur_y, fs, C_TEXT);
                cur_y += fs + 4;
                snprintf(line_buf, sizeof(line_buf), "%s", tok);
            } else {
                snprintf(line_buf, sizeof(line_buf), "%s", test);
            }
            tok = strtok(NULL, " ");
        }
        if (line_buf[0] != '\0') {
            DrawText(line_buf, (int)txt_x, (int)cur_y, fs, C_TEXT);
        }
    }

    /* ── Hint at bottom-right of panel ─────────────────────────────── */
    const char* hint;
    if (!s_line_complete)
        hint = "...";
    else if (s_current + 1 < s_line_count)
        hint = "[Tap to continue]";
    else if (s_auto_dismiss)
        hint = "[Tap to close]";
    else if (s_ended)
        hint = "[Repeat Dialog]";
    else
        hint = "[Tap to finish]";

    int hfs = DLG_FONT_HINT;
    int htw = MeasureText(hint, hfs);
    DrawText(hint,
             (int)(card.x + card.width - htw - pad),
             (int)(card.y + card.height - hfs - pad * 0.5f),
             hfs, C_HINT);
}

bool modal_dialogue_handle_click(int mx, int my) {
    if (!s_open || dlg_hidden()) return false;
    if (s_age < 0.12f) return true;  /* block during pop-in */

    int sw = GetScreenWidth();
    int sh = GetScreenHeight();
    Rectangle card = panel_rect(sw, sh);

    /* Close button: the mobile reader collapses to the hidden state; a
     * desktop paired dialogue collapses so the interact modal reclaims its
     * space (the Dialog button restores it); anything else dismisses. */
    if (hit_rect(mx, my, s_fs_close_rect)) {
        if (viewport_is_mobile() && s_fullscreen) {
            s_fullscreen = false;
            s_age = 0.0f;
        } else if (!viewport_is_mobile() && MODAL_DIALOGUE_RENDER_ENTITY == s_render &&
                   modal_interact_is_open()) {
            s_collapsed = true;
        } else {
            modal_dialogue_finish(false);
        }
        return true;
    }

    /* Quest-talk switcher: select a quest-talk, or (when one is selected) the
     * lone "Cancel Dialog" button returns to the greeting. */
    for (int i = 0; i < s_qt_btn_count; i++) {
        if (!hit_rect(mx, my, s_qt_btn_rect[i])) continue;
        int qt = s_qt_btn_index[i];
        modal_interact_set_quest_talk(qt == modal_interact_quest_talk_selected() ? -1 : qt);
        return true;
    }

    bool inside = hit_rect(mx, my, card);

    if (!inside) {
        /* The fullscreen reader owns the whole screen — swallow. */
        if (dlg_fullscreen()) return true;
        /* Inventory lore dismisses itself; the paired interaction view lets
         * modal_interact own dismissal, so let the tap fall through. */
        if (!s_auto_dismiss) return false;
        modal_dialogue_finish(false);
        return true;
    }

    /* Render-only: nothing to advance, just swallow the tap. */
    if (s_line_count == 0) return true;

    /* Inside card → advance the typewriter / line, or complete. */
    if (!s_line_complete) {
        /* Reveal full line immediately */
        s_chars_visible = (int)strlen(s_lines[s_current].text);
        s_line_complete = true;
    } else if (s_current + 1 < s_line_count) {
        /* Next line */
        s_current++;
        s_chars_visible = 0;
        s_char_timer    = 0.0f;
        s_line_complete = false;
    } else if (s_auto_dismiss) {
        /* Inventory lore: read-through closes the modal. */
        modal_dialogue_finish(true);
    } else if (!s_ended) {
        /* Entity dialogue: first full read emits the dlg_complete the server
         * validates for quest-talk, then stays open showing "Repeat Dialog". */
        s_ended = true;
        emit_dlg_complete();
        /* A finished mobile entity dialogue returns to the interact modal.
         * Reading state resets so the footer opens a fresh visual read. */
        if (viewport_is_mobile() && s_fullscreen &&
            MODAL_DIALOGUE_RENDER_ENTITY == s_render) {
            s_fullscreen    = false;
            s_age           = 0.0f; /* replay the modal pop for the mode change */
            s_current       = 0;
            s_chars_visible = 0;
            s_char_timer    = 0.0f;
            s_line_complete = false;
            s_ended         = false;
        }
    } else {
        /* Repeat Dialog → re-read from the top (visual only). */
        s_current       = 0;
        s_chars_visible = 0;
        s_char_timer    = 0.0f;
        s_line_complete = false;
        s_ended         = false;
    }

    return true;
}
