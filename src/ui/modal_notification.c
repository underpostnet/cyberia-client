#include "modal_notification.h"

#include "item_slot.h"
#include "modal.h"
#include "object_layer.h"
#include "object_layers_management.h"

#include <raylib.h>
#include <string.h>

#define MN_W          340
#define MN_H          64
#define MN_TOP        16
#define MN_HOLD       3.4f   /* seconds fully visible before fade-out */
#define MN_FADE       0.4f
#define MN_FONT_TITLE 18
#define MN_FONT_BODY  13
#define MN_SLOT       44

static bool  s_open = false;
static float s_age  = 0.0f;
static char  s_title[96]   = {0};
static char  s_message[160] = {0};
static Color s_accent = { 90, 200, 110, 255 };
static char  s_reward_item[64] = {0};
static int   s_reward_qty = 0;

static const Color C_TITLE = { 230, 235, 245, 255 };
static const Color C_BODY  = { 170, 180, 200, 230 };

void modal_notification_init(void) {
    s_open = false;
}

void modal_notification_show(const char* title, const char* message, Color accent) {
    modal_notification_show_reward(title, message, accent, NULL, 0);
}

void modal_notification_show_reward(const char* title, const char* message, Color accent,
                                    const char* reward_item_id, int reward_quantity) {
    strncpy(s_title, title ? title : "", sizeof(s_title) - 1);
    s_title[sizeof(s_title) - 1] = '\0';
    strncpy(s_message, message ? message : "", sizeof(s_message) - 1);
    s_message[sizeof(s_message) - 1] = '\0';
    strncpy(s_reward_item, reward_item_id ? reward_item_id : "", sizeof(s_reward_item) - 1);
    s_reward_item[sizeof(s_reward_item) - 1] = '\0';
    s_reward_qty = reward_quantity;
    s_accent = accent;
    s_age    = 0.0f;
    s_open   = true;
}

void modal_notification_update(float dt) {
    if (!s_open) return;
    s_age += dt;
    if (s_age >= MODAL_POP_DURATION + MN_HOLD + MN_FADE) s_open = false;
}

void modal_notification_draw(void) {
    if (!s_open) return;

    /* Fade out over the last MN_FADE seconds; pop-in handled by shared alpha. */
    float in    = modal_pop_alpha(s_age);
    float t_out = s_age - (MODAL_POP_DURATION + MN_HOLD);
    float out   = (t_out <= 0.0f) ? 1.0f : 1.0f - (t_out / MN_FADE);
    if (out < 0.0f) out = 0.0f;
    float a = in * out;

    int sw = GetScreenWidth();
    float slide = (1.0f - modal_pop_scale(s_age)) * 12.0f; /* slide down on entry */
    Rectangle card = { (float)((sw - MN_W) / 2), (float)MN_TOP - slide, (float)MN_W, (float)MN_H };

    Color bg = MODAL_PANEL_BG;
    bg.a = (unsigned char)(235 * a);
    DrawRectangleRec(card, bg);
    Color border = MODAL_PANEL_BORDER;
    border.a = (unsigned char)(border.a * a);
    DrawRectangleLinesEx(card, 1.0f, border);

    /* Accent bar on the left. */
    Color accent = s_accent;
    accent.a = (unsigned char)(accent.a * a);
    DrawRectangle((int)card.x, (int)card.y, 4, (int)card.height, accent);

    Color title = C_TITLE; title.a = (unsigned char)(title.a * a);
    Color body  = C_BODY;  body.a  = (unsigned char)(body.a * a);
    DrawText(s_title, (int)(card.x + 14), (int)(card.y + 10), MN_FONT_TITLE, title);
    if (s_message[0] != '\0') {
        DrawText(s_message, (int)(card.x + 14), (int)(card.y + 10 + MN_FONT_TITLE + 6),
                 MN_FONT_BODY, body);
    }

    /* Reward item slot on the right — inspect-at-a-glance of what was granted. */
    if (s_reward_item[0] != '\0') {
        Rectangle slot = { card.x + card.width - MN_SLOT - 10,
                           card.y + (card.height - MN_SLOT) * 0.5f, MN_SLOT, MN_SLOT };
        ObjectLayerState ol = { 0 };
        strncpy(ol.item_id, s_reward_item, sizeof(ol.item_id) - 1);
        ol.active = true;
        ol.quantity = s_reward_qty;
        item_slot_draw(slot, &ol, obj_layers_mgr_get());
    }
}
