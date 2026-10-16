#include "emoji.h"

#include "content/stat_contract_generated.h"
#include "text.h"
#include "ui_icon.h"

#include <limits.h>
#include <stdio.h>
#include <string.h>

const char* emoji_icon_of(const char* word, size_t length, char* out, size_t out_size) {
    if (3 > length || ':' != word[0] || ':' != word[length - 1]) return NULL;
    for (int i = 0; CYBERIA_CHAT_EMOJI_COUNT > i; i++) {
        const char* name = CYBERIA_CHAT_EMOJI[i];
        if (strlen(name) == length - 2 && 0 == strncmp(name, word + 1, length - 2)) {
            snprintf(out, out_size, "emoji-%s", name);
            return out;
        }
    }
    return NULL;
}

/* An emoji is a little taller than the text, and square. */
static int emoji_px(int size) { return size + size / 3; }

/* Lays out the words of `text` within `maxw`; returns the height used and
 * stores the widest line in `width`. */
static int layout(const char* text, int x, int y, int maxw, int size, Color color, bool draw, int* width) {
    *width = 0;
    if (NULL == text || '\0' == text[0]) return 0;
    const int line_h = text_line_height(size) > emoji_px(size) ? text_line_height(size) : emoji_px(size);
    const int space = text_measure_compat(" ", size);
    int cx = 0;
    int cy = y;
    const char* p = text;
    while ('\0' != *p) {
        while (' ' == *p) p++;
        size_t length = strcspn(p, " ");
        if (0 == length) break;
        char word[256];
        snprintf(word, sizeof(word), "%.*s", (int)length, p);
        char icon[40];
        bool emoji = NULL != emoji_icon_of(word, length, icon, sizeof(icon));
        int w = emoji ? emoji_px(size) : text_measure_compat(word, size);
        if (0 < cx && cx + space + w > maxw) {
            cx = 0;
            cy += line_h;
        } else if (0 < cx) {
            cx += space;
        }
        if (draw) {
            if (emoji) {
                ui_icon_draw(icon, (float)(x + cx) + (float)w * 0.5f, (float)cy + (float)line_h * 0.5f, w, false, 0.0f);
            } else {
                text_draw_compat(word, x + cx, cy + (line_h - size) / 2, size, color);
            }
        }
        cx += w;
        if (cx > *width) *width = cx;
        p += length;
    }
    return cy + line_h - y;
}

int emoji_text_wrap(const char* text, int x, int y, int maxw, int size, Color color, bool draw) {
    int width;
    return layout(text, x, y, maxw, size, color, draw, &width);
}

int emoji_text_width(const char* text, int size) {
    int width;
    layout(text, 0, 0, INT_MAX, size, BLANK, false, &width);
    return width;
}
