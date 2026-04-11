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
#include "modal.h"
#include "ol_as_animated_ico.h"
#include "game_state.h"
#include "client.h"
#include <raylib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

/* ── Module state ─────────────────────────────────────────────────────── */

static ObjectLayersManager* s_ol_manager = NULL;
static bool  s_open       = false;
static float s_age        = 0.0f;

/* Dialogue content */
static DialogueLine s_lines[DIALOGUE_MAX_LINES];
static int          s_line_count  = 0;
static int          s_current     = 0;   /* index of the line being displayed */

/* Entity / item context */
static char s_entity_id[64] = {0};
static char s_item_id[128]  = {0};

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

#define DLG_SPRITE_FRAC     0.22f   /* sprite size as fraction of panel height */
#define DLG_SPRITE_MIN      48      /* minimum sprite px                       */
#define DLG_SPRITE_MAX      128     /* maximum sprite px                       */
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
static const Color C_CARD_BG   = {  12,  12,  24, 230 };
static const Color C_CARD_BORD = {  70,  70, 120, 200 };

/* ── Helpers ──────────────────────────────────────────────────────────── */

static void send_dialogue_msg(const char* type) {
    char buf[256];
    snprintf(buf, sizeof(buf),
        "{\"type\":\"%s\",\"payload\":{\"entityId\":\"%s\",\"itemId\":\"%s\"}}",
        type, s_entity_id, s_item_id);
    int rc = client_send(buf);
    printf("[MODAL_DIALOGUE] WS -> %s (entity=%s, item=%s) rc=%d\n",
           type, s_entity_id, s_item_id, rc);
}

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

static int dlg_sprite_size(float panel_h) {
    int s = (int)(panel_h * DLG_SPRITE_FRAC);
    if (s < DLG_SPRITE_MIN) s = DLG_SPRITE_MIN;
    if (s > DLG_SPRITE_MAX) s = DLG_SPRITE_MAX;
    return s;
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

    s_age           = 0.0f;
    s_char_timer    = 0.0f;
    s_chars_visible = 0;
    s_line_complete = false;
    s_open          = true;

    printf("[MODAL_DIALOGUE] Open: entity=%s item=%s lines=%d\n",
           s_entity_id, s_item_id, s_line_count);

    /* Notify server → immunity starts */
    send_dialogue_msg("dialogue_start");
}

void modal_dialogue_close(void) {
    if (!s_open) return;
    s_open = false;

    printf("[MODAL_DIALOGUE] Close: entity=%s item=%s\n", s_entity_id, s_item_id);

    /* Notify server → immunity ends */
    send_dialogue_msg("dialogue_end");
    s_line_count = 0;
    s_current    = 0;

    /* Fire and clear the one-shot on_close callback */
    ModalDialogueOnClose cb = s_on_close;
    s_on_close = NULL;
    if (cb) cb();
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

    /* Card background with subtle top border */
    DrawRectangleRec(card, C_CARD_BG);
    DrawLine((int)card.x, (int)card.y,
             (int)(card.x + card.width), (int)card.y, C_CARD_BORD);

    int pad       = dlg_pad(sw);
    int sprite_sz = dlg_sprite_size(card.height);

    /* ── Layout: sprite left-aligned, text area to the right ─────── */
    float x0 = card.x + pad;
    float y0 = card.y + pad;

    /* Item sprite (ol as icon) */
    if (s_item_id[0] != '\0' && s_ol_manager) {
        ol_as_ico_draw(s_ol_manager, s_item_id,
                       (int)x0, (int)y0, sprite_sz,
                       "down_idle", 0, WHITE);
    }
    DrawRectangleLinesEx((Rectangle){ x0, y0, (float)sprite_sz, (float)sprite_sz },
                         1.0f, C_CARD_BORD);

    /* Speaker name — right of sprite, vertically centred with sprite top */
    float txt_x = x0 + sprite_sz + pad;
    float txt_max_w = card.x + card.width - txt_x - pad;
    const DialogueLine* line = &s_lines[s_current];

    int fs_speaker = DLG_FONT_SPEAKER;
    if (line->speaker[0] != '\0') {
        DrawText(line->speaker, (int)txt_x, (int)y0, fs_speaker, C_SPEAKER);
    }

    /* Progress indicator (e.g. "2 / 5") next to speaker or right-aligned */
    if (s_line_count > 1) {
        char prog[16];
        snprintf(prog, sizeof(prog), "%d / %d", s_current + 1, s_line_count);
        int pfs = DLG_FONT_HINT;
        int pw  = MeasureText(prog, pfs);
        DrawText(prog,
                 (int)(card.x + card.width - pw - pad),
                 (int)(y0 + 2), pfs, C_HINT);
    }

    /* ── Dialogue text (typewriter, word-wrapped) ───────────────────── */
    float text_y = y0 + fs_speaker + 8.0f;
    int   fs     = DLG_FONT_TEXT;

    /* Build the partial string for the typewriter */
    char partial[DIALOGUE_MAX_TEXT];
    int len = s_chars_visible;
    if (len > (int)sizeof(partial) - 1) len = (int)sizeof(partial) - 1;
    if (len > (int)strlen(line->text))  len = (int)strlen(line->text);
    memcpy(partial, line->text, len);
    partial[len] = '\0';

    /* Word-wrap rendering */
    {
        char copy[DIALOGUE_MAX_TEXT];
        strncpy(copy, partial, sizeof(copy) - 1);
        copy[sizeof(copy) - 1] = '\0';

        char line_buf[256] = {0};
        float cur_y = text_y;
        float max_w = txt_max_w;
        char* tok = strtok(copy, " ");
        while (tok) {
            char test[256];
            if (line_buf[0] == '\0')
                snprintf(test, sizeof(test), "%s", tok);
            else
                snprintf(test, sizeof(test), "%s %s", line_buf, tok);

            if (MeasureText(test, fs) > (int)max_w && line_buf[0] != '\0') {
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

bool modal_dialogue_handle_click(int mx, int my, bool clicked) {
    if (!s_open) return false;
    if (s_age < 0.12f) return true;  /* block during pop-in */
    if (!clicked)       return true;  /* consume motion */

    int sw = GetScreenWidth();
    int sh = GetScreenHeight();
    Rectangle card = panel_rect(sw, sh);

    bool inside = hit_rect(mx, my, card);

    if (!inside) {
        /* Tap outside → close early */
        modal_dialogue_close();
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
        /* All lines read → close */
        modal_dialogue_close();
    }

    return true;
}
