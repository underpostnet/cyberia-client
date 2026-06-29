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
        DrawRectangleLinesEx(bounds, bw, border);
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
