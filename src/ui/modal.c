#include "modal.h"
#include "text.h"
#include <math.h>
#include <string.h>

/* ── Shared panel chrome ──────────────────────────────────────────────── */

const Color MODAL_OVERLAY_BG   = {  0,  0,  0, 170 };
const Color MODAL_PANEL_BG     = { 14, 14, 22, 245 };
const Color MODAL_PANEL_BORDER = { 80, 80, 130, 220 };

float modal_pop_scale(float age) {
    if (age >= MODAL_POP_DURATION) return 1.0f;
    float t = age / MODAL_POP_DURATION;
    return 0.80f + 0.20f * (1.0f - powf(1.0f - t, 3.0f));
}

float modal_pop_alpha(float age) {
    float t = age / MODAL_POP_DURATION;
    if (t > 1.0f) t = 1.0f;
    if (t < 0.0f) t = 0.0f;
    return t;
}

Rectangle modal_scale_rect(Rectangle rect, float scale) {
    float cx = rect.x + rect.width * 0.5f;
    float cy = rect.y + rect.height * 0.5f;
    float w  = rect.width * scale;
    float h  = rect.height * scale;
    return (Rectangle){ cx - w * 0.5f, cy - h * 0.5f, w, h };
}

void modal_draw_overlay(int screen_width, int screen_height, float age) {
    Color c = MODAL_OVERLAY_BG;
    c.a = (unsigned char)(c.a * modal_pop_alpha(age));
    DrawRectangle(0, 0, screen_width, screen_height, c);
}

void modal_draw_panel_ex(Rectangle rect, float age, Color border, float border_width) {
    float a = modal_pop_alpha(age);
    Color bg = MODAL_PANEL_BG;
    bg.a = (unsigned char)(bg.a * a);
    DrawRectangleRec(rect, bg);
    Color bc = border;
    bc.a = (unsigned char)(bc.a * a);
    DrawRectangleLinesEx(rect, border_width, bc);
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
        n--;
        /* Land on a UTF-8 lead byte so a cut never splits a codepoint. */
        while (n > 0 && 0x80 == ((unsigned char)text[n] & 0xC0)) n--;
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

