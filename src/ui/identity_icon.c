#include "identity_icon.h"

#include "ol_stack_ico.h"
#include "ui_icon.h"
#include "utils.h"

#include "data/engine_client.h"
#include "world/object_layer.h"
#include "world/object_layers_management.h"

#include <raylib.h>
#include <stddef.h>

static const Color C_FRAME_BG = { 18, 22, 38, 255 };

void identity_icon_draw(const char* skin_item_id, Rectangle r) {
    DrawRectangleRec(r, C_FRAME_BG);
    int size = (int)(r.width < r.height ? r.width : r.height);
    if (NULL != skin_item_id && '\0' != skin_item_id[0] && 0 < get_atlas_texture(skin_item_id, FETCH_P2).id) {
        ObjectLayerState skin = { .active = true, .quantity = 1, .slot = -1 };
        copy_str(skin.item_id, sizeof skin.item_id, skin_item_id);
        ol_stack_ico_draw(obj_layers_mgr_get(), &skin, 1, (int)r.x, (int)r.y, size, "down_idle", 0, WHITE);
    } else {
        ui_icon_draw("anon", r.x + r.width * 0.5f, r.y + r.height * 0.5f, (int)((float)size * 0.8f), false, 0.0f);
    }
    DrawRectangleLinesEx(r, 1.0f, BLACK);
}
