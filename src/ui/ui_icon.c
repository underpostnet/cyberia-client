#include "ui_icon.h"

#include "network/engine_client.h"

#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void ui_icon_draw(const char* icon_id, float cx, float cy, int size, bool bounce, float phase) {
    assert(icon_id);
    assert(strlen(icon_id));

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
    Texture2D tex = fetch_texture(url, 1, FETCH_P1, true);

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

    char url[512];
    snprintf(url, sizeof(url), "/assets/ui-icons/%s.png", icon_id);
    Texture2D tex = fetch_texture(url, 1, FETCH_P1, true);
    if (tex.id <= 0) return; /* decorative — nothing while loading */

    Rectangle src = { 0, 0, (float)tex.width, (float)tex.height };
    Rectangle dst = { cx, cy, size, size };
    /* Origin at the icon centre so rotation spins in place. */
    DrawTexturePro(tex, src, dst, (Vector2){ size * 0.5f, size * 0.5f },
                   rotation_deg, tint);
}
