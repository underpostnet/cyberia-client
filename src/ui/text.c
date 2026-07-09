/* This TU defines the compat shims, so it must call the real raylib DrawTextEx /
 * MeasureTextEx — suppress the macro overrides for itself. */
#define CYBERIA_TEXT_NO_OVERRIDE
#include "text.h"

#include "domain/presentation_runtime.h"
#include "domain/viewport.h"
#include "network/engine_client.h"
#include "util/log.h"

#include <raylib.h>
#include <stdlib.h>
#include <string.h>

/* Glyph atlas base size — generous so the font stays crisp when scaled up. */
#define TEXT_FONT_BASE_SIZE 64

/* Responsive scaling: on a mobile viewport (see domain/viewport.h) the global
 * font multiplier is reduced so HUD/UI text is proportionally smaller on
 * phones. Desktop is unaffected. */
#define TEXT_MOBILE_FONT_SCALE 0.88f

static Font  s_font;
static bool  s_loaded;
static bool  s_fetching;
static char  s_family[128];      /* family currently loaded or already attempted */
static float s_factor = 1.0f;

static void on_font_fetched(const FetchResponse *r) {
    s_fetching = false;
    if (!r->success || NULL == r->data || 0 == r->size) {
        LOG_ERROR("[text] main font fetch failed for '%s'", s_family);
        free(r->data);
        return;
    }
    Font f = LoadFontFromMemory(".ttf", (const unsigned char *)r->data, (int)r->size,
                                TEXT_FONT_BASE_SIZE, NULL, 0);
    free(r->data);
    if (!IsFontValid(f)) {
        LOG_ERROR("[text] LoadFontFromMemory failed for '%s'", s_family);
        return;
    }
    SetTextureFilter(f.texture, TEXTURE_FILTER_BILINEAR);
    if (s_loaded) UnloadFont(s_font);
    s_font = f;
    s_loaded = true;
    LOG_INFO("[text] main font '%s' loaded (%d glyphs)", s_family, f.glyphCount);
}

void text_font_init(void) {
    s_font = GetFontDefault();
    s_loaded = false;
    s_fetching = false;
    s_family[0] = '\0';
    s_factor = 1.0f;
}

void text_font_sync(void) {
    float factor = presentation_runtime_font_factor_size();
    s_factor = factor > 0.0f ? factor : 1.0f;

    const char *family = presentation_runtime_font_family();
    if (NULL == family || '\0' == family[0]) return;
    if (s_fetching) return;
    /* s_family records the family we last loaded or attempted; only (re)fetch when
     * the hint names a different one — this also stops a retry loop on failure. */
    if (0 == strcmp(family, s_family)) return;

    strncpy(s_family, family, sizeof(s_family) - 1);
    s_family[sizeof(s_family) - 1] = '\0';

    char url[256];
    snprintf(url, sizeof(url), "/assets/fonts/%s", s_family);
    s_fetching = true;
    fetch_request_start("cyberia-main-font", url, on_font_fetched);
    LOG_INFO("[text] fetching main font %s", url);
}

void text_font_unload(void) {
    if (s_loaded) {
        UnloadFont(s_font);
        s_loaded = false;
    }
}

Font text_active_font(void) {
    return s_loaded ? s_font : GetFontDefault();
}

/* Effective multiplier: the hint-derived factor times a live responsive
 * reduction below the mobile breakpoint. Evaluated per call (not cached in
 * s_factor) so it tracks viewport width even in loops that never re-sync. */
static float effective_factor(void) {
    float f = s_factor;
    if (viewport_is_mobile()) f *= TEXT_MOBILE_FONT_SCALE;
    return f;
}

float text_font_factor(void) {
    return effective_factor();
}

void text_draw_compat(const char *text, int x, int y, int size, Color color) {
    float fs = (float)size * effective_factor();
    if (fs < 1.0f) fs = 1.0f;
    float spacing = (float)((int)fs / 10);
    DrawTextEx(text_active_font(), text, (Vector2){ (float)x, (float)y }, fs, spacing, color);
}

int text_measure_compat(const char *text, int size) {
    float fs = (float)size * effective_factor();
    if (fs < 1.0f) fs = 1.0f;
    float spacing = (float)((int)fs / 10);
    return (int)MeasureTextEx(text_active_font(), text, fs, spacing).x;
}

#define TEXT_LINE_GAP 3

int text_line_height(int size) {
    int fs = (int)((float)size * effective_factor());
    if (fs < 1) fs = 1;
    return fs + TEXT_LINE_GAP;
}

int text_wrap(const char *text, int x, int y, int maxw, int size, Color col, bool center, bool draw) {
    if (NULL == text || '\0' == text[0]) return 0;
    char buf[512];
    strncpy(buf, text, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';

    int line_h = text_line_height(size);
    int cy = y;
    char line[512] = { 0 };
    char test[512];

    char *tok = strtok(buf, " ");
    while (tok) {
        if ('\0' == line[0]) snprintf(test, sizeof(test), "%s", tok);
        else                 snprintf(test, sizeof(test), "%s %s", line, tok);
        if (text_measure_compat(test, size) > maxw && '\0' != line[0]) {
            if (draw) {
                int lx = center ? x + (maxw - text_measure_compat(line, size)) / 2 : x;
                text_draw_compat(line, lx, cy, size, col);
            }
            cy += line_h;
            snprintf(line, sizeof(line), "%s", tok);
        } else {
            snprintf(line, sizeof(line), "%s", test);
        }
        tok = strtok(NULL, " ");
    }
    if ('\0' != line[0]) {
        if (draw) {
            int lx = center ? x + (maxw - text_measure_compat(line, size)) / 2 : x;
            text_draw_compat(line, lx, cy, size, col);
        }
        cy += line_h;
    }
    return cy - y;
}
