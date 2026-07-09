#include "ui_icon.h"

#include "texture_cache.h"

#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ui_icon owns its own texture cache — decorative, presentation-only icons
 * fetched from /assets/ui-icons/{id}.png. It is a peer of the
 * object-layer atlas cache, not a tenant of it: zero dependency on
 * object_layers_management. */

static TextureCache* s_icon_cache = NULL;

static void icon_blob_cb(const FetchResponse* r) {
    if (s_icon_cache) { texture_cache_on_blob_fetched(s_icon_cache, r); }
    else              { free(r->data); }
}

void ui_icon_init(int capacity) {
    if (s_icon_cache) { return; }
    s_icon_cache = texture_cache_create(capacity, "ui_icon", icon_blob_cb);
}

void ui_icon_cleanup(void) {
    texture_cache_destroy(s_icon_cache);
    s_icon_cache = NULL;
}

void ui_icon_draw(const char* icon_id, float cx, float cy, int size, bool bounce, float phase) {
    assert(icon_id);
    assert(strlen(icon_id));
    assert(s_icon_cache);

    float offset_y = 0.0f;
    if (bounce) {
        float t = (float)GetTime();
        float raw = sinf((t * UI_ICON_BOUNCE_FREQ * 2.0f * PI) + phase);
        /* Ease-in-out: cube the sine for a softer reversal at peaks. */
        offset_y = raw * raw * raw * UI_ICON_BOUNCE_AMP;
    }
    float draw_cy = cy + offset_y;

    char url[512];
    snprintf(url, sizeof(url), "/assets/ui-icons/%s.png", icon_id);
    Texture2D tex = texture_cache_get(s_icon_cache, url);

    if (tex.id > 0) {
        Rectangle src = { 0, 0, (float)tex.width, (float)tex.height };
        Rectangle dst = { cx - size * 0.5f, draw_cy - size * 0.5f,
                          (float)size, (float)size };
        DrawTexturePro(tex, src, dst, (Vector2){0, 0}, 0.0f, WHITE);
    } else {
        /* Still loading — subtle pulsing placeholder dot. */
        float pulse = 0.5f + 0.5f * sinf((float)GetTime() * 4.0f);
        unsigned char alpha = (unsigned char)(40 + (int)(pulse * 60.0f));
        float r = size * 0.25f;
        DrawCircle((int)cx, (int)draw_cy, r, (Color){150, 160, 180, alpha});
    }
}

void ui_icon_draw_ex(const char* icon_id, float cx, float cy, float size,
                     float rotation_deg, Color tint) {
    assert(icon_id);
    assert(strlen(icon_id));
    assert(s_icon_cache);

    char url[512];
    snprintf(url, sizeof(url), "/assets/ui-icons/%s.png", icon_id);
    Texture2D tex = texture_cache_get(s_icon_cache, url);
    if (tex.id <= 0) return; /* decorative — nothing while loading */

    Rectangle src = { 0, 0, (float)tex.width, (float)tex.height };
    Rectangle dst = { cx, cy, size, size };
    /* Origin at the icon centre so rotation spins in place. */
    DrawTexturePro(tex, src, dst, (Vector2){ size * 0.5f, size * 0.5f },
                   rotation_deg, tint);
}
