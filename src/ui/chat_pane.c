#include "chat_pane.h"
#include "emoji.h"
#include "identity_icon.h"
#include "modal.h"
#include "text.h"

#include "view/viewport.h"
#include "js/text_input_bridge.h"
#include "net/client_event.h"
#include "chat_log.h"
#include "ui_button.h"
#include "ui_scroll.h"
#include "utils.h"

#include <assert.h>
#include <ctype.h>
#include <raylib.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#define CHAT_DRAW_MAX     50
#define CHAT_GAP          6.0f
#define CHAT_SEND_W       76.0f

static const Color C_MINE     = { 120, 180, 240, 255 };
static const Color C_THEIRS   = { 200, 200, 220, 230 };
static const Color C_SYSTEM   = { 230, 170, 110, 235 };
static const Color C_HINT     = { 130, 140, 165, 200 };
static const Color C_SEND     = {  35,  80, 160, 255 };
static const Color C_INPUT_BG = {  18,  18,  36, 225 };

typedef struct {
    Rectangle list;
    Rectangle input;
    Rectangle emoji;  /* the square toggle of the emoji menu */
    Rectangle send;
    Rectangle menu;   /* the emoji menu over the bottom of the history */
    int       menu_columns;
} ChatLayout;

/* The pane that shows the one DOM input, and where. */
static ChatPane* s_input_owner;
static Rectangle s_input_rect;

static inline int   chat_font(void)  { return viewport_is_mobile() ? 12 : 14; }
static inline float chat_row_h(void) { return viewport_is_mobile() ? 30.0f : 34.0f; }

/* The input row sits on the bottom edge and the history takes the rest. The
 * emoji menu opens over the history, right-aligned above its toggle. */
static ChatLayout chat_layout(Rectangle content) {
    ChatLayout l = { 0 };
    float row_h = chat_row_h();
    float input_y = content.y + content.height - row_h;
    l.send  = (Rectangle){ content.x + content.width - CHAT_SEND_W, input_y, CHAT_SEND_W, row_h };
    l.emoji = (Rectangle){ l.send.x - CHAT_GAP - row_h, input_y, row_h, row_h };
    l.input = (Rectangle){ content.x, input_y, l.emoji.x - CHAT_GAP - content.x, row_h };
    l.list  = (Rectangle){ content.x, content.y, content.width, input_y - CHAT_GAP - content.y };
    if (0.0f > l.list.height) l.list.height = 0.0f;

    float cell = row_h + CHAT_GAP;
    int fit = (int)((content.width - CHAT_GAP) / cell);
    int rows = (CYBERIA_CHAT_EMOJI_COUNT + fit - 1) / (0 < fit ? fit : 1);
    l.menu_columns = (CYBERIA_CHAT_EMOJI_COUNT + rows - 1) / (0 < rows ? rows : 1);
    float menu_w = (float)l.menu_columns * cell + CHAT_GAP;
    float menu_h = (float)rows * cell + CHAT_GAP;
    l.menu = (Rectangle){ content.x + content.width - menu_w, input_y - CHAT_GAP - menu_h, menu_w, menu_h };
    return l;
}

static Rectangle emoji_cell(const ChatLayout* l, int index) {
    float side = chat_row_h();
    float cell = side + CHAT_GAP;
    return (Rectangle){ l->menu.x + CHAT_GAP + (float)(index % l->menu_columns) * cell,
                        l->menu.y + CHAT_GAP + (float)(index / l->menu_columns) * cell, side, side };
}

/* Appends the token of an emoji to the typed text, inside the rune limit. */
static void insert_emoji(int index) {
    char text[CHAT_LINE_BYTES];
    size_t length = text_input_bridge_read(text, sizeof text);
    const char* space = 0 < length && ' ' != text[length - 1] ? " " : "";
    char token[40];
    snprintf(token, sizeof token, "%s:%s: ", space, CYBERIA_CHAT_EMOJI[index]);
    if (CYBERIA_CHAT_TEXT_MAX_RUNES < GetCodepointCount(text) + (int)strlen(token)) return;
    snprintf(text + length, sizeof text - length, "%s", token);
    text_input_bridge_set(text);
}

