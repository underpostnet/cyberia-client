#ifndef CYBERIA_UI_TEXT_H
#define CYBERIA_UI_TEXT_H

/* Main UI font, configured through client hints (RENDER_DEFAULTS.fontFamily +
 * fontFactorSize). The TTF is fetched async from /assets/fonts/<fontFamily> and,
 * once loaded, becomes the default font for every DrawText / MeasureText call via
 * the shims below; raylib's built-in font is used until then. fontFactorSize
 * scales every text size uniformly.
 *
 * raylib.h is included first so the DrawText / MeasureText macro overrides are
 * defined only AFTER raylib's own declarations — inclusion order in consumers is
 * therefore irrelevant. */

#include <raylib.h>
#include <stdbool.h>

void  text_font_init(void);    /* after InitWindow: seed defaults (built-in font) */
void  text_font_sync(void);    /* per frame: kick the async fetch once hints name a font */
void  text_font_unload(void);
Font  text_active_font(void);  /* loaded main font, or GetFontDefault() until ready */
float text_font_factor(void);

/* DrawText / MeasureText routed through the active font + size factor, mirroring
 * raylib's built-in spacing (fontSize/10) so measure and draw stay consistent. */
void text_draw_compat(const char *text, int x, int y, int size, Color color);
int  text_measure_compat(const char *text, int size);

/* Line advance for `size`-point text, including the active font factor + a small
 * inter-line gap. Use it to advance y past one drawn line so layouts scale when
 * the font size / family changes. */
int  text_line_height(int size);

/* Word-wrap `text` into `maxw` pixels at `size` (active font + factor applied),
 * left-aligned at x or horizontally centred within [x, x+maxw] when `center`.
 * Renders when `draw` is true, else only measures. Returns the total pixel height
 * consumed — the single source of truth for text-driven dynamic layout height. */
int  text_wrap(const char *text, int x, int y, int maxw, int size, Color col, bool center, bool draw);

/* Variadic so a compound-literal Color argument — `(Color){ r, g, b, a }` — is
 * passed through as raw tokens and parsed by the C compiler, not split on its
 * inner commas by the preprocessor. */
#ifndef CYBERIA_TEXT_NO_OVERRIDE
#define DrawText(...)    text_draw_compat(__VA_ARGS__)
#define MeasureText(...) text_measure_compat(__VA_ARGS__)
#endif

#endif /* CYBERIA_UI_TEXT_H */
