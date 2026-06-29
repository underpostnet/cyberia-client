/* This TU defines the compat shims, so it must call the real raylib DrawTextEx /
 * MeasureTextEx — suppress the macro overrides for itself. */
#define CYBERIA_TEXT_NO_OVERRIDE
#include "text.h"

#include "domain/presentation_runtime.h"
#include "network/engine_client.h"
#include "util/log.h"

#include <raylib.h>
#include <stdlib.h>
#include <string.h>

/* Glyph atlas base size — generous so the font stays crisp when scaled up. */
#define TEXT_FONT_BASE_SIZE 64

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

float text_font_factor(void) {
    return s_factor;
}

void text_draw_compat(const char *text, int x, int y, int size, Color color) {
    float fs = (float)size * s_factor;
    if (fs < 1.0f) fs = 1.0f;
    float spacing = (float)((int)fs / 10);
    DrawTextEx(text_active_font(), text, (Vector2){ (float)x, (float)y }, fs, spacing, color);
}

int text_measure_compat(const char *text, int size) {
    float fs = (float)size * s_factor;
    if (fs < 1.0f) fs = 1.0f;
    float spacing = (float)((int)fs / 10);
    return (int)MeasureTextEx(text_active_font(), text, fs, spacing).x;
}
