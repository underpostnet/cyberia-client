#include "modal.h"
#include "text.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

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

void modal_draw_panel(Rectangle rect, float age) {
    modal_draw_panel_ex(rect, age, MODAL_PANEL_BORDER, 1.5f);
}

bool modal_wide_layout(void) {
    int sw = GetScreenWidth();
    int sh = GetScreenHeight();
    return (float)sw > (float)sh * 1.4f;
}

int modal_init_struct(Modal* modal) {
    assert(modal);

    memset(modal, 0, sizeof(Modal));

    // Set default dimensions
    modal->min_width = 200;
    modal->min_height = 100;
    modal->padding = 15;
    modal->margin_top = 10;
    modal->margin_right = 10;
    modal->margin_bottom = 10;
    modal->margin_left = 10;

    // Set default position mode
    modal->position_mode = MODAL_POS_TOP_RIGHT;
    modal->custom_x = 0;
    modal->custom_y = 0;

    // Set default colors
    modal->background_color = (Color){0, 0, 0, 200};
    modal->border_color = (Color){100, 100, 100, 200};
    modal->shadow_color = (Color){0, 0, 0, 180};
    modal->background_alpha = 0.78f;
    modal->border_width = 1.0f;
    modal->draw_shadow = true;
    modal->draw_border = true;

    // Font settings
    modal->font_size = 16;
    modal->line_spacing = 22;
    modal->text_align = MODAL_ALIGN_CENTER;

    // Visibility
    modal->visible = true;

    // Animation
    modal->fade_alpha = 1.0f;
    modal->fade_in = true;

    // Clear lines
    modal->line_count = 0;
    for (int i = 0; i < MODAL_MAX_LINES; i++) {
        modal->lines[i].text[0] = '\0';
        modal->lines[i].color = WHITE;
        modal->lines[i].visible = true;
    }

    return 0;
}

void modal_clear_lines(Modal* modal) {
    assert(modal);

    modal->line_count = 0;
    for (int i = 0; i < MODAL_MAX_LINES; i++) {
        modal->lines[i].text[0] = '\0';
        modal->lines[i].visible = true;
    }
}

void modal_draw_struct(const Modal* modal, int screen_width, int screen_height) {
    assert(modal);
    if (!modal->visible) return;
    if (modal->line_count == 0) return;

    // Calculate content dimensions
    int max_text_width = 0;
    for (int i = 0; i < modal->line_count; i++) {
        if (!modal->lines[i].visible) continue;
        int text_width = MeasureText(modal->lines[i].text, modal->font_size);
        if (text_width > max_text_width) {
            max_text_width = text_width;
        }
    }

    // Calculate modal dimensions
    int modal_width = max_text_width + (modal->padding * 2);
    if (modal_width < modal->min_width) {
        modal_width = modal->min_width;
    }

    int modal_height = (modal->line_spacing * modal->line_count) + (modal->padding * 2);
    if (modal_height < modal->min_height) {
        modal_height = modal->min_height;
    }

    // Calculate modal position based on position mode
    int modal_x = 0;
    int modal_y = 0;

    switch (modal->position_mode) {
        case MODAL_POS_TOP_LEFT:
            modal_x = modal->margin_left;
            modal_y = modal->margin_top;
            break;
        case MODAL_POS_TOP_RIGHT:
            modal_x = screen_width - modal_width - modal->margin_right;
            modal_y = modal->margin_top;
            break;
        case MODAL_POS_BOTTOM_LEFT:
            modal_x = modal->margin_left;
            modal_y = screen_height - modal_height - modal->margin_bottom;
            break;
        case MODAL_POS_BOTTOM_RIGHT:
            modal_x = screen_width - modal_width - modal->margin_right;
            modal_y = screen_height - modal_height - modal->margin_bottom;
            break;
        case MODAL_POS_CENTER:
            modal_x = (screen_width - modal_width) / 2;
            modal_y = (screen_height - modal_height) / 2;
            break;
        case MODAL_POS_CUSTOM:
            modal_x = modal->custom_x;
            modal_y = modal->custom_y;
            break;
    }

    // Draw modal background
    Rectangle modal_rect = {
        (float)modal_x,
        (float)modal_y,
        (float)modal_width,
        (float)modal_height
    };

    Color bg_color = modal->background_color;
    bg_color.a = (unsigned char)(255 * modal->background_alpha * modal->fade_alpha);
    DrawRectangleRec(modal_rect, bg_color);

    // Draw border
    if (modal->draw_border) {
        Color border_color = modal->border_color;
        border_color.a = (unsigned char)(border_color.a * modal->fade_alpha);
        DrawRectangleLinesEx(modal_rect, modal->border_width, border_color);
    }

    // Draw text lines
    int text_start_y = modal_y + modal->padding;

    for (int i = 0; i < modal->line_count; i++) {
        if (!modal->lines[i].visible) continue;

        const char* text = modal->lines[i].text;
        Color text_color = modal->lines[i].color;
        text_color.a = (unsigned char)(text_color.a * modal->fade_alpha);

        int text_width = MeasureText(text, modal->font_size);
        int text_x = modal_x + modal->padding;

        // Apply text alignment
        switch (modal->text_align) {
            case MODAL_ALIGN_CENTER:
                text_x = modal_x + (modal_width - text_width) / 2;
                break;
            case MODAL_ALIGN_RIGHT:
                text_x = modal_x + modal_width - text_width - modal->padding;
                break;
            case MODAL_ALIGN_LEFT:
            default:
                text_x = modal_x + modal->padding;
                break;
        }

        int text_y = text_start_y + (i * modal->line_spacing);

        // Draw shadow if enabled
        if (modal->draw_shadow) {
            Color shadow_color = modal->shadow_color;
            shadow_color.a = (unsigned char)(shadow_color.a * modal->fade_alpha);
            DrawText(text, text_x + 1, text_y + 1, modal->font_size, shadow_color);
        }

        // Draw text
        DrawText(text, text_x, text_y, modal->font_size, text_color);
    }
}

