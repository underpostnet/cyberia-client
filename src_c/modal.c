#include "modal.h"
#include <stdio.h>
#include <string.h>

int modal_init_struct(Modal* modal) {
    if (!modal) return -1;
    
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
    if (!modal) return;
    
    modal->line_count = 0;
    for (int i = 0; i < MODAL_MAX_LINES; i++) {
        modal->lines[i].text[0] = '\0';
        modal->lines[i].visible = true;
    }
}

int modal_add_line(Modal* modal, const char* text, Color color) {
    if (!modal || !text) return -1;
    if (modal->line_count >= MODAL_MAX_LINES) return -1;
    
    strncpy(modal->lines[modal->line_count].text, text, MODAL_MAX_LINE_LENGTH - 1);
    modal->lines[modal->line_count].text[MODAL_MAX_LINE_LENGTH - 1] = '\0';
    modal->lines[modal->line_count].color = color;
    modal->lines[modal->line_count].visible = true;
    modal->line_count++;
    
    return 0;
}

int modal_set_line(Modal* modal, int line_index, const char* text, Color color) {
    if (!modal || !text) return -1;
    if (line_index < 0 || line_index >= MODAL_MAX_LINES) return -1;
    
    strncpy(modal->lines[line_index].text, text, MODAL_MAX_LINE_LENGTH - 1);
    modal->lines[line_index].text[MODAL_MAX_LINE_LENGTH - 1] = '\0';
    modal->lines[line_index].color = color;
    modal->lines[line_index].visible = true;
    
    if (line_index >= modal->line_count) {
        modal->line_count = line_index + 1;
    }
    
    return 0;
}

void modal_update_struct(Modal* modal, float delta_time) {
    if (!modal) return;
    
    // Optional: Add fade in/out animations here if needed
    (void)delta_time; // Unused for now
}

void modal_draw_struct(const Modal* modal, int screen_width, int screen_height) {
    if (!modal || !modal->visible) return;
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

void modal_set_visible(Modal* modal, bool visible) {
    if (!modal) return;
    modal->visible = visible;
}

void modal_set_style(Modal* modal, Color bg_color, Color border_color, float alpha) {
    if (!modal) return;
    
    modal->background_color = bg_color;
    modal->border_color = border_color;
    
    if (alpha < 0.0f) alpha = 0.0f;
    if (alpha > 1.0f) alpha = 1.0f;
    modal->background_alpha = alpha;
}

void modal_set_dimensions(Modal* modal, int min_width, int min_height) {
    if (!modal) return;
    
    modal->min_width = min_width;
    modal->min_height = min_height;
}

void modal_set_position(Modal* modal, int position_mode, 
                       int margin_top, int margin_right, 
                       int margin_bottom, int margin_left) {
    if (!modal) return;
    
    modal->position_mode = position_mode;
    modal->margin_top = margin_top;
    modal->margin_right = margin_right;
    modal->margin_bottom = margin_bottom;
    modal->margin_left = margin_left;
}

void modal_set_custom_position(Modal* modal, int x, int y) {
    if (!modal) return;
    
    modal->position_mode = MODAL_POS_CUSTOM;
    modal->custom_x = x;
    modal->custom_y = y;
}

void modal_set_text_alignment(Modal* modal, int align) {
    if (!modal) return;
    modal->text_align = align;
}

void modal_set_font(Modal* modal, int font_size, int line_spacing) {
    if (!modal) return;
    
    modal->font_size = font_size;
    modal->line_spacing = line_spacing;
}