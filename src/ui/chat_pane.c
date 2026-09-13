#include "chat_pane.h"
#include "text.h"

#include "domain/viewport.h"
#include "js/text_input_bridge.h"
#include "network/game/game_client.h"
#include "notification.h"
#include "notify_store.h"
#include "ui_button.h"
#include "ui_scroll.h"
#include "util/utils.h"

#include <assert.h>
#include <ctype.h>
#include <raylib.h>
#include <stdbool.h>
#include <string.h>

#define CHAT_TEXT_MAX     128
#define CHAT_LINE_BYTES   512
#define CHAT_DRAW_MAX     50
#define CHAT_GAP          6.0f
#define CHAT_SEND_W       76.0f
#define CHAT_PRESET_COUNT 6

static const char* CHAT_PRESETS[CHAT_PRESET_COUNT] = {
    "Hello!", "GG", "Help!", "Trade?", "Follow me", "Thanks!",
};

static const Color C_MINE     = { 120, 180, 240, 255 };
static const Color C_THEIRS   = { 200, 200, 220, 230 };
static const Color C_TEXT     = { 220, 220, 230, 240 };
static const Color C_HINT     = { 130, 140, 165, 200 };
static const Color C_BTN      = {  24,  30,  48, 255 };
static const Color C_SEND     = {  35,  80, 160, 255 };
static const Color C_INPUT_BG = {  18,  18,  36, 225 };

typedef struct {
    Rectangle list;
    Rectangle input;
    Rectangle send;
    Rectangle presets[CHAT_PRESET_COUNT];
} ChatLayout;

static char      s_entity_id[64];
static UIScroll  s_scroll;
static float     s_content_h;
static int       s_seen_count;
static double    s_seen_ts;
static bool      s_stick;
static bool      s_input_shown;
static Rectangle s_input_rect;

static inline int   chat_font(void)  { return viewport_is_mobile() ? 12 : 14; }
static inline float chat_row_h(void) { return viewport_is_mobile() ? 30.0f : 34.0f; }

/* Presets wrap along the bottom edge, the input row sits above them, and the
 * history takes the rest. */
static ChatLayout chat_layout(Rectangle content) {
    ChatLayout l = { 0 };
    int   font  = chat_font();
    float row_h = chat_row_h();
    float x = 0.0f;
    float y = 0.0f;
    for (int i = 0; CHAT_PRESET_COUNT > i; i++) {
        float w = (float)MeasureText(CHAT_PRESETS[i], font) + 2.0f * UI_BUTTON_DEFAULT_PAD;
        if (w > content.width) w = content.width;
        if (0.0f < x && x + w > content.width) {
            x = 0.0f;
            y += row_h + CHAT_GAP;
        }
        l.presets[i] = (Rectangle){ content.x + x, y, w, row_h };
        x += w + CHAT_GAP;
    }
    float presets_y = content.y + content.height - (y + row_h);
    for (int i = 0; CHAT_PRESET_COUNT > i; i++) l.presets[i].y += presets_y;

    float input_y = presets_y - CHAT_GAP - row_h;
    l.send  = (Rectangle){ content.x + content.width - CHAT_SEND_W, input_y, CHAT_SEND_W, row_h };
    l.input = (Rectangle){ content.x, input_y, content.width - CHAT_SEND_W - CHAT_GAP, row_h };
    l.list  = (Rectangle){ content.x, content.y, content.width, input_y - CHAT_GAP - content.y };
    if (0.0f > l.list.height) l.list.height = 0.0f;
    return l;
}

/* Trim and send one line. The history keeps only lines the socket accepted. */
static void send_line(const char* entity_id, const char* text) {
    char line[CHAT_LINE_BYTES];
    copy_str(line, sizeof(line), text);
    char* start = line;
    while (isspace((unsigned char)*start)) start++;
    size_t length = strlen(start);
    while (0 < length && isspace((unsigned char)start[length - 1])) start[--length] = '\0';
    if (0 == length) return;
    if (network_send_chat(entity_id, start)) notify_store_push(entity_id, "You", start, true);
}

static void send_input(const char* entity_id) {
    char text[CHAT_LINE_BYTES];
    text_input_bridge_take(text, sizeof(text));
    send_line(entity_id, text);
}

static bool same_pixels(Rectangle a, Rectangle b) {
    return (int)a.x == (int)b.x && (int)a.y == (int)b.y &&
           (int)a.width == (int)b.width && (int)a.height == (int)b.height;
}

