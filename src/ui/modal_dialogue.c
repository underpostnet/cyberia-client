/**
 * @file modal_dialogue.c
 * @brief Dialogue/lore modal implementation — uses modal.c for rendering.
 *
 * Architecture:
 *   - Wraps a Modal struct (from modal.c) for the text box rendering.
 *   - Manages its own state for line progression and typewriter effect.
 *   - Sends "dialogue_start" / "dialogue_end" JSON messages to the Go
 *     server so it can grant / revoke damage immunity.
 *   - Item sprite is rendered via ol_as_animated_ico beside the speaker.
 */

#include "modal_dialogue.h"

#include "domain/local_player.h"
#include "game_state.h"
#include "modal.h"
#include "ol_as_animated_ico.h"
#include "util/log.h"

#include <assert.h>
#include <math.h>
#include <raylib.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ── Module state ─────────────────────────────────────────────────────── */

static ObjectLayersManager* s_ol_manager = NULL;
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
#define DLG_TOP_FRAC        0.50f
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

/* ── Public API ──────────────────────────────────────────────────────── */

void modal_dialogue_init(ObjectLayersManager* ol_manager) {
    s_ol_manager = ol_manager;
    s_open       = false;
    s_line_count = 0;
    s_current    = 0;
    s_on_close   = NULL;
    modal_init_struct(&s_modal);
}

void modal_dialogue_open(const char* entity_id, const char* item_id,
                         const char* dialog_code,
                         const DialogueLine* lines, int line_count) {
    if (!lines || line_count <= 0) return;

    int n = line_count;
    if (n > DIALOGUE_MAX_LINES) n = DIALOGUE_MAX_LINES;

    memcpy(s_lines, lines, sizeof(DialogueLine) * n);
    s_line_count = n;
    s_current    = 0;

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
    s_open          = true;

    LOG_INFO("[MODAL_DIALOGUE] Open: entity=%s item=%s code=%s lines=%d\n",
             s_entity_id, s_item_id, s_dialog_code, s_line_count);

    /* dlg_start freezes the player server-side (modal protection). */
    local_player_request_dialogue_start(s_entity_id, s_item_id);
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

    if (completed)
        local_player_request_dialogue_complete(s_entity_id, s_item_id, s_dialog_code);
    else
        local_player_request_dialogue_cancel(s_entity_id, s_item_id);

    s_line_count = 0;
    s_current    = 0;
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
    if (!s_open || s_current >= s_line_count) return;

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

    /* Slide-up: panel starts 20px below its rest position */
    float slide_offset = 20.0f * (1.0f - ease);
    Rectangle card = {
        panel.x, panel.y + slide_offset,
        panel.width, panel.height
    };

    /* Card background with subtle top border — standardized panel fill. */
    DrawRectangleRec(card, MODAL_PANEL_BG);
    DrawLine((int)card.x, (int)card.y,
             (int)(card.x + card.width), (int)card.y, C_CARD_BORD);

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

    /* Item sprite — left column, centred, no border */
    if (s_item_id[0] != '\0' && s_ol_manager) {
        ol_as_ico_draw(s_ol_manager, s_item_id,
                       icon_x, icon_y, icon_sz,
                       "down_idle", 0, WHITE);
    }

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
    else
        hint = "[Tap to close]";

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
        /* Tap outside → dismiss early (dlg_cancel) */
        modal_dialogue_finish(false);
        return true;
    }

    /* Inside card → advance */
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
    } else {
        /* All lines read → complete (dlg_complete) */
        modal_dialogue_finish(true);
    }

    return true;
}
