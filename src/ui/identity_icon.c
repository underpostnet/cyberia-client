#include "identity_icon.h"

#include "content/stat_contract_generated.h"
#include "data/engine_client.h"
#include "modal.h"
#include "text.h"
#include "ui_icon.h"
#include "ui_rect.h"
#include "utils.h"

#include "world/object_layers_management.h"
#include "world/world_types.h"

#include <raylib.h>
#include <stdio.h>
#include <string.h>

#define IDENTITY_ICON_PLAYERS 128
/* The engine draws a preview on 300 px; an icon decodes it at half that. */
#define IDENTITY_ICON_DOWNSCALE 2

static const Color C_FRAME_BG = { 18, 22, 38, 255 };

/* The least recently used player leaves when the table is full. */
static struct {
    char   name[PLAYER_NAME_BYTES];
    char   latest[CYBERIA_IDENTITY_PREVIEW_URL_MAX];
    char   shown[CYBERIA_IDENTITY_PREVIEW_URL_MAX];
    double used;
} s_players[IDENTITY_ICON_PLAYERS];

static int find_player(const char* name) {
    for (int i = 0; IDENTITY_ICON_PLAYERS > i; ++i)
        if ('\0' != s_players[i].name[0] && 0 == strcmp(s_players[i].name, name)) return i;
    return -1;
}

void identity_appearance_ref(const ObjectLayerState* layers, int count, char* out, size_t size) {
    int used = snprintf(out, size, ENGINE_API_BASE CYBERIA_IDENTITY_PREVIEW_ROUTE);
    int added = 0;
    for (int i = 0; count > i; ++i) {
        if (!layers[i].active || '\0' == layers[i].item_id[0]) continue;
        const ObjectLayer* definition = lookup_cached_layer(layers[i].item_id);
        if (NULL == definition) { out[0] = '\0'; return; }
        if ('\0' == definition->data.render.cid[0]) continue;
        int wrote = snprintf(out + used, size - (size_t)used, "%s%s", 0 < added ? "," : "", definition->cid);
        if (CYBERIA_IDENTITY_PREVIEW_MAX_LAYERS == added || 0 > wrote || size <= (size_t)(used + wrote)) {
            out[0] = '\0';
            return;
        }
        used += wrote;
        added++;
    }
    if (0 == added) out[0] = '\0';
}

void identity_icon_note(const char* name, const char* avatar) {
    if (NULL == name || '\0' == name[0] || NULL == avatar || '/' != avatar[0]) return;
    int slot = find_player(name);
    if (0 > slot) {
        slot = 0;
        for (int i = 1; IDENTITY_ICON_PLAYERS > i; ++i)
            if (s_players[slot].used > s_players[i].used) slot = i;
        memset(&s_players[slot], 0, sizeof s_players[slot]);
        copy_str(s_players[slot].name, sizeof s_players[slot].name, name);
    }
    copy_str(s_players[slot].latest, sizeof s_players[slot].latest, avatar);
    s_players[slot].used = GetTime();
}

void identity_icon_draw(const char* name, Rectangle r) {
    DrawRectangleRec(r, C_FRAME_BG);
    int slot = NULL != name && '\0' != name[0] ? find_player(name) : -1;
    Texture2D texture = { 0 };
    if (0 <= slot) {
        s_players[slot].used = GetTime();
        texture = fetch_texture(s_players[slot].latest, IDENTITY_ICON_DOWNSCALE, FETCH_P2, true);
        if (0 < texture.id) copy_str(s_players[slot].shown, sizeof s_players[slot].shown, s_players[slot].latest);
        else if ('\0' != s_players[slot].shown[0])
            texture = fetch_texture(s_players[slot].shown, IDENTITY_ICON_DOWNSCALE, FETCH_P2, true);
    }
    if (0 < texture.id) {
        DrawTexturePro(texture, (Rectangle){ 0, 0, (float)texture.width, (float)texture.height },
                       ui_rect_contain(r, (float)texture.width, (float)texture.height), (Vector2){ 0 }, 0.0f, WHITE);
    } else {
        int size = (int)(r.width < r.height ? r.width : r.height);
        ui_icon_draw("anon", r.x + r.width * 0.5f, r.y + r.height * 0.5f, (int)((float)size * 0.8f), false, 0.0f);
    }
    DrawRectangleLinesEx(r, 1.0f, BLACK);
}

void identity_preview_draw(const char* name, const char* note, Rectangle r, int font, Color color, Color note_color) {
    float face = r.height;
    identity_icon_draw(name, (Rectangle){ r.x, r.y, face, face });
    int x = (int)(r.x + face + 8.0f);
    int width = (int)(r.width - face - 8.0f);
    modal_draw_clipped_text(name, x, (int)r.y, width, font, color);
    if (NULL != note && '\0' != note[0])
        modal_draw_clipped_text(note, x, (int)r.y + text_line_height(font), width, font - 2, note_color);
}
