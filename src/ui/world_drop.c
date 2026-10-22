#include "world_drop.h"

#include "content/stat_contract_generated.h"
#include "modal_notification.h"
#include "sim/game_state.h"
#include "sim/local_player.h"
#include "view/game_render.h"
#include "view/local_player_view.h"
#include "world/economy.h"
#include "world/world_types.h"

#include <math.h>
#include <raylib.h>

/* A drag target stays this far inside the reach the server allows. */
#define WD_REACH_MARGIN 0.5f

static const Color C_DROP    = { 236, 236, 0, 255 };
static const Color C_REFUSED = { 210, 90, 80, 255 };

/* The bag or storage stack the quantity picker asks about. */
static struct {
    Box     box;
    int     slot;
    Vector2 cell;
} s_pending;

static Vector2 own_centre(void) {
    Vector2 pos = local_player_view_position();
    Vector2 size = 0 <= game_state_slot(g_local_player.id) ? game_state_transform(g_local_player.id)->size
                                                           : (Vector2){ 1.0f, 1.0f };
    return (Vector2){ pos.x + size.x * 0.5f, pos.y + size.y * 0.5f };
}

Vector2 world_drop_cell(Vector2 screen) {
    Vector2 at = game_render_screen_to_world(screen);
    Vector2 centre = own_centre();
    float dx = at.x - centre.x, dy = at.y - centre.y;
    float reach = CYBERIA_ITEM_DROP_REACH - WD_REACH_MARGIN;
    float d = sqrtf(dx * dx + dy * dy);
    if (d <= reach) return at;
    return (Vector2){ centre.x + dx / d * reach, centre.y + dy / d * reach };
}

bool world_drop_allowed(const char* item_id, const char** reason) {
    *reason = NULL;
    if (local_player_is_coin(item_id)) *reason = "Coins stay in your wallet.";
    else if (!economy_policy(item_id)->world_drop) *reason = "This item cannot be dropped.";
    else if (local_player_trading()) *reason = "Finish or cancel your trade first.";
    else if (STATUS_ICON_DEAD == local_player_status_icon()) *reason = "Not while you wait to respawn.";
    else if (local_player_unsaved()) *reason = "Drops wait until your progress saves again.";
    return NULL == *reason;
}

static void confirmed(const char* item_id, int quantity) {
    local_player_request_item_drop(s_pending.box, s_pending.slot, quantity, s_pending.cell);
}

void world_drop_ask(Box box, int slot, const char* item_id, int quantity, Vector2 cell) {
    const char* reason = NULL;
    if (!world_drop_allowed(item_id, &reason)) {
        modal_notification_show("Drop", reason, C_REFUSED);
        return;
    }
    s_pending.box = box;
    s_pending.slot = slot;
    s_pending.cell = cell;
    modal_notification_show_split("Drop on the ground", "Anyone can take it, you too. It vanishes after a while.",
                                  C_DROP, item_id, quantity, "Drop", "drop", confirmed, NULL);
}

void world_drop_draw_target(Vector2 cell, bool allowed) {
    Vector2 centre = game_render_world_to_screen(cell);
    Vector2 edge = game_render_world_to_screen((Vector2){ cell.x + 1.0f, cell.y });
    float radius = fabsf(edge.x - centre.x) * 0.7f;
    Color color = allowed ? C_DROP : C_REFUSED;
    DrawCircleV(centre, radius, Fade(color, 0.25f));
    DrawCircleLinesV(centre, radius, color);
    DrawCircleLinesV(centre, radius - 1.0f, color);
}
