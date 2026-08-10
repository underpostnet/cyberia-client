#include "modal_anchor.h"

#include "domain/viewport.h"
#include "game_render.h"
#include "game_state.h"
#include "inventory_bar.h"
#include "toolbar.h"
#include "world_types.h"

#include <math.h>
#include <string.h>

bool modal_anchor_active(void) {
    return !viewport_is_mobile() && GetScreenWidth() > MODAL_ANCHOR_MIN_SCREEN_W;
}

Rectangle modal_anchor_safe_area(float pad, float min_h) {
    float sw = (float)GetScreenWidth();
    float sh = (float)GetScreenHeight();
    float top = toolbar_height() + pad;
    float bottom = sh - inventory_bar_visible_height() - pad;
    if (bottom < top + min_h) bottom = top + min_h;
    float width = sw - 2.0f * pad;
    if (width < 1.0f) width = 1.0f;
    return (Rectangle){ pad, top, width, bottom - top };
}

/* Resolve an id across every world-mirror entity array the interact and
 * inventory modals can target: bots, resource nodes, other players, and the
 * local player. */
static const EntityState* find_entity(const char* entity_id) {
    if (NULL == entity_id || '\0' == entity_id[0]) return &g_game_state.player.base;
    if (0 == strcmp(entity_id, g_game_state.player.base.id))
        return &g_game_state.player.base;

    const BotState* bot = game_state_find_bot(entity_id);
    if (bot) return &bot->base;

    const PlayerState* player = game_state_find_player(entity_id);
    if (player) return &player->base;

    for (int i = 0; i < g_game_state.resource_count; i++) {
        if (0 == strcmp(g_game_state.resources[i].base.id, entity_id))
            return &g_game_state.resources[i].base;
    }
    return NULL;
}

/* Screen point to hang a card from: the top-centre of `entity_id`'s bounding
 * box, converted from world space through the gameplay camera. NULL or an empty
 * id resolves to the local player. False when the id names no entity in the
 * world mirror (it left the AOI) or the camera has yet to produce a finite
 * projection — the capture then falls back to a centred placement. */
static bool entity_point(const char* entity_id, Vector2* out_point) {
    if (NULL == out_point) return false;
    const EntityState* entity = find_entity(entity_id);
    if (NULL == entity) return false;

    /* Grid-space top-centre of the bounding box; game_render_world_to_screen
     * applies cell_size and the gameplay Camera2D, the same projection the
     * overhead nameplates and world FX use. */
    Vector2 top_center = {
        entity->interp_pos.x + entity->dims.x * 0.5f,
        entity->interp_pos.y,
    };
    Vector2 screen = game_render_world_to_screen(top_center);
    if (!isfinite(screen.x) || !isfinite(screen.y)) return false;

    *out_point = screen;
    return true;
}

/* Slide `rect` back inside `safe`, shrinking it first if it does not fit.
 * Clamps against the far edge before the near one: with a card as large as the
 * safe area both passes run and the near edge wins, so the result stays inside
 * instead of overshooting. */
static Rectangle fit_into(Rectangle rect, Rectangle safe) {
    if (rect.width  > safe.width)  rect.width  = safe.width;
    if (rect.height > safe.height) rect.height = safe.height;

    float max_x = safe.x + safe.width - rect.width;
    float max_y = safe.y + safe.height - rect.height;
    if (rect.x > max_x)  rect.x = max_x;
    if (rect.x < safe.x) rect.x = safe.x;
    if (rect.y > max_y)  rect.y = max_y;
    if (rect.y < safe.y) rect.y = safe.y;
    return rect;
}

static Rectangle place_above(Vector2 point, Vector2 size, float gap, Rectangle safe) {
    if (size.x > safe.width)  size.x = safe.width;
    if (size.y > safe.height) size.y = safe.height;
    Rectangle rect = { point.x - size.x * 0.5f, point.y - gap - size.y, size.x, size.y };
    return fit_into(rect, safe);
}

static Rectangle place_center(Vector2 size, Rectangle safe) {
    if (size.x > safe.width)  size.x = safe.width;
    if (size.y > safe.height) size.y = safe.height;
    return (Rectangle){
        safe.x + (safe.width  - size.x) * 0.5f,
        safe.y + (safe.height - size.y) * 0.5f,
        size.x,
        size.y,
    };
}

void modal_anchor_capture(ModalAnchor* anchor, const char* entity_id,
                          Vector2 size, float gap, Rectangle safe) {
    if (NULL == anchor) return;

    /* Resolve the corner for a card at least as tall as the growth reserve;
     * the card still draws at its real height, it just keeps room beneath it. */
    Vector2 reserved = { size.x, size.y < MODAL_ANCHOR_GROWTH_RESERVE
                                 ? MODAL_ANCHOR_GROWTH_RESERVE : size.y };
    Vector2 point;
    Rectangle rect = entity_point(entity_id, &point)
                   ? place_above(point, reserved, gap, safe)
                   : place_center(reserved, safe);
    anchor->top_left = (Vector2){ rect.x, rect.y };
    anchor->captured = true;
}

Rectangle modal_anchor_rect(const ModalAnchor* anchor, Vector2 size, Rectangle safe) {
    if (NULL == anchor || !anchor->captured) return place_center(size, safe);
    return fit_into((Rectangle){ anchor->top_left.x, anchor->top_left.y,
                                 size.x, size.y }, safe);
}

/* Exponential rate the card height chases its content target at. */
#define MODAL_ANCHOR_RESIZE_LAMBDA 16.0f

float modal_anchor_ease_height(float current, float target, float dt) {
    if (current < 0.0f) return target;
    return current + (target - current) * (1.0f - expf(-MODAL_ANCHOR_RESIZE_LAMBDA * dt));
}
