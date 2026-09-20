#include "modal.h"
#include "ease.h"
#include "text.h"
#include "toolbar.h"
#include <string.h>

/* ── Shared panel chrome ──────────────────────────────────────────────── */

const Color MODAL_OVERLAY_BG   = {  0,  0,  0, 170 };
const Color MODAL_PANEL_BG     = { 14, 14, 22, 245 };
const Color MODAL_PANEL_BORDER = { 80, 80, 130, 220 };

float modal_pop_scale(float age) {
    if (age >= MODAL_POP_DURATION) return 1.0f;
    float t = age / MODAL_POP_DURATION;
    return 0.80f + 0.20f * ease_out_cubic(t);
}

float modal_pop_alpha(float age) {
    float t = age / MODAL_POP_DURATION;
    if (t > 1.0f) t = 1.0f;
    if (t < 0.0f) t = 0.0f;
    return t;
}

void modal_draw_float_shadow(Rectangle rect, float age) {
    float a = modal_pop_alpha(age);
    for (int i = MODAL_SHADOW_LAYERS; i >= 1; i--) {
        float grow = (float)i * 3.0f;
        Color c = { 0, 0, 0, (unsigned char)(24.0f * a) };
        DrawRectangleRec((Rectangle){ rect.x - grow, rect.y - grow + 3.0f,
                                      rect.width + 2.0f * grow,
                                      rect.height + 2.0f * grow }, c);
    }
}

void modal_draw_clipped_text(const char* text, int x, int y, int max_w,
                             int font_size, Color color) {
    if (NULL == text || '\0' == text[0] || max_w <= 0) return;
    if (MeasureText(text, font_size) <= max_w) {
        DrawText(text, x, y, font_size, color);
        return;
    }

    char buf[160];
    size_t n = strlen(text);
    if (n > sizeof(buf) - 4) n = sizeof(buf) - 4;
    while (n > 0) {
        n = (size_t)text_utf8_floor(text, (int)n - 1);
        memcpy(buf, text, n);
        memcpy(buf + n, "...", 4);
        if (MeasureText(buf, font_size) <= max_w) {
            DrawText(buf, x, y, font_size, color);
            return;
        }
    }
}

bool modal_wide_layout(void) {
    int sw = GetScreenWidth();
    int sh = GetScreenHeight();
    return (float)sw > (float)sh * 1.4f;
}


void modal_draw_card(Rectangle card, bool anchored, float age, float overlay_h,
                     Color border, float header_h) {
    float alpha = modal_pop_alpha(age);
    if (anchored) {
        modal_draw_float_shadow(card, age);
    } else {
        Color overlay = MODAL_OVERLAY_BG;
        overlay.a = (unsigned char)(overlay.a * alpha);
        DrawRectangle(0, 0, GetScreenWidth(), (int)overlay_h, overlay);
    }
    Color bg = MODAL_PANEL_BG;
    bg.a = (unsigned char)((anchored ? MODAL_ANCHOR_PANEL_ALPHA : 150.0f) * alpha);
    DrawRectangleRec(card, bg);
    Color bc = border;
    bc.a = (unsigned char)(bc.a * alpha);
    DrawRectangleLinesEx(card, 1.0f, bc);
    DrawRectangle((int)card.x, (int)card.y, (int)card.width, (int)header_h,
                  (Color){ border.r, border.g, border.b, 40 });
}

void modal_draw_title(const char* text, Rectangle card, float pad, float header_h,
                      int font, Color color, float close_x) {
    float tx = card.x + pad;
    if (tx < toolbar_toggle_right()) tx = toolbar_toggle_right();
    modal_draw_clipped_text(text, (int)tx, (int)(card.y + (header_h - font) * 0.5f),
                            (int)(close_x - MODAL_HEADER_TITLE_GAP - tx), font, color);
}
