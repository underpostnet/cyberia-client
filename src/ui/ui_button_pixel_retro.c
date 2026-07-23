#include "ui_button_pixel_retro.h"

#include "text.h"
#include "ui_icon.h"

#include <raylib.h>

static unsigned char clamp_u8(float v) {
    if (v < 0.0f) return 0;
    if (v > 255.0f) return 255;
    return (unsigned char)(v + 0.5f);
}

/* Move a colour toward white by `amt` (0..1). */
static Color pixel_lighten(Color c, float amt) {
    return (Color){ clamp_u8(c.r + (255 - c.r) * amt),
                    clamp_u8(c.g + (255 - c.g) * amt),
                    clamp_u8(c.b + (255 - c.b) * amt), c.a };
}

/* Move a colour toward black by `amt` (0..1). */
static Color pixel_darken(Color c, float amt) {
    return (Color){ clamp_u8(c.r * (1.0f - amt)),
                    clamp_u8(c.g * (1.0f - amt)),
                    clamp_u8(c.b * (1.0f - amt)), c.a };
}

static void draw_outlined(const char* text, int x, int y, int font, Color col) {
    for (int oy = -1; oy <= 1; oy++)
        for (int ox = -1; ox <= 1; ox++)
            if (ox || oy) DrawText(text, x + ox, y + oy, font, BLACK);
    DrawText(text, x, y, font, col);
}

static void draw_outlined_wrap(const char* text, int x, int y, int maxw, int font, Color col) {
    for (int oy = -1; oy <= 1; oy++)
        for (int ox = -1; ox <= 1; ox++)
            if (ox || oy) text_wrap(text, x + ox, y + oy, maxw, font, BLACK, false, true);
    text_wrap(text, x, y, maxw, font, col, false, true);
}

void ui_button_pixel_retro_draw(Rectangle r, const UIButtonPixelRetroStyle* style, bool hovered) {
    bool active_hover = hovered && style->enabled;
    Color base = style->bg;
    Color fill = active_hover ? pixel_lighten(base, 0.14f) : base;
    Color highlight = pixel_lighten(base, 0.45f);
    Color shadow = pixel_darken(base, 0.45f);
    Color text_col = style->text_color.a == 0 ? WHITE : style->text_color;

    Rectangle inner = { r.x + 2.0f, r.y + 2.0f, r.width - 4.0f, r.height - 4.0f };

    DrawRectangleRounded(r, 0.18f, 6, BLACK);
    DrawRectangleRounded(inner, 0.18f, 6, fill);
    DrawRectangle((int)(inner.x + 4.0f), (int)inner.y, (int)(inner.width - 8.0f), 2, highlight);
    DrawRectangle((int)(inner.x + 4.0f), (int)(inner.y + inner.height - 2.0f),
                  (int)(inner.width - 8.0f), 2, shadow);
    if (style->selected || active_hover) DrawRectangleRoundedLinesEx(inner, 0.18f, 6, 1.0f, WHITE);

    bool has_icon  = style->icon_id && '\0' != style->icon_id[0];
    bool has_label = style->label && '\0' != style->label[0];

    if (style->wrap_label) {
        float icon_sz = r.height - 12.0f;
        if (has_icon) {
            float cx = r.x + 8.0f + icon_sz * 0.5f;
            float cy = r.y + r.height * 0.5f;
            ui_icon_draw_ex(style->icon_id, cx + 1.0f, cy + 1.0f, icon_sz, 0.0f, BLACK);
            ui_icon_draw_ex(style->icon_id, cx, cy, icon_sz, 0.0f, text_col);
        }
        if (has_label) {
            int tx = (int)(r.x + (has_icon ? 12.0f + icon_sz + 6.0f : 10.0f));
            int tw = (int)(r.x + r.width - 8.0f) - tx;
            if (tw < 20) tw = 20;
            int th = text_wrap(style->label, tx, 0, tw, style->font_size, WHITE, false, false);
            int ty = (int)(r.y + (r.height - th) * 0.5f);
            draw_outlined_wrap(style->label, tx, ty, tw, style->font_size, text_col);
        }
        return;
    }

    /* Centered group: icon + single line, sized to fit and centered. */
    float icon_sz = (float)style->font_size + 6.0f;
    if (icon_sz > r.height - 8.0f) icon_sz = r.height - 8.0f;
    float gap = has_icon && has_label ? 6.0f : 0.0f;
    int   label_w = has_label ? MeasureText(style->label, style->font_size) : 0;
    float group_w = (has_icon ? icon_sz : 0.0f) + gap + (float)label_w;
    float gx = r.x + (r.width - group_w) * 0.5f;
    float cy = r.y + r.height * 0.5f;

    if (has_icon) {
        ui_icon_draw_ex(style->icon_id, gx + icon_sz * 0.5f + 1.0f, cy + 1.0f, icon_sz, 0.0f, BLACK);
        ui_icon_draw_ex(style->icon_id, gx + icon_sz * 0.5f, cy, icon_sz, 0.0f, text_col);
        gx += icon_sz + gap;
    }
    if (has_label) {
        draw_outlined(style->label, (int)gx, (int)(cy - style->font_size * 0.5f),
                      style->font_size, text_col);
    }
}
