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
#include "game_state.h"
#include "interaction_bubble.h"
#include "modal_interact.h"
#include "modal.h"
#include "object_layer.h"
#include "object_layers_management.h"
#include "ol_as_animated_ico.h"
#include "ol_stack_ico.h"
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

#define DLG_SPRITE_FRAC     0.50f   /* sprite width as fraction of card width  */
#define DLG_FONT_SPEAKER    22
#define DLG_FONT_TEXT       18
#define DLG_FONT_HINT       14
#define DLG_PAD_FRAC        0.03f   /* padding as fraction of panel width      */
#define DLG_PAD_MIN         12
#define DLG_PAD_MAX         32
#define DLG_TYPEWRITER_SPD  0.022f  /* seconds per character                   */

/* Vertical split: panel occupies [50 % … bottom − bar_h] of the screen.   */
#define DLG_TOP_FRAC        0.56f
#define DLG_SIDE_PAD        18      /* matches modal_interact MI_PAD            */
#define DLG_BAR_OFFSET      72      /* inventory bar height                    */

/* ── Colours ──────────────────────────────────────────────────────────── */

static const Color C_SPEAKER   = { 200, 220, 255, 255 };
static const Color C_TEXT      = { 220, 220, 230, 240 };
static const Color C_HINT      = { 140, 140, 160, 180 };
static const Color C_CARD_BORD = {  70,  70, 120, 200 };
/* ── Helpers ──────────────────────────────────────────────────────────── */

static Rectangle panel_rect(int sw, int sh) {
    float top = sh * DLG_TOP_FRAC;
    float bot = (float)(sh - DLG_BAR_OFFSET);
    if (bot <= top) bot = top + 60.0f;
    return (Rectangle){ 0.0f, top, (float)sw, bot - top };
}

static int dlg_pad(int sw) {
    int p = (int)(sw * DLG_PAD_FRAC);
    if (p < DLG_PAD_MIN) p = DLG_PAD_MIN;
    if (p > DLG_PAD_MAX) p = DLG_PAD_MAX;
    return p;
}

static int dlg_sprite_size(float card_w) {
    return (int)(card_w * DLG_SPRITE_FRAC);
}

static bool hit_rect(int mx, int my, Rectangle r) {
    return ((float)mx >= r.x && (float)mx < r.x + r.width &&
            (float)my >= r.y && (float)my < r.y + r.height);
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
        if (completed)
            local_player_request_dialogue_complete(s_entity_id, s_item_id, s_dialog_code);
        else
            local_player_request_dialogue_cancel(s_entity_id, s_item_id);
    }

    s_dlg_started = false;
    s_line_count  = 0;
    s_current     = 0;
    s_quest_style = false;
}

void modal_dialogue_set_quest_style(bool on) {
    s_quest_style = on;
}

/* Emit dlg_complete (the server validates quest-talk objectives) WITHOUT
 * closing — the entity dialogue stays open so the player can repeat it. */
static void emit_dlg_complete(void) {
    if (!s_dlg_started) return;
    s_dlg_started = false;
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
    if (!s_open) return;

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
    DrawRectangle(0, (int)panel.y, sw, (int)panel.height + DLG_BAR_OFFSET,
                  (Color){ 0, 0, 0, oa });

    /* Slide-up: panel starts 20px below its rest position. The card is inset
     * horizontally by DLG_SIDE_PAD to match the interaction modal's margin so
     * the two screen halves frame with the same padding. */
    float slide_offset = 20.0f * (1.0f - ease);
    Rectangle card = {
        panel.x + DLG_SIDE_PAD, panel.y + slide_offset,
        panel.width - 2 * DLG_SIDE_PAD, panel.height
    };

    /* Card background — translucent so the world reads through. Quest-talk
     * dialogues get a yellow frame + quest icon to mark the mission context. */
    Color bg = MODAL_PANEL_BG;
    bg.a = 190;
    DrawRectangleRec(card, bg);
    if (s_quest_style) {
        DrawRectangleLinesEx(card, 2.0f, (Color){ 230, 200, 60, 230 });
        int qs = 22;
        ui_icon_draw("quest", card.x + 22, card.y + qs + 3, qs, false, 0.0f);
    } else {
        DrawLine((int)card.x, (int)card.y,
                 (int)(card.x + card.width), (int)card.y, C_CARD_BORD);
    }

    int pad       = dlg_pad(sw);

    /* ── Layout: sprite left-aligned (half width), text to the right ── */
    float x0 = card.x;
    float y0 = card.y;

    /* Left-column width is always half the card; the drawn icon is
     * capped to the available card height so it never overflows the
     * bottom edge.  The icon is then centred inside its column.       */
    int col_w    = dlg_sprite_size(card.width);           /* column = half card width */
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

    /* Speaker name + progress — right of sprite column */
    const DialogueLine* line = &s_lines[s_current];
    int fs_speaker = DLG_FONT_SPEAKER;
    float txt_x    = x0 + col_w + pad;
    float txt_max  = card.x + card.width - txt_x - pad;
    float ty       = y0 + pad;

    if (line->speaker[0] != '\0') {
        DrawText(line->speaker, (int)txt_x, (int)ty, fs_speaker, C_SPEAKER);
    }

    if (s_line_count > 1) {
        char prog[16];
        snprintf(prog, sizeof(prog), "%d / %d", s_current + 1, s_line_count);
        int pfs = DLG_FONT_HINT;
        int pw  = MeasureText(prog, pfs);
        DrawText(prog,
                 (int)(card.x + card.width - pw - pad),
                 (int)(ty + 2), pfs, C_HINT);
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

        char line_buf[256] = {0};
        float cur_y = text_y;
        char* tok = strtok(copy, " ");
        while (tok) {
            char test[256];
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
    if (!s_open) return false;
    if (s_age < 0.12f) return true;  /* block during pop-in */

    int sw = GetScreenWidth();
    int sh = GetScreenHeight();
    Rectangle card = panel_rect(sw, sh);

    bool inside = hit_rect(mx, my, card);

    if (!inside) {
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