void chat_pane_update(Rectangle content, const char* entity_id, bool input_visible, float dt) {
    assert(entity_id);
    ChatLayout l = chat_layout(content);

    if (0 != strcmp(s_entity_id, entity_id)) {
        /* A draft belongs to the entity it was typed for. */
        char draft[CHAT_LINE_BYTES];
        text_input_bridge_take(draft, sizeof(draft));
        copy_str(s_entity_id, sizeof(s_entity_id), entity_id);
        ui_scroll_reset(&s_scroll);
        s_content_h  = 0.0f;
        s_seen_count = -1;
    }

    /* The pane shows every line, so none stays unread. */
    notification_clear(NOTIF_CHAT, entity_id);

    const NotifyEntry* entry = notify_store_get(entity_id);
    int    count = NULL != entry ? entry->count : 0;
    double ts    = 0 < count ? entry->messages[count - 1].ts_ms : 0.0;
    if (count != s_seen_count || ts != s_seen_ts || l.list.height != s_scroll.view.height) {
        s_stick = true;
        s_seen_count = count;
        s_seen_ts = ts;
    }

    ui_scroll_update(&s_scroll, l.list, s_content_h, dt);

    if (!input_visible) {
        chat_pane_hide();
        return;
    }
    if (!s_input_shown || !same_pixels(s_input_rect, l.input)) {
        if (!s_input_shown) s_stick = true;
        text_input_bridge_show((int)l.input.x, (int)l.input.y, (int)l.input.width,
                               (int)l.input.height, CHAT_TEXT_MAX, "Say something...");
        s_input_shown = true;
        s_input_rect = l.input;
    }
    if (text_input_bridge_submitted()) send_input(entity_id);
}

void chat_pane_draw(Rectangle content, const char* entity_id, const char* entity_name) {
    assert(entity_id && entity_name);
    ChatLayout l = chat_layout(content);
    int font = chat_font();
    int mx = GetMouseX();
    int my = GetMouseY();
    const char* their_label = '\0' != entity_name[0] ? entity_name : entity_id;

    const NotifyEntry* entry = notify_store_get(entity_id);
    int count = NULL != entry ? entry->count : 0;

    ui_scroll_begin(&s_scroll);
    float top = l.list.y - ui_scroll_offset(&s_scroll);
    float y = top;
    if (0 == count) {
        DrawText("No messages yet...", (int)l.list.x, (int)y, font, C_HINT);
        y += (float)text_line_height(font);
    }
    /* ponytail: wraps the newest CHAT_DRAW_MAX lines every frame; cache the
     * wrapped heights if a long history shows up in a profile. */
    for (int i = CHAT_DRAW_MAX < count ? count - CHAT_DRAW_MAX : 0; count > i; i++) {
        const NotifyMessage* m = &entry->messages[i];
        DrawText(m->mine ? "You" : their_label, (int)l.list.x, (int)y, font,
                 m->mine ? C_MINE : C_THEIRS);
        y += (float)text_line_height(font);
        y += (float)text_wrap(m->text, (int)l.list.x, (int)y, (int)l.list.width, font,
                              C_TEXT, false, true) + CHAT_GAP;
    }
    s_content_h = y - top;
    ui_scroll_end(&s_scroll);
    if (s_stick) {
        /* The next update clamps the offset to the scroll range. */
        s_scroll.offset = s_content_h - l.list.height;
        s_stick = false;
    }

    /* The DOM input covers this frame while it shows. */
    DrawRectangleRec(l.input, C_INPUT_BG);

    UIButtonPixelRetroStyle send = {
        .bg = C_SEND, .label = "Send", .font_size = font, .text_color = C_TEXT, .enabled = true,
    };
    ui_button_pixel_retro_draw(l.send, &send, ui_button_hit(l.send, mx, my));
    for (int i = 0; CHAT_PRESET_COUNT > i; i++) {
        UIButtonPixelRetroStyle preset = {
            .bg = C_BTN, .label = CHAT_PRESETS[i], .font_size = font, .text_color = C_TEXT,
            .enabled = true,
        };
        ui_button_pixel_retro_draw(l.presets[i], &preset, ui_button_hit(l.presets[i], mx, my));
    }
}

void chat_pane_press(Rectangle content, const char* entity_id, int mx, int my) {
    assert(entity_id);
    ChatLayout l = chat_layout(content);
    if (ui_button_hit(l.send, mx, my)) {
        send_input(entity_id);
        return;
    }
    for (int i = 0; CHAT_PRESET_COUNT > i; i++) {
        if (ui_button_hit(l.presets[i], mx, my)) {
            send_line(entity_id, CHAT_PRESETS[i]);
            return;
        }
    }
    if (CheckCollisionPointRec((Vector2){ (float)mx, (float)my }, l.list)) {
        ui_scroll_on_press(&s_scroll, mx, my);
    }
}

bool chat_pane_wheel(Rectangle content, float wheel_delta) {
    return ui_scroll_on_wheel(&s_scroll, chat_layout(content).list, s_content_h, wheel_delta);
}

void chat_pane_hide(void) {
    if (!s_input_shown) return;
    text_input_bridge_hide();
    s_input_shown = false;
}
