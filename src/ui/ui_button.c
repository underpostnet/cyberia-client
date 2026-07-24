#include "ui_button.h"
#include "text.h"
#include "ui_icon.h"

#include <stddef.h>

/* Toggle-derived baseline chrome. */
static const Color UB_BG          = {  20,  24,  40, 220 };
static const Color UB_BG_HOVER    = {  44,  50,  74, 230 };
static const Color UB_BG_SELECTED = {  40,  60, 100, 235 };
static const Color UB_BG_DISABLED = {  18,  18,  26, 200 };
static const Color UB_BORDER      = {  80, 160, 220, 240 };
static const Color UB_TEXT        = { 200, 210, 230, 240 };
static const Color UB_TEXT_DIS    = {  90,  95, 110, 150 };

static bool has_str(const char* s) { return NULL != s && '\0' != s[0]; }

static Color or_default(Color c, Color fallback) {
    return 0 == c.a ? fallback : c;
}

static int resolve_font(const UIButtonStyle* s) {
    return s->font_size > 0 ? s->font_size : UI_BUTTON_DEFAULT_FONT;
}

static float resolve_pad(const UIButtonStyle* s) {
    return s->padding > 0.0f ? s->padding : UI_BUTTON_DEFAULT_PAD;
}

static float resolve_gap(const UIButtonStyle* s) {
    return s->gap > 0.0f ? s->gap : UI_BUTTON_DEFAULT_GAP;
}

/* Icon edge length. Bounds == NULL → measure context (no geometry yet). */
static int resolve_icon_size(const UIButtonStyle* s, const Rectangle* bounds) {
    if (s->icon_size > 0) return s->icon_size;
    int font = resolve_font(s);
    if (has_str(s->text)) return font + 2;
    if (bounds) {
        float m = bounds->width < bounds->height ? bounds->width : bounds->height;
        int sz = (int)(m * 0.75f);
        return sz < 6 ? 6 : sz;
    }
    return font + 6;
}

Vector2 ui_button_measure(const UIButtonStyle* style) {
    int   font   = resolve_font(style);
    float pad    = resolve_pad(style);
    bool  ic     = has_str(style->icon_id);
    bool  tx     = has_str(style->text);

    int icon_sz  = ic ? resolve_icon_size(style, NULL) : 0;
    int text_w   = tx ? MeasureText(style->text, font) : 0;
    float gap    = (ic && tx) ? resolve_gap(style) : 0.0f;

    float content_w = (float)icon_sz + gap + (float)text_w;
    float content_h = (float)(icon_sz > font ? icon_sz : font);
    return (Vector2){ content_w + pad * 2.0f, content_h + pad * 2.0f };
}

UIButtonState ui_button_resolve_state(bool enabled, bool selected, bool hovered) {
    if (!enabled) return UI_BUTTON_DISABLED;
    if (selected) return UI_BUTTON_SELECTED;
    if (hovered)  return UI_BUTTON_HOVERED;
    return UI_BUTTON_NORMAL;
}

bool ui_button_hit(Rectangle b, int mx, int my) {
    return ((float)mx >= b.x && (float)mx < b.x + b.width &&
            (float)my >= b.y && (float)my < b.y + b.height);
}

static Color state_bg(const UIButtonStyle* s, UIButtonState st) {
    switch (st) {
        case UI_BUTTON_HOVERED:  return or_default(s->bg_hover,    UB_BG_HOVER);
        case UI_BUTTON_SELECTED: return or_default(s->bg_selected, UB_BG_SELECTED);
        case UI_BUTTON_DISABLED: return or_default(s->bg_disabled, UB_BG_DISABLED);
        case UI_BUTTON_NORMAL:
        default:                 return or_default(s->bg,          UB_BG);
    }
}

static Color state_text(const UIButtonStyle* s, UIButtonState st) {
    Color base = or_default(s->text_color, UB_TEXT);
    switch (st) {
        case UI_BUTTON_SELECTED: return or_default(s->text_selected, base);
        case UI_BUTTON_DISABLED: return or_default(s->text_disabled, UB_TEXT_DIS);
        default:                 return base;
    }
}

/* ── Pixel-retro helpers ─────────────────────────────────────────────── */

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

/* ── Pixel-retro draw (public, used by modal_interact etc.) ──────────── */

