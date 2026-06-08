/**
 * ui_button — shared button renderer.
 *
 * One drawing path for every tap target in the client: toggle switches, zoom
 * buttons, modal tabs, close buttons, direction/mode buttons, scroll arrows,
 * and pager controls. Modeled on the ui_toggle chrome (filled rect + border +
 * centered content), generalized to carry an icon, a text label, or both.
 *
 * A button must have at least an icon or a label. Content is laid out as a
 * centered group (icon, gap, text). Zero-valued style fields fall back to
 * built-in defaults so each call site sets only what differs from the toggle
 * baseline; a color with alpha 0 means "use the default for that slot".
 *
 * `ui_button_measure` returns the content-fit size so callers can grow or
 * shrink a button to its content instead of hardcoding dimensions.
 */

#ifndef UI_BUTTON_H
#define UI_BUTTON_H

#include <raylib.h>
#include <stdbool.h>

#define UI_BUTTON_DEFAULT_FONT  18
#define UI_BUTTON_DEFAULT_PAD   8.0f
#define UI_BUTTON_DEFAULT_GAP   6.0f

typedef enum {
    UI_BUTTON_NORMAL,
    UI_BUTTON_HOVERED,
    UI_BUTTON_SELECTED,
    UI_BUTTON_DISABLED,
} UIButtonState;

typedef struct {
    const char* text;        /* NULL/"" = no label */
    const char* icon_id;     /* NULL/"" = no icon (ui-icons stem) */

    int   font_size;         /* 0 = UI_BUTTON_DEFAULT_FONT */
    int   icon_size;         /* 0 = derived from label / bounds */
    float padding;           /* inner padding per side; 0 = UI_BUTTON_DEFAULT_PAD */
    float gap;               /* icon-to-text gap; 0 = UI_BUTTON_DEFAULT_GAP */

    /* Backgrounds per state. alpha 0 → built-in default for that state. */
    Color bg;
    Color bg_hover;
    Color bg_selected;
    Color bg_disabled;

    /* Border. alpha 0 → no border. width 0 → 1.0; selected uses width+0.5. */
    Color border;
    Color border_selected;   /* alpha 0 → border */
    float border_width;

    /* Text colors. alpha 0 → default; selected/disabled fall back to text. */
    Color text_color;
    Color text_selected;
    Color text_disabled;

    bool  rounded;
    float roundness;         /* used when rounded; 0 → 0.18 */
    bool  no_fill;           /* skip the background fill (bare icon/text target) */
} UIButtonStyle;

/* Content-fit size (icon + gap + text + padding*2). */
Vector2 ui_button_measure(const UIButtonStyle* style);

/* Draw within explicit bounds; content is centered as a group. */
void ui_button_draw(Rectangle bounds, const UIButtonStyle* style, UIButtonState state);

/* Returns the resolved state for a tap target given hover/selected/enabled. */
UIButtonState ui_button_resolve_state(bool enabled, bool selected, bool hovered);

bool ui_button_hit(Rectangle bounds, int mx, int my);

#endif /* UI_BUTTON_H */
