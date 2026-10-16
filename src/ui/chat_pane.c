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

#define CHAT_DRAW_MAX     50
#define CHAT_GAP          6.0f
#define CHAT_SEND_W       76.0f
#define CHAT_PRESET_COUNT 6

static const char* CHAT_PRESETS[CHAT_PRESET_COUNT] = {
    "Hello!", "GG", "Help!", "Trade?", "Follow me", "Thanks!",
};

static const Color C_MINE     = { 120, 180, 240, 255 };
static const Color C_THEIRS   = { 200, 200, 220, 230 };
static const Color C_SYSTEM   = { 230, 170, 110, 235 };
static const Color C_HINT     = { 130, 140, 165, 200 };
static const Color C_BTN      = {  24,  30,  48, 255 };
static const Color C_SEND     = {  35,  80, 160, 255 };
static const Color C_INPUT_BG = {  18,  18,  36, 225 };

typedef struct {
    Rectangle list;
    Rectangle input;
    Rectangle send;
    Rectangle presets[CHAT_PRESET_COUNT];
    Rectangle emoji[CYBERIA_CHAT_EMOJI_COUNT];
} ChatLayout;

/* The pane that shows the one DOM input, and where. */
static ChatPane* s_input_owner;
static Rectangle s_input_rect;

static inline int   chat_font(void)  { return viewport_is_mobile() ? 12 : 14; }
static inline float chat_row_h(void) { return viewport_is_mobile() ? 30.0f : 34.0f; }

/* Places one quick-send button of width `w` in the wrapping flow. */
static Rectangle flow(Rectangle content, float w, float row_h, float* x, float* y) {
    if (w > content.width) w = content.width;
    if (0.0f < *x && *x + w > content.width) {
        *x = 0.0f;
        *y += row_h + CHAT_GAP;
    }
    Rectangle r = { content.x + *x, *y, w, row_h };
    *x += w + CHAT_GAP;
    return r;
}

/* Presets and emoji wrap along the bottom edge, the input row sits above
 * them, and the history takes the rest. */
static ChatLayout chat_layout(Rectangle content) {
    ChatLayout l = { 0 };
    int   font  = chat_font();
    float row_h = chat_row_h();
    float x = 0.0f;
    float y = 0.0f;
    for (int i = 0; CHAT_PRESET_COUNT > i; i++)
        l.presets[i] = flow(content, (float)MeasureText(CHAT_PRESETS[i], font) + 2.0f * UI_BUTTON_DEFAULT_PAD, row_h, &x, &y);
    for (int i = 0; CYBERIA_CHAT_EMOJI_COUNT > i; i++) l.emoji[i] = flow(content, row_h, row_h, &x, &y);
    float presets_y = content.y + content.height - (y + row_h);
    for (int i = 0; CHAT_PRESET_COUNT > i; i++) l.presets[i].y += presets_y;
    for (int i = 0; CYBERIA_CHAT_EMOJI_COUNT > i; i++) l.emoji[i].y += presets_y;

    float input_y = presets_y - CHAT_GAP - row_h;
    l.send  = (Rectangle){ content.x + content.width - CHAT_SEND_W, input_y, CHAT_SEND_W, row_h };
    l.input = (Rectangle){ content.x, input_y, content.width - CHAT_SEND_W - CHAT_GAP, row_h };
    l.list  = (Rectangle){ content.x, content.y, content.width, input_y - CHAT_GAP - content.y };
    if (0.0f > l.list.height) l.list.height = 0.0f;
    return l;
}

/* Trim and push one line. The history shows it once the server echoes it. */
static void send_line(const ChatPane* pane, const char* text) {
    char line[CHAT_LINE_BYTES];
    copy_str(line, sizeof(line), text);
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

static void send_input(const ChatPane* pane) {
    char text[CHAT_LINE_BYTES];
    text_input_bridge_take(text, sizeof(text));
    send_line(pane, text);
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
    double ts    = 0 < count ? h->lines[count - 1].ts_ms : 0.0;
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
        identity_icon_draw(m->from_skin, (Rectangle){ l.list.x, y, face, face });
        DrawText(m->mine ? "You" : m->from_name, (int)(l.list.x + face + CHAT_GAP), (int)y, font,
                 m->mine ? C_MINE : C_THEIRS);
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
    for (int i = 0; CHAT_PRESET_COUNT > i; i++) {
        UIButtonPixelRetroStyle preset = {
            .bg = C_BTN, .label = CHAT_PRESETS[i], .font_size = font, .text_color = MODAL_TEXT,
            .enabled = true,
        };
        ui_button_pixel_retro_draw(l.presets[i], &preset, ui_button_hit(l.presets[i], mx, my));
    }
    for (int i = 0; CYBERIA_CHAT_EMOJI_COUNT > i; i++) {
        char icon[40];
        snprintf(icon, sizeof(icon), "emoji-%s", CYBERIA_CHAT_EMOJI[i]);
        UIButtonPixelRetroStyle emoji = { .bg = C_BTN, .icon_id = icon, .enabled = true };
        ui_button_pixel_retro_draw(l.emoji[i], &emoji, ui_button_hit(l.emoji[i], mx, my));
    }
}

void chat_pane_press(ChatPane* pane, Rectangle content, int mx, int my) {
    assert(pane);
    ChatLayout l = chat_layout(content);
    if (ui_button_hit(l.send, mx, my)) {
        send_input(pane);
        return;
    }
    for (int i = 0; CHAT_PRESET_COUNT > i; i++) {
        if (ui_button_hit(l.presets[i], mx, my)) {
            send_line(pane, CHAT_PRESETS[i]);
            return;
        }
    }
    for (int i = 0; CYBERIA_CHAT_EMOJI_COUNT > i; i++) {
        if (ui_button_hit(l.emoji[i], mx, my)) {
            char token[40];
            snprintf(token, sizeof(token), ":%s:", CYBERIA_CHAT_EMOJI[i]);
            send_line(pane, token);
            return;
        }
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
    if (s_input_owner != pane) return;
    text_input_bridge_hide();
    s_input_owner = NULL;
}