/* "14:32" for a line of today, "Oct 17 14:32" for an older one. */
static void time_label(double at_ms, char* out, size_t size) {
    time_t now = time(NULL);
    time_t at = (time_t)(at_ms / 1000.0);
    struct tm today = *localtime(&now);
    struct tm line = *localtime(&at);
    bool same_day = today.tm_year == line.tm_year && today.tm_yday == line.tm_yday;
    strftime(out, size, same_day ? "%H:%M" : "%b %d %H:%M", &line);
}

/* Takes the typed line, trimmed, and pushes it. The history shows it once the server echoes it. */
static void send_input(const ChatPane* pane) {
    char line[CHAT_LINE_BYTES];
    text_input_bridge_take(line, sizeof(line));
    char* start = line;
    while (isspace((unsigned char)*start)) start++;
    size_t length = strlen(start);
    while (0 < length && isspace((unsigned char)start[length - 1])) start[--length] = '\0';
    if (0 == length) return;
    client_event_payload_t p = { .channel = pane->conversation.channel,
                                 .entity_id = pane->conversation.peer };
    copy_str(p.text, sizeof(p.text), start);
    client_event_push(CLIENT_EVENT_CHAT, p);
}

static bool same_pixels(Rectangle a, Rectangle b) {
    return (int)a.x == (int)b.x && (int)a.y == (int)b.y &&
           (int)a.width == (int)b.width && (int)a.height == (int)b.height;
}

void chat_pane_bind(ChatPane* pane, ChatConversation conversation) {
    assert(pane);
    if (conversation.channel == pane->conversation.channel &&
        conversation.peer == pane->conversation.peer) return;
    if (s_input_owner == pane) {
        /* A draft belongs to the conversation it was typed for. */
        char draft[CHAT_LINE_BYTES];
        text_input_bridge_take(draft, sizeof(draft));
    }
    pane->conversation = conversation;
    pane->emoji_open = false;
    ui_scroll_reset(&pane->scroll);
    pane->content_h  = 0.0f;
    pane->seen_count = -1;
}

void chat_pane_update(ChatPane* pane, Rectangle content, bool input_visible, float dt) {
    assert(pane);
    ChatLayout l = chat_layout(content);

    /* The pane shows every line, so none stays unread. */
    chat_log_mark_read(pane->conversation);

    const ChatHistory* h = chat_log_get(pane->conversation);
    int    count = NULL != h ? h->count : 0;
    double ts    = 0 < count ? h->lines[count - 1].at_ms : 0.0;
    if (count != pane->seen_count || ts != pane->seen_ts || l.list.height != pane->scroll.view.height) {
        pane->stick = true;
        pane->seen_count = count;
        pane->seen_ts = ts;
    }

    ui_scroll_update(&pane->scroll, l.list, pane->content_h, dt);

    if (!input_visible) {
        chat_pane_hide(pane);
        return;
    }
    if (s_input_owner != pane || !same_pixels(s_input_rect, l.input)) {
        if (s_input_owner != pane) {
            char draft[CHAT_LINE_BYTES];
            text_input_bridge_take(draft, sizeof(draft));
            pane->stick = true;
        }
        text_input_bridge_show((int)l.input.x, (int)l.input.y, (int)l.input.width,
                               (int)l.input.height, CYBERIA_CHAT_TEXT_MAX_RUNES, "Say something...");
        s_input_owner = pane;
        s_input_rect = l.input;
    }
    if (text_input_bridge_submitted()) send_input(pane);
}