void ui_button_pixel_retro_draw(Rectangle r, const UIButtonPixelRetroStyle* style, bool hovered) {
    bool active_hover = hovered && style->enabled;
    Color base = style->bg;
    Color fill = active_hover ? pixel_lighten(base, 0.14f) : base;
    Color highlight = pixel_lighten(base, 0.45f);
    Color shadow = pixel_darken(base, 0.45f);
    Color text_col = style->text_color.a == 0 ? WHITE : style->text_color;

    Rectangle inner = { r.x + 2.0f, r.y + 2.0f, r.width - 4.0f, r.height - 4.0f };

    if (style->flat) {
        /* Clean icon button: no fill, no black border, no bevel edges.
         * Only the icon is drawn, with an optional hover/selected outline. */
    } else {
        DrawRectangleRounded(inner, 0.18f, 6, fill);
        DrawRectangle((int)(inner.x + 4.0f), (int)inner.y, (int)(inner.width - 8.0f), 2, highlight);
        DrawRectangle((int)(inner.x + 4.0f), (int)(inner.y + inner.height - 2.0f),
                      (int)(inner.width - 8.0f), 2, shadow);
        DrawRectangleRoundedLinesEx(r, 0.18f, 6, 2.0f, BLACK);
    }
    if (style->selected || active_hover) {
        Rectangle outline = style->flat ? r : inner;
        DrawRectangleRoundedLinesEx(outline, 0.18f, 6, 1.0f, WHITE);
    }

    bool has_icon  = style->icon_id && '\0' != style->icon_id[0];
    bool has_label = style->label && '\0' != style->label[0];

    if (style->wrap_label) {
        float icon_sz = r.height - 12.0f;
        if (has_icon) {
            float cx = r.x + 8.0f + icon_sz * 0.5f;
            float cy = r.y + r.height * 0.5f;
            if (!style->flat)
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

    /* Centered group: icon + single line, sized to fit and centered.
     * When there is no label, size the icon to fill the button (minus padding)
     * so that icon-only buttons (toggle arrows, toolbar) get a large icon. */
    float icon_sz;
    if (has_icon && !has_label) {
        float max_sz = r.height < r.width ? r.height : r.width;
        icon_sz = max_sz - 8.0f;
        if (icon_sz < 8.0f) icon_sz = 8.0f;
    } else {
        icon_sz = (float)style->font_size + 6.0f;
        if (icon_sz > r.height - 8.0f) icon_sz = r.height - 8.0f;
    }
    float gap = has_icon && has_label ? 6.0f : 0.0f;
    int   label_w = has_label ? MeasureText(style->label, style->font_size) : 0;
    float group_w = (has_icon ? icon_sz : 0.0f) + gap + (float)label_w;
    float gx = r.x + (r.width - group_w) * 0.5f;
    float cy = r.y + r.height * 0.5f;

    if (has_icon) {
        if (!style->flat)
            ui_icon_draw_ex(style->icon_id, gx + icon_sz * 0.5f + 1.0f, cy + 1.0f, icon_sz, 0.0f, BLACK);
        ui_icon_draw_ex(style->icon_id, gx + icon_sz * 0.5f, cy, icon_sz, 0.0f, text_col);
        gx += icon_sz + gap;
    }
    if (has_label) {
        draw_outlined(style->label, (int)gx, (int)(cy - style->font_size * 0.5f),
                      style->font_size, text_col);
    }
}

/* ── Main draw ───────────────────────────────────────────────────────── */

void ui_button_draw(Rectangle bounds, const UIButtonStyle* style, UIButtonState st) {
    if (!style->no_fill) {
        Color bg = state_bg(style, st);
        if (style->rounded) {
            float r = style->roundness > 0.0f ? style->roundness : 0.18f;
            DrawRectangleRounded(bounds, r, 4, bg);
        } else {
            DrawRectangleRec(bounds, bg);
        }
    }

    Color border = (UI_BUTTON_SELECTED == st)
        ? or_default(style->border_selected, or_default(style->border, UB_BORDER))
        : or_default(style->border, (Color){0});
    if (border.a != 0) {
        float bw = style->border_width > 0.0f ? style->border_width : 1.0f;
        if (UI_BUTTON_SELECTED == st) bw += 0.5f;
        if (style->rounded) {
            float r = style->roundness > 0.0f ? style->roundness : 0.18f;
            DrawRectangleRoundedLinesEx(bounds, r, 4, bw, border);
        } else {
            DrawRectangleLinesEx(bounds, bw, border);
        }
    }

    bool  ic   = has_str(style->icon_id);
    bool  tx   = has_str(style->text);
    int   font = resolve_font(style);
    int   icon_sz = ic ? resolve_icon_size(style, &bounds) : 0;
    int   text_w  = tx ? MeasureText(style->text, font) : 0;
    float gap     = (ic && tx) ? resolve_gap(style) : 0.0f;

    float content_w = (float)icon_sz + gap + (float)text_w;
    float start_x   = bounds.x + (bounds.width - content_w) * 0.5f;
    float cy        = bounds.y + bounds.height * 0.5f;

    if (ic) {
        ui_icon_draw(style->icon_id, start_x + icon_sz * 0.5f, cy, icon_sz, false, 0.0f);
        start_x += icon_sz + gap;
    }
    if (tx) {
        DrawText(style->text, (int)start_x, (int)(cy - font * 0.5f), font,
                 state_text(style, st));
    }
}