void chat_pane_draw(ChatPane* pane, Rectangle content) {
    assert(pane);
    ChatLayout l = chat_layout(content);
    int font = chat_font();
    int stamp_font = font - 2;
    int mx = GetMouseX();
    int my = GetMouseY();

    const ChatHistory* h = chat_log_get(pane->conversation);
    int count = NULL != h ? h->count : 0;

    ui_scroll_begin(&pane->scroll);
    float top = l.list.y - ui_scroll_offset(&pane->scroll);
    float y = top;
    if (0 == count) {
        DrawText("No messages yet...", (int)l.list.x, (int)y, font, C_HINT);
        y += (float)text_line_height(font);
    }
    /* ponytail: wraps the newest CHAT_DRAW_MAX lines every frame; cache the
     * wrapped heights if a long history shows up in a profile. */
    for (int i = CHAT_DRAW_MAX < count ? count - CHAT_DRAW_MAX : 0; count > i; i++) {
        const ChatLine* m = &h->lines[i];
        if (0 == m->from_id) {
            y += (float)text_wrap(m->text, (int)l.list.x, (int)y, (int)l.list.width, font, C_SYSTEM, false, true) +
                 CHAT_GAP;
            continue;
        }
        float face = (float)text_line_height(font);
        identity_icon_draw(m->from_name, (Rectangle){ l.list.x, y, face, face });
        char when[24];
        time_label(m->at_ms, when, sizeof when);
        int stamp_w = MeasureText(when, stamp_font);
        int right = (int)(l.list.x + l.list.width);
        int name_x = (int)(l.list.x + face + CHAT_GAP);
        modal_draw_clipped_text(m->mine ? "You" : m->from_name, name_x, (int)y,
                                right - stamp_w - (int)CHAT_GAP - name_x, font, m->mine ? C_MINE : C_THEIRS);
        DrawText(when, right - stamp_w, (int)y + font - stamp_font, stamp_font, C_HINT);
        y += face;
        y += (float)emoji_text_wrap(m->text, (int)l.list.x, (int)y, (int)l.list.width, font, MODAL_TEXT, true) +
             CHAT_GAP;
    }
    pane->content_h = y - top;
    ui_scroll_end(&pane->scroll);
    if (pane->stick) {
        /* The next update clamps the offset to the scroll range. */
        pane->scroll.offset = pane->content_h - l.list.height;
        pane->stick = false;
    }

    /* The DOM input covers this frame while it shows. */
    DrawRectangleRec(l.input, C_INPUT_BG);

    UIButtonPixelRetroStyle send = {
        .bg = C_SEND, .label = "Send", .font_size = font, .text_color = MODAL_TEXT, .enabled = true,
    };
    ui_button_pixel_retro_draw(l.send, &send, ui_button_hit(l.send, mx, my));
    UIButtonPixelRetroStyle toggle = {
        .bg = MODAL_BUTTON, .icon_id = "emoji-smile", .selected = pane->emoji_open, .enabled = true,
    };
    ui_button_pixel_retro_draw(l.emoji, &toggle, ui_button_hit(l.emoji, mx, my));
    if (!pane->emoji_open) return;

    DrawRectangleRec(l.menu, MODAL_PANEL_BG);
    DrawRectangleLinesEx(l.menu, 1.0f, MODAL_PANEL_BORDER);
    for (int i = 0; CYBERIA_CHAT_EMOJI_COUNT > i; i++) {
        char icon[40];
        snprintf(icon, sizeof(icon), "emoji-%s", CYBERIA_CHAT_EMOJI[i]);
        Rectangle cell = emoji_cell(&l, i);
        UIButtonPixelRetroStyle emoji = { .bg = MODAL_BUTTON, .icon_id = icon, .enabled = true };
        ui_button_pixel_retro_draw(cell, &emoji, ui_button_hit(cell, mx, my));
    }
}

void chat_pane_press(ChatPane* pane, Rectangle content, int mx, int my) {
    assert(pane);
    ChatLayout l = chat_layout(content);
    if (ui_button_hit(l.emoji, mx, my)) {
        pane->emoji_open = !pane->emoji_open;
        return;
    }
    bool menu = pane->emoji_open;
    pane->emoji_open = false;
    if (menu && ui_button_hit(l.menu, mx, my)) {
        ui_scroll_cancel_press(&pane->scroll);
        for (int i = 0; CYBERIA_CHAT_EMOJI_COUNT > i; i++)
            if (ui_button_hit(emoji_cell(&l, i), mx, my)) insert_emoji(i);
        return;
    }
    if (ui_button_hit(l.send, mx, my)) {
        send_input(pane);
        return;
    }
    if (CheckCollisionPointRec((Vector2){ (float)mx, (float)my }, l.list)) {
        ui_scroll_on_press(&pane->scroll, mx, my);
    }
}

bool chat_pane_wheel(ChatPane* pane, Rectangle content, float wheel_delta) {
    assert(pane);
    return ui_scroll_on_wheel(&pane->scroll, chat_layout(content).list, pane->content_h, wheel_delta);
}

void chat_pane_hide(ChatPane* pane) {
    pane->emoji_open = false;
    if (s_input_owner != pane) return;
    text_input_bridge_hide();
    s_input_owner = NULL;
}